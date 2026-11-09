#include "audio.h"
#include "core.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define SFX_CHANNELS  4
#define SFX_CHANNEL0  0

typedef struct {
    int16_t *data;
    int      nsamples;
} SfxClip;

static SfxClip     g_sfx[SFX_COUNT];
static ndspWaveBuf g_sfx_wb[SFX_CHANNELS];
static int         g_sfx_next_ch = 0;
static int         g_audio_ready = 0;
static int         g_sfx_rate = 32000;   /* 32000 on old 3DS, 48000 on New 3DS */
static char        g_audio_status[64] = "not init";

static void sfx_tone(int16_t *out, int nsamples, int offset,
                     double freq, double amp, int wave) {
    double two_pi = 6.28318530717958647692;
    for (int i = 0; i < nsamples; i++) {
        double t = (double)i / (double)g_sfx_rate;
        double env;

        double attack = 0.005;
        if (t < attack) env = t / attack;
        else            env = exp(-(t - attack) * 4.0);
        double v = 0.0;
        if (freq > 0.0) {
            double phase = two_pi * freq * t;
            if (wave == 0)       v = sin(phase);
            else if (wave == 1)  v = (sin(phase) >= 0.0) ? 1.0 : -1.0;
            else                 v = 2.0 / 3.14159265 * asin(sin(phase));
        }
        double s = v * env * amp;
        if (s >  1.0) s =  1.0;
        if (s < -1.0) s = -1.0;
        int16_t sample = (int16_t)(s * 30000.0);
        out[offset + i] += sample;
    }
}

static int16_t *sfx_build_sweep(const double *notes, int count, int per_ms,
                                double amp, int wave, int *out_n) {
    int per_samples = (g_sfx_rate * per_ms) / 1000;
    int total = per_samples * count;
    int16_t *buf = (int16_t *)linearAlloc(total * sizeof(int16_t));
    if (!buf) { *out_n = 0; return NULL; }
    memset(buf, 0, total * sizeof(int16_t));
    for (int i = 0; i < count; i++) {
        sfx_tone(buf, per_samples, i * per_samples, notes[i], amp, wave);
    }
    *out_n = total;
    return buf;
}

static int16_t *sfx_build_blip(double freq, int ms, double amp, int wave, int *out_n) {
    int n = (g_sfx_rate * ms) / 1000;
    int16_t *buf = (int16_t *)linearAlloc(n * sizeof(int16_t));
    if (!buf) { *out_n = 0; return NULL; }
    memset(buf, 0, n * sizeof(int16_t));
    sfx_tone(buf, n, 0, freq, amp, wave);
    *out_n = n;
    return buf;
}

static int16_t *sfx_build_chord(const double *notes, int count, int ms,
                                double amp, int wave, int *out_n) {
    int n = (g_sfx_rate * ms) / 1000;
    int16_t *buf = (int16_t *)linearAlloc(n * sizeof(int16_t));
    if (!buf) { *out_n = 0; return NULL; }
    memset(buf, 0, n * sizeof(int16_t));
    for (int i = 0; i < count; i++) {
        sfx_tone(buf, n, 0, notes[i], amp / count, wave);
    }
    *out_n = n;
    return buf;
}

