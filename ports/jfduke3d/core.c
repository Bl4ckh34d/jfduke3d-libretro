/* Native libretro platform for JFDuke3D. Game/Build code retains its licenses. */
#include "build.h"
#include "baselayer.h"
#include "baselayer_priv.h"
#include "osd.h"
#include "cache1d.h"
#include "a.h"
#include "libretro.h"
#include "libco.h"
#include <setjmp.h>
#include <stdbool.h>

static retro_environment_t environment;
static retro_video_refresh_t video;
static retro_audio_sample_batch_t audio;
static retro_input_poll_t poll;
static retro_input_state_t input;
static retro_log_printf_t logger;
static cothread_t engine, frontend;
static jmp_buf exit_point;
static bool stopped, stopping, ready, shutdown_requested;
static uint64_t frames, last_event_frame, last_timer_tick;
static unsigned rendered_frames;
static int timer_rate;
static void (*timer_callback)(void);
static uint8_t framebuffer[640 * 480];
static uint32_t pixels[640 * 480], colours[256];
static unsigned output_width = 640, output_height = 400;
static char content[2048], data_directory[2048], save_directory[2048];
static bool key_state[256];
static bool keyboard_down[256], gamepad_down[256];
int displaycnt = 1;
extern void Shutdown(void);
extern int Duke_InGame(void);
extern void Duke_ClearSearchPaths(void);
extern void OSD_Cleanup(void);
extern void Duke_Audio(int16_t *out, unsigned frames);

static void yield_frame(void)
{
    if (stopping) longjmp(exit_point, 1);
    co_switch(frontend);
    if (stopping) longjmp(exit_point, 1);
}
void Duke_Ready(int value) { ready = value != 0; }
void Duke_Exit(int code)
{
    if (logger) logger(RETRO_LOG_INFO, "JFDuke3D exit %d\n", code);
    longjmp(exit_point, 1);
}
const char *Duke_SaveDirectory(void) { return save_directory; }
FILE *Duke_fopen(const char *name, const char *mode)
{
    char path[4096];
    if (*name == '/') return fopen(name, mode);
    if (strstr(name, "..")) return NULL;
    if (snprintf(path, sizeof(path), "%s/%s", save_directory, name) >= sizeof(path)) return NULL;
    FILE *file = fopen(path, mode);
    if (!file && mode[0] == 'r' && !strchr(mode, '+')) {
        if (snprintf(path, sizeof(path), "%s/%s", data_directory, name) < sizeof(path)) file = fopen(path, mode);
    }
    return file;
}

static void run_engine(void)
{
    if (setjmp(exit_point) == 0 && !stopping) {
        char group_arg[2050], path_arg[2050];
        snprintf(group_arg, sizeof(group_arg), "-g%s", content);
        snprintf(path_arg, sizeof(path_arg), "-j%s", data_directory);
        const char *args[] = {"duke3d", group_arg, path_arg, "-nosetup"};
        _buildargc = 4; _buildargv = args;
        baselayer_init();
        app_main(_buildargc, args);
    }
    stopping = false;
    if (ready) { ready = false; Shutdown(); }
    uninitgroupfile();
    uninitsystem();
    Duke_ClearSearchPaths();
    OSD_Cleanup();
    stopped = true;
    for (;;) co_switch(frontend);
}

void initputs(const char *text) { if (logger) logger(RETRO_LOG_INFO, "%s", text); }
void debugprintf(const char *fmt, ...)
{
    char text[2048]; va_list args; va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args); va_end(args); initputs(text);
}
int startwin_open(void) { return 0; }
int startwin_close(void) { return 0; }
int startwin_puts(const char *text) { return 0; }
int startwin_settitle(const char *text) { return 0; }
int startwin_idle(void *data) { return 0; }
int startwin_run(struct startwin_settings *data) { return STARTWIN_RUN; }
int initsystem(void)
{
    /* uninitengine frees projection tables. Recreate them even at the same mode. */
    videomodereset = 1;
    return 0;
}
void uninitsystem(void) { uninitinput(); uninittimer(); }
int wm_msgbox(const char *title, const char *fmt, ...)
{
    char text[2048]; va_list args; va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args); va_end(args);
    if (logger) logger(RETRO_LOG_ERROR, "%s: %s\n", title, text);
    return 0;
}
int wm_ynbox(const char *title, const char *fmt, ...) { return 0; }
int wm_filechooser(const char *dir, const char *file, const char *type, int open, char **choice) { return -1; }
int wm_idle(void *data) { yield_frame(); return 0; }
void wm_allowbackgroundidle(int on) {}
void wm_allowtaskswitching(int on) {}
void wm_setapptitle(const char *name) {}
void wm_setwindowtitle(const char *name) {}

