/* In-process implementation of jfaudiolib's slot-zero driver ABI.
 * The upstream no-sound implementation is not linked into this build. */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "adlmidi.h"
#include "midifuncs.h"
#include "driver_nosound.h"

static char *ring;
static unsigned block_size, blocks, block_index, used;
static void (*refill)(void), (*service)(void);
static struct ADL_MIDIPlayer *synth;
static uint64_t midi_phase, midi_rate;
static unsigned midi_ticks;
static int music_volume = 255;

int NoSoundDrv_GetError(void) { return 0; }
const char *NoSoundDrv_ErrorString(int code) { return "Libretro PCM/MIDI"; }
int NoSoundDrv_PCM_Init(int *rate, int *channels, int *bits, void *data)
{
    NoSoundDrv_PCM_StopPlayback();
    *rate = 48000; *channels = 2; *bits = 16;
    return 0;
}
void NoSoundDrv_PCM_Shutdown(void) { NoSoundDrv_PCM_StopPlayback(); }
int NoSoundDrv_PCM_BeginPlayback(char *buffer, int size, int count, void (*callback)(void))
{
    if (!buffer || size <= 0 || count <= 0 || !callback) return -1;
    ring = buffer; block_size = size; blocks = count;
    block_index = used = 0; refill = callback;
    refill();
    return 0;
}
void NoSoundDrv_PCM_StopPlayback(void) { ring = NULL; refill = NULL; }
void NoSoundDrv_PCM_Lock(void) {}
void NoSoundDrv_PCM_Unlock(void) {}
int NoSoundDrv_CD_Init(void) { return -1; }
void NoSoundDrv_CD_Shutdown(void) {}
int NoSoundDrv_CD_Play(int track, int loop) { return -1; }
void NoSoundDrv_CD_Stop(void) {}
void NoSoundDrv_CD_Pause(int pause) {}
int NoSoundDrv_CD_IsPlaying(void) { return 0; }
void NoSoundDrv_CD_SetVolume(int volume) {}

static void note_off(int c, int k, int v) { adl_rt_noteOff(synth, c, k); }
static void note_on(int c, int k, int v) { adl_rt_noteOn(synth, c, k, v); }
static void poly_touch(int c, int k, int p) { adl_rt_noteAfterTouch(synth, c, k, p); }
static void control(int c, int n, int v) { adl_rt_controllerChange(synth, c, n, v); }
static void program(int c, int p) { adl_rt_patchChange(synth, c, p); }
static void channel_touch(int c, int p) { adl_rt_channelAfterTouch(synth, c, p); }
static void bend(int c, int lo, int hi) { adl_rt_pitchBend(synth, c, (hi << 7) | lo); }
static void volume(int v) { music_volume = v < 0 ? 0 : v > 255 ? 255 : v; }
static int get_volume(void) { return music_volume; }
static void sysex(const unsigned char *data, int length) { adl_rt_systemExclusive(synth, data, length); }
int NoSoundDrv_MIDI_Init(midifuncs *functions, const char *params)
{
    NoSoundDrv_MIDI_Shutdown();
    synth = adl_init(48000);
    if (!synth) return -1;
    adl_switchEmulator(synth, ADLMIDI_EMU_DOSBOX);
    adl_setNumChips(synth, 1);
    adl_setBank(synth, 0);
    *functions = (midifuncs){note_off, note_on, poly_touch, control, program,
                           channel_touch, bend, NULL, NULL, volume, get_volume, sysex};
    music_volume = 255;
    return 0;
}
void NoSoundDrv_MIDI_Shutdown(void)
{
    NoSoundDrv_MIDI_HaltPlayback();
    if (synth) adl_close(synth);
    synth = NULL;
}
int NoSoundDrv_MIDI_StartPlayback(void (*callback)(void))
{
    service = callback; midi_phase = midi_ticks = 0;
    return 0;
}
void NoSoundDrv_MIDI_HaltPlayback(void)
{
    service = NULL; midi_phase = 0;
    if (synth) adl_panic(synth);
}
unsigned NoSoundDrv_MIDI_GetTick(void) { return midi_ticks; }
void NoSoundDrv_MIDI_SetTempo(int tempo, int division)
{
    midi_rate = tempo > 0 && division > 0 ? (uint64_t)tempo * division : 0;
}
void NoSoundDrv_MIDI_Lock(void) {}
void NoSoundDrv_MIDI_Unlock(void) {}

void Duke_Audio(int16_t *output, unsigned frames)
{
    memset(output, 0, frames * 4);
    unsigned remaining = frames * 4;
    char *dest = (char *)output;
    while (ring && remaining) {
        if (used == block_size) {
            refill(); used = 0; block_index = (block_index + 1) % blocks;
        }
        unsigned count = block_size - used;
        if (count > remaining) count = remaining;
        memcpy(dest, ring + block_index * block_size + used, count);
        used += count; remaining -= count; dest += count;
    }
    /* Small synthesis chunks keep MIDI event timing within 0.67 ms. */
    for (unsigned offset = 0; offset < frames; ) {
        unsigned count = frames - offset;
        if (count > 32) count = 32;
        if (service) {
            midi_phase += count * midi_rate;
            while (service && midi_phase >= UINT64_C(48000) * 60) {
                midi_phase -= UINT64_C(48000) * 60;
                midi_ticks++; service();
            }
        }
        if (synth) {
            int16_t music[64] = {0};
            adl_generate(synth, count * 2, music);
            for (unsigned i = 0; i < count * 2; ++i) {
                int mixed = output[offset * 2 + i] + music[i] * music_volume / 510;
                output[offset * 2 + i] = mixed < -32768 ? -32768 : mixed > 32767 ? 32767 : mixed;
            }
        }
        offset += count;
    }
}
