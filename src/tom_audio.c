#include "tom.h"

#include <math.h>
#include <string.h>

#include <SDL.h>

#define RATE 44100

/* The click is the line jump. The horn is only a clean HLT.
   The horn buffer starts with a short silence so the click is heard
   on its own, then the horn arrives — the same order as the seminar. */
static float click_buf[2048];
static int click_len;
static float horn_buf[40000];
static int horn_len;

static int click_on;
static int click_pos;
static int horn_on;
static int horn_pos;

static SDL_AudioDeviceID device_id;
static int device_ready;

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    Sint16 *out = (Sint16 *)stream;
    int frames = len / (int)sizeof(Sint16);
    int i;

    (void)userdata;
    memset(stream, 0, (size_t)len);
    for (i = 0; i < frames; i++) {
        float sample = 0.f;
        if (click_on) {
            if (click_pos < click_len)
                sample += click_buf[click_pos++];
            if (click_pos >= click_len)
                click_on = 0;
        }
        if (horn_on) {
            if (horn_pos < horn_len)
                sample += horn_buf[horn_pos++];
            if (horn_pos >= horn_len)
                horn_on = 0;
        }
        if (sample > 1.f)
            sample = 1.f;
        if (sample < -1.f)
            sample = -1.f;
        out[i] = (Sint16)(sample * 28000.f);
    }
}

static void fill_click(void)
{
    int i;
    const float pi = 3.14159265f;
    click_len = (int)(0.018f * RATE);
    if (click_len > (int)(sizeof click_buf / sizeof click_buf[0]))
        click_len = (int)(sizeof click_buf / sizeof click_buf[0]);
    for (i = 0; i < click_len; i++) {
        float t = (float)i / (float)RATE;
        float env = expf(-t * 260.f);
        float tick = sinf(2.f * pi * 1700.f * t);
        float thump = sinf(2.f * pi * 240.f * t) * env;
        click_buf[i] = (tick * 0.55f + thump * 0.45f) * env * 0.7f;
    }
}

static void fill_horn(void)
{
    int i;
    const float pi = 3.14159265f;
    int lead = (int)(0.04f * RATE);
    int body;

    /* Two pitches a major third apart, with a little second harmonic.
       That is the sound of a small electric car horn, not a beep. */
    body = (int)(0.55f * RATE);
    horn_len = lead + body;
    if (horn_len > (int)(sizeof horn_buf / sizeof horn_buf[0]))
        horn_len = (int)(sizeof horn_buf / sizeof horn_buf[0]);
    for (i = 0; i < horn_len; i++) {
        float t;
        float attack;
        float release;
        float a;
        float b;
        float sample;
        if (i < lead) {
            horn_buf[i] = 0.f;
            continue;
        }
        t = (float)(i - lead) / (float)RATE;
        attack = t < 0.012f ? t / 0.012f : 1.f;
        release = 1.f;
        if (t > 0.47f)
            release = (0.55f - t) / 0.08f;
        if (release < 0.f)
            release = 0.f;
        a = sinf(2.f * pi * 410.f * t) + 0.22f * sinf(2.f * pi * 820.f * t);
        b = sinf(2.f * pi * 520.f * t) + 0.18f * sinf(2.f * pi * 1040.f * t);
        sample = (a + b) * 0.28f * attack * release;
        if (sample > 1.f)
            sample = 1.f;
        if (sample < -1.f)
            sample = -1.f;
        horn_buf[i] = sample;
    }
}

void audio_init(void)
{
    SDL_AudioSpec want;
    SDL_AudioSpec got;

    fill_click();
    fill_horn();
    memset(&want, 0, sizeof want);
    want.freq = RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audio_callback;
    device_id = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    device_ready = device_id != 0;
    if (device_ready)
        SDL_PauseAudioDevice(device_id, 0);
}

void audio_shutdown(void)
{
    if (!device_ready)
        return;
    SDL_CloseAudioDevice(device_id);
    device_ready = 0;
}

static void retrigger(int *on, int *pos)
{
    if (!device_ready)
        return;
    SDL_LockAudioDevice(device_id);
    *pos = 0;
    *on = 1;
    SDL_UnlockAudioDevice(device_id);
}

void audio_click(void)
{
    retrigger(&click_on, &click_pos);
}

void audio_horn(void)
{
    retrigger(&horn_on, &horn_pos);
}
