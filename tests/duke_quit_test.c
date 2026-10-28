/* Run the real ARM core through its game quit path, then reload it. */
#include "libretro.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static struct retro_keyboard_callback keyboard;
static unsigned frame, shutdowns, pictures;
static void log_message(enum retro_log_level level, const char *fmt, ...)
{
    if (!strstr(fmt, "exit")) return;
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}
static bool environment(unsigned command, void *data)
{
    switch (command) {
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            ((struct retro_log_callback *)data)->log = log_message; return true;
        case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
            keyboard = *(struct retro_keyboard_callback *)data; return true;
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *(const char **)data = "/data/local/tmp/duke-quit-saves"; return true;
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: return true;
        case RETRO_ENVIRONMENT_SHUTDOWN: shutdowns++; return true;
        default: return false;
    }
}
static void video(const void *data, unsigned w, unsigned h, size_t pitch)
{
    assert(w == 640 && h == 400 && pitch == w * 4);
    if (data) pictures++;
}
static size_t audio(const int16_t *data, size_t frames) { return frames; }
static void poll(void) {}
static int16_t input(unsigned port, unsigned device, unsigned index, unsigned id)
{
    if (device != RETRO_DEVICE_JOYPAD) return 0;
    if (id == RETRO_DEVICE_ID_JOYPAD_B)
        return (frame >= 600 && frame < 606) || (frame >= 660 && frame < 666) ||
               (frame >= 720 && frame < 726) || (frame >= 780 && frame < 786) ||
               (frame >= 840 && frame < 846) || (frame >= 900 && frame < 906) ||
               (frame >= 1260 && frame < 1266);
    return 0;
}
#define LOAD(name) __typeof__(&name) p_##name = dlsym(library, #name); assert(p_##name)
int main(int argc, char **argv)
{
    assert(argc == 3);
    mkdir("/data/local/tmp/duke-quit-saves", 0700);
    void *library = dlopen(argv[1], RTLD_NOW); if (!library) { puts(dlerror()); return 2; }
    LOAD(retro_set_environment); LOAD(retro_set_video_refresh);
    LOAD(retro_set_audio_sample_batch); LOAD(retro_set_input_poll); LOAD(retro_set_input_state);
    LOAD(retro_init); LOAD(retro_deinit); LOAD(retro_load_game); LOAD(retro_unload_game); LOAD(retro_run);
    p_retro_set_environment(environment); p_retro_set_video_refresh(video);
    p_retro_set_audio_sample_batch(audio); p_retro_set_input_poll(poll); p_retro_set_input_state(input);
    p_retro_init();
    struct retro_game_info game = {argv[2], NULL, 0, NULL};
    assert(p_retro_load_game(&game)); p_retro_unload_game();
    assert(shutdowns == 0); /* Frontend-initiated unload must not request shutdown. */
    for (unsigned iteration = 0; iteration < 2; iteration++) {
        shutdowns = pictures = 0;
        assert(p_retro_load_game(&game)); assert(keyboard.callback);
        for (frame = 0; frame < 1500 && !shutdowns; frame++) {
            if (frame == 1200) keyboard.callback(true, RETROK_F10, 0, 0);
            if (frame == 1210) keyboard.callback(false, RETROK_F10, 0, 0);
            p_retro_run();
        }
        printf("load %u: frames=%u pictures=%u shutdowns=%u\n", iteration, frame, pictures, shutdowns);
        fflush(stdout);
        if (shutdowns != 1) { p_retro_unload_game(); p_retro_deinit(); return 1; }
        for (unsigned extra = 0; extra < 3; extra++) p_retro_run();
        assert(shutdowns == 1);
        p_retro_unload_game(); assert(shutdowns == 1);
    }
    p_retro_deinit(); dlclose(library);
    puts("PASS: game quit requests shutdown once; frontend unload and two reloads are safe");
    return 0;
}