static void sfx_build_all(void) {
    int n;

    {
        double a[] = { 523.25, 659.25 };
        double b[] = { 659.25, 783.99, 987.77 };
        int na, nb;
        int16_t *ba = sfx_build_chord(a, 2, 220, 0.45, 2, &na);
        int16_t *bb = sfx_build_chord(b, 3, 420, 0.45, 2, &nb);
        int total = na + nb;
        int16_t *buf = (int16_t *)linearAlloc(total * sizeof(int16_t));
        if (buf) {
            memset(buf, 0, total * sizeof(int16_t));
            memcpy(buf, ba, na * sizeof(int16_t));
            memcpy(buf + na, bb, nb * sizeof(int16_t));
        }
        if (ba) linearFree(ba);
        if (bb) linearFree(bb);
        g_sfx[SFX_STARTUP].data = buf;
        g_sfx[SFX_STARTUP].nsamples = total;
    }

    g_sfx[SFX_CLICK].data = sfx_build_blip(1760.0, 35, 0.18, 1, &n);
    g_sfx[SFX_CLICK].nsamples = n;

    {
        double notes[] = { 1046.5, 1318.5, 1568.0 };
        g_sfx[SFX_DING].data = sfx_build_sweep(notes, 3, 80, 0.30, 2, &n);
        g_sfx[SFX_DING].nsamples = n;
    }

    {
        double notes[] = { 196.0, 207.65 };
        g_sfx[SFX_ERROR].data = sfx_build_chord(notes, 2, 380, 0.35, 1, &n);
        g_sfx[SFX_ERROR].nsamples = n;
    }

    {
        double notes[] = { 1200.0, 900.0 };
        g_sfx[SFX_FLAG].data = sfx_build_sweep(notes, 2, 40, 0.22, 1, &n);
        g_sfx[SFX_FLAG].nsamples = n;
    }

    g_sfx[SFX_REVEAL].data = sfx_build_blip(440.0, 40, 0.18, 0, &n);
    g_sfx[SFX_REVEAL].nsamples = n;

    {
        double notes[] = { 987.77, 783.99, 659.25, 523.25 };
        g_sfx[SFX_SHUTDOWN].data = sfx_build_sweep(notes, 4, 110, 0.40, 2, &n);
        g_sfx[SFX_SHUTDOWN].nsamples = n;
    }
}

void sfx_init(void) {
    memset(g_sfx, 0, sizeof(g_sfx));
    memset(g_sfx_wb, 0, sizeof(g_sfx_wb));

    g_sfx_rate = is_new3ds() ? 48000 : 32000;

    Result rc = ndspInit();
    if (R_FAILED(rc)) {
        snprintf(g_audio_status, sizeof(g_audio_status),
                 "run DSP1.3dsx to enable");
        g_audio_ready = 0;
        return;
    }

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    float mix[12] = {0};
    mix[0] = 1.0f; mix[1] = 1.0f;

    for (int i = 0; i < SFX_CHANNELS; i++) {
        int ch = SFX_CHANNEL0 + i;
        ndspChnReset(ch);
        ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
        ndspChnSetRate(ch, (float)g_sfx_rate);
        ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
        ndspChnSetMix(ch, mix);
    }

    sfx_build_all();
    snprintf(g_audio_status, sizeof(g_audio_status), "ready");
    g_audio_ready = 1;
}

void sfx_exit(void) {
    if (!g_audio_ready) return;
    for (int i = 0; i < SFX_CHANNELS; i++) {
        ndspChnWaveBufClear(SFX_CHANNEL0 + i);
    }
    ndspExit();
    for (int i = 0; i < SFX_COUNT; i++) {
        if (g_sfx[i].data) linearFree(g_sfx[i].data);
        g_sfx[i].data = NULL;
    }
    g_audio_ready = 0;
}

void sfx_play(SfxId id) {
    if (!g_audio_ready) return;
    if (id < 0 || id >= SFX_COUNT) return;
    SfxClip *clip = &g_sfx[id];
    if (!clip->data || clip->nsamples <= 0) return;

    int slot = g_sfx_next_ch;
    g_sfx_next_ch = (g_sfx_next_ch + 1) % SFX_CHANNELS;

    ndspWaveBuf *wb = &g_sfx_wb[slot];
    int ch = SFX_CHANNEL0 + slot;

    memset(wb, 0, sizeof(*wb));
    wb->data_vaddr = clip->data;
    wb->nsamples   = clip->nsamples;
    wb->looping    = false;
    ndspChnWaveBufAdd(ch, wb);
}

const char *sfx_status(void) { return g_audio_status; }