int initinput(void) { inputdevices = 3; return 0; }
void releaseallbuttons(void) { memset(keystatus, 0, sizeof(keystatus)); mouseb = 0; }
void uninitinput(void) { releaseallbuttons(); }
const char *getkeyname(int key) { return NULL; }
const char *getjoyname(int what, int number) { return NULL; }
int initmouse(void) { return 0; }
void uninitmouse(void) {}
void grabmouse(int active) {}
void readmousexy(int *x, int *y) { *x = mousex; *y = mousey; mousex = mousey = 0; }
void readmousebstatus(int *buttons) { *buttons = mouseb; }

int inittimer(int rate, void (*callback)(void))
{
    timer_rate = rate; timer_callback = callback; last_timer_tick = frames * rate / 60;
    return 0;
}
void uninittimer(void) { timer_rate = 0; timer_callback = NULL; }
void sampletimer(void)
{
    uint64_t current = frames * timer_rate / 60;
    while (last_timer_tick < current) {
        last_timer_tick++; totalclock++;
        if (timer_callback) timer_callback();
    }
}
unsigned getticks(void) { return frames * 1000 / 60; }
unsigned getusecticks(void) { return frames * 1000000 / 60; }
int gettimerfreq(void) { return timer_rate; }
int handleevents(void)
{
    if (last_event_frame == frames) yield_frame();
    last_event_frame = frames;
    sampletimer();
    return 0;
}

void getvalidmodes(void)
{
    if (validmodecnt) return;
    for (int fs = 0; fs < 2; ++fs) {
        addvalidmode(320, 200, 8, fs, 0, 60, 0);
        addvalidmode(640, 400, 8, fs, 0, 60, 0);
        addvalidmode(640, 480, 8, fs, 0, 60, 0);
    }
    sortvalidmodes();
}
const char *getdisplayname(int display) { return "Libretro"; }
int setvideomode(int width, int height, int depth, int fs)
{
    if (depth != 8 || width < 320 || width > 640 || height < 200 || height > 480) return -1;
    if (baselayer_videomodewillchange) baselayer_videomodewillchange();
    xres = width; yres = height; bpp = depth; fullscreen = fs;
    frameplace = (intptr_t)framebuffer; bytesperline = width; imageSize = width * height; numpages = 1;
    setvlinebpl(bytesperline);
    for (int y = 0; y <= height; ++y) ylookup[y] = y * bytesperline;
    videomodereset = 0;
    if (baselayer_videomodedidchange) baselayer_videomodedidchange();
    OSD_ResizeDisplay(width, height);
    setpalettefade(palfadergb.r, palfadergb.g, palfadergb.b, palfadedelta);
    return 0;
}
void showframe(void)
{
    rendered_frames++;
    if (xres > 0 && yres > 0) {
        output_width = xres; output_height = yres;
        for (int i = 0; i < xres * yres; ++i) pixels[i] = colours[framebuffer[i]];
    }
    yield_frame();
}
int setpalette(int start, int count, unsigned char *palette)
{
    for (int i = start; i < start + count && i < 256; ++i) {
        colours[i] = (curpalettefaded[i].r << 16) | (curpalettefaded[i].g << 8) | curpalettefaded[i].b;
    }
    return 0;
}
int setsysgamma(float shadergamma, float sysgamma) { return -1; }

