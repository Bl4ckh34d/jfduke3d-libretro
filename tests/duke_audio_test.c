#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "midifuncs.h"
#include "driver_nosound.h"

void Duke_Audio(int16_t *out, unsigned frames);
static int callbacks, ticks;
static void refill(void) { callbacks++; }
static void tick(void) { ticks++; }

int main(void)
{
    int rate = 22050, channels = 1, bits = 8;
    int16_t ring[16], out[40];
    for (int i = 0; i < 16; ++i) ring[i] = i * 100;
    assert(NoSoundDrv_PCM_Init(&rate, &channels, &bits, NULL) == 0);
    assert(rate == 48000 && channels == 2 && bits == 16);
    assert(NoSoundDrv_PCM_BeginPlayback((char *)ring, 16, 2, refill) == 0);
    Duke_Audio(out, 10);
    assert(callbacks == 3);
    for (int i = 0; i < 20; ++i) assert(out[i] == ring[i % 16]);
    NoSoundDrv_PCM_StopPlayback();
    memset(out, 1, sizeof(out));
    Duke_Audio(out, 20);
    for (int i = 0; i < 40; ++i) assert(out[i] == 0);
    midifuncs funcs;
    assert(NoSoundDrv_MIDI_Init(&funcs, NULL) == 0);
    NoSoundDrv_MIDI_SetTempo(120, 96);
    NoSoundDrv_MIDI_StartPlayback(tick);
    for (int i = 0; i < 1200; ++i) Duke_Audio(out, 20);
    assert(ticks == 96);
    funcs.NoteOn(0, 60, 100);
    unsigned nonzero = 0;
    for (int i = 0; i < 100; ++i) {
        Duke_Audio(out, 20);
        for (int j = 0; j < 40; ++j) nonzero += out[j] != 0;
    }
    assert(nonzero > 500);
    NoSoundDrv_MIDI_HaltPlayback();
    int before = ticks;
    NoSoundDrv_MIDI_Shutdown();
    Duke_Audio(out, 20);
    assert(ticks == before);
    for (int i = 0; i < 40; ++i) assert(out[i] == 0);
    NoSoundDrv_PCM_Shutdown();
    printf("PASS: PCM ring, stop, MIDI clock, synth and shutdown (%u nonzero samples)\n", nonzero);
}