static void send_key(int scan, bool down, unsigned character)
{
    if (key_state[scan] == down) return;
    key_state[scan] = down; keystatus[scan] = down;
    keyfifo[keyfifoend] = scan;
    keyfifo[(keyfifoend + 1) & (KEYFIFOSIZ - 1)] = down;
    keyfifoend = (keyfifoend + 2) & (KEYFIFOSIZ - 1);
    if (down && character) {
        keyasciififo[keyasciififoend] = character;
        keyasciififoend = (keyasciififoend + 1) & (KEYFIFOSIZ - 1);
    }
}
static void key(int scan, bool down, unsigned character)
{
    gamepad_down[scan] = down;
    send_key(scan, down || keyboard_down[scan], character);
}
static void keyboard_event(bool down, unsigned code, uint32_t character, uint16_t modifiers)
{
    static const uint8_t letters[26] = {0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,
                                       0x31,0x18,0x19,0x10,0x13,0x1f,0x14,0x16,0x2f,0x11,0x2d,0x15,0x2c};
    unsigned scan = 0;
    if (code >= RETROK_a && code <= RETROK_z) scan = letters[code - RETROK_a];
    else if (code >= RETROK_1 && code <= RETROK_9) scan = 2 + code - RETROK_1;
    else if (code >= RETROK_F1 && code <= RETROK_F10) scan = 0x3b + code - RETROK_F1;
    else switch (code) {
        case RETROK_0: scan=0x0b; break; case RETROK_RETURN: scan=0x1c; break;
        case RETROK_ESCAPE: scan=1; break; case RETROK_SPACE: scan=0x39; break;
        case RETROK_BACKSPACE: scan=0x0e; break; case RETROK_TAB: scan=0x0f; break;
        case RETROK_UP: scan=0xc8; break; case RETROK_DOWN: scan=0xd0; break;
        case RETROK_LEFT: scan=0xcb; break; case RETROK_RIGHT: scan=0xcd; break;
        case RETROK_LSHIFT: scan=0x2a; break; case RETROK_RSHIFT: scan=0x36; break;
        case RETROK_LCTRL: scan=0x1d; break; case RETROK_LALT: scan=0x38; break;
        case RETROK_NUMLOCK: scan=0x45; break; case RETROK_BACKQUOTE: scan=0x29; break;
        case RETROK_MINUS: scan=0x0c; break; case RETROK_EQUALS: scan=0x0d; break;
        case RETROK_LEFTBRACKET: scan=0x1a; break; case RETROK_RIGHTBRACKET: scan=0x1b; break;
        case RETROK_COMMA: scan=0x33; break; case RETROK_PERIOD: scan=0x34; break;
        default: break;
    }
    if (!ready || !scan) return;
    if (scan == OSD_CaptureKey(-1)) { if (down) OSD_ShowDisplay(-1); return; }
    if (!OSD_HandleKey(scan, down)) {
        if (down && character) OSD_HandleChar(character);
        return;
    }
    keyboard_down[scan] = down;
    send_key(scan, down || gamepad_down[scan], character);
}
static bool button(unsigned id) { return input(0, RETRO_DEVICE_JOYPAD, 0, id) != 0; }
static void update_input(void)
{
    int lx = input(0, RETRO_DEVICE_ANALOG, 0, 0), ly = input(0, RETRO_DEVICE_ANALOG, 0, 1);
    int rx = input(0, RETRO_DEVICE_ANALOG, 1, 0), ry = input(0, RETRO_DEVICE_ANALOG, 1, 1);
    bool playing = ready && Duke_InGame();
    key(0x11, ly < -8000 || (playing && button(4)), 0); key(0x1f, ly > 8000 || (playing && button(5)), 0);
    key(0x1e, lx < -8000, 0); key(0x20, lx > 8000, 0);
    key(0xc8, button(4), 0); key(0xd0, button(5), 0);
    key(0xcb, button(6), 0); key(0xcd, button(7), 0);
    key(0x01, button(3) || (!playing && button(8)), 27);
    key(0x1c, playing ? button(1) : button(0), 13);
    /* Original yes/no dialogs accept Y; do not emulate Fire across a load. */
    key(0x15, !playing && button(0), 0);
    key(0x39, playing && button(0), 32);
    key(0x12, button(9), 'e');
    key(0x38, playing && button(8), 0);
    key(0x29, button(12), 0);
    key(0x0f, button(2), 9);
    key(0x33, button(10), ','); key(0x34, button(11), '.');
    key(0x1a, button(14), 0); key(0x1b, button(15), 0);
    if (playing) mousex += 3 * (button(7) - button(6));
    mousex += abs(rx) > 6000 ? rx / 1600 : 0;
    mousey += abs(ry) > 6000 ? ry / 2000 : 0;
    mousex += input(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
    mousey += input(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
    mouseb = (button(13) || input(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT)) ? 1 : 0;
}

void retro_set_environment(retro_environment_t cb) { environment = cb; }
void retro_set_video_refresh(retro_video_refresh_t cb) { video = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) {}
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll = cb; }
void retro_set_input_state(retro_input_state_t cb) { input = cb; }
void retro_init(void)
{
    struct retro_log_callback log;
    if (environment(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) logger = log.log;
    struct retro_keyboard_callback keyboard = {keyboard_event};
    environment(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &keyboard);
    static const struct retro_input_descriptor descriptors[] = {
        {0, RETRO_DEVICE_JOYPAD, 0, 0, "Jump / Confirm"},
        {0, RETRO_DEVICE_JOYPAD, 0, 1, "Use inventory"},
        {0, RETRO_DEVICE_JOYPAD, 0, 2, "Map"},
        {0, RETRO_DEVICE_JOYPAD, 0, 3, "Game menu"},
        {0, RETRO_DEVICE_JOYPAD, 0, 4, "Forward / Menu up"},
        {0, RETRO_DEVICE_JOYPAD, 0, 5, "Backward / Menu down"},
        {0, RETRO_DEVICE_JOYPAD, 0, 6, "Turn left / Menu left"},
        {0, RETRO_DEVICE_JOYPAD, 0, 7, "Turn right / Menu right"},
        {0, RETRO_DEVICE_JOYPAD, 0, 8, "Crouch / Cancel"},
        {0, RETRO_DEVICE_JOYPAD, 0, 9, "Open / Use"},
        {0, RETRO_DEVICE_JOYPAD, 0, 10, "Previous weapon"},
        {0, RETRO_DEVICE_JOYPAD, 0, 11, "Next weapon"},
        {0, RETRO_DEVICE_JOYPAD, 0, 12, "Quick kick"},
        {0, RETRO_DEVICE_JOYPAD, 0, 13, "Fire"},
        {0, RETRO_DEVICE_JOYPAD, 0, 14, "Previous inventory item"},
        {0, RETRO_DEVICE_JOYPAD, 0, 15, "Next inventory item"},
        {0, RETRO_DEVICE_ANALOG, 0, 0, "Strafe"},
        {0, RETRO_DEVICE_ANALOG, 0, 1, "Move"},
        {0, RETRO_DEVICE_ANALOG, 1, 0, "Turn"},
        {0, RETRO_DEVICE_ANALOG, 1, 1, "Look"},
        {0}
    };
    environment(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void *)descriptors);
}
void retro_deinit(void) { retro_unload_game(); }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_get_system_info(struct retro_system_info *info)
{
    *info = (struct retro_system_info){"JFDuke3D TV", "55c5f95-native", "grp", true, false};
}
void retro_get_system_av_info(struct retro_system_av_info *info)
{
    *info = (struct retro_system_av_info){{640, 400, 640, 480, 4.0f / 3.0f}, {60.0, 48000.0}};
}
bool retro_load_game(const struct retro_game_info *game)
{
    const char *save = NULL;
    enum retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!game || !game->path || strlen(game->path) >= sizeof(content)) return false;
    if (!environment(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save) || !save || strlen(save) >= sizeof(save_directory)) return false;
    if (!environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format)) return false;
    strcpy(content, game->path); strcpy(data_directory, content); strcpy(save_directory, save);
    char *slash = strrchr(data_directory, '/'); if (!slash) return false; *slash = 0;
    mkdir(save_directory, 0700);
    stopped = stopping = ready = shutdown_requested = false;
    frames = rendered_frames = 0; last_event_frame = UINT64_MAX; totalclock = 0;
    memset(key_state, 0, sizeof(key_state)); releaseallbuttons();
    memset(keyboard_down, 0, sizeof(keyboard_down)); memset(gamepad_down, 0, sizeof(gamepad_down));
    memset(pixels, 0, sizeof(pixels));
    frontend = co_active(); engine = co_create(4 * 1024 * 1024, run_engine);
    return engine != NULL;
}
void retro_unload_game(void)
{
    if (!engine) return;
    stopping = true;
    while (!stopped) co_switch(engine);
    co_delete(engine); engine = NULL;
}
void retro_run(void)
{
    int16_t samples[1600];
    if (!engine) return;
    if (stopped) return;
    frames++; poll(); update_input();
    co_switch(engine);
    /* A game-menu Quit finishes the coroutine. Notify the frontend from its
     * own context, after engine cleanup, instead of repeating the final frame.
     * Frontend-initiated unload does not pass through this path. */
    if (stopped) {
        if (!shutdown_requested) {
            shutdown_requested = true;
            environment(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
        }
        return;
    }
    Duke_Audio(samples, 800);
    video(pixels, output_width, output_height, output_width * 4);
    if (audio) audio(samples, 800);
    if (logger && frames % 600 == 0) {
        logger(RETRO_LOG_DEBUG, "Duke: %u rendered frames per 600 frontend frames, game clock %d\n", rendered_frames, totalclock);
        rendered_frames = 0;
    }
}
void retro_reset(void) {}
void retro_set_controller_port_device(unsigned port, unsigned device) {}
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t count) { return false; }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { return false; }
bool retro_unserialize(const void *data, size_t size) { return false; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {}
void *retro_get_memory_data(unsigned id) { return NULL; }
size_t retro_get_memory_size(unsigned id) { return 0; }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
