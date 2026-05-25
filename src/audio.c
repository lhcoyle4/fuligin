/* fuligin/src/audio.c
 * Procedural audio engine for FULIGIN.
 * All sound effects are synthesized at startup; no external audio files required.
 * Uses SDL2_mixer for channel management and playback.
 *
 * Author: [author]
 * Date: 2025
 */

#include "audio.h"
#include "game.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ===========================================================================
 * CONSTANTS
 * =========================================================================== */

/* Audio output sample rate in Hz. Must match Mix_OpenAudio call in audio_init(). */
#define SAMPLE_RATE 44100

/*
 * IMPORTANT: audio_buffers[] must remain allocated for the lifetime of the
 * program.  Mix_QuickLoad_RAW() does NOT copy the data — it holds a raw
 * pointer into our buffers.  Freeing them before audio_cleanup() would cause
 * SDL_mixer to read freed memory and produce noise or a crash.
 */
static int16_t *audio_buffers[SFX_COUNT];
static Mix_Chunk *mix_chunks[SFX_COUNT];
static int ufo_channel = -1;
static int thrust_channel = -1;

/* ===========================================================================
 * SOUND GENERATION
 * =========================================================================== */

/**
 * @brief Generates a player laser firing sound.
 *        Synthesis: additive sine — fundamental plus 3rd harmonic —
 *        sweeping linearly from 900 Hz down to 200 Hz with a linear
 *        amplitude decay.  The harmonic ratio gives a warm chiptune timbre.
 *        Duration: ~0.15 s.
 * @param out_len Set to the byte length of the returned buffer on success,
 *                or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_laser(int *out_len) {
    if (!out_len) return NULL;
    float duration = 0.15f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = (int16_t*)malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    double phase = 0.0;
    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        float progress = t / duration;

        /* Linear sweep from 900 Hz to 200 Hz */
        float freq = 900.0f - 700.0f * progress;
        phase += 2.0 * M_PI * freq / SAMPLE_RATE;
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;

        /* Linear volume decay */
        float amp = 12000.0f * (1.0f - progress);

        /* Mix fundamental (sine) and 3rd harmonic for a retro chiptune shape */
        float wave = (float)(sin(phase) + 0.3 * sin(3.0 * phase));
        buf[i] = (int16_t)(wave * amp);
    }
    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a white-noise explosion burst.
 *        Synthesis: white noise multiplied by an exponential amplitude
 *        envelope (amp = 16000 * exp(-decay * t)).  Used for generic
 *        explosions; duration and decay rate are caller-supplied so the
 *        same generator covers small, medium, and large variants.
 * @param duration Length of the sound in seconds.
 * @param decay    Exponential decay rate (higher = shorter tail).
 * @param out_len  Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_noise(float duration, float decay, int *out_len) {
    if (!out_len) return NULL;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = (int16_t*)malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;

        /* White noise scaled to float [-1.0, 1.0] */
        float noise = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;

        /* Exponential volume decay */
        float amp = 16000.0f * (float)exp(-decay * t);

        buf[i] = (int16_t)(noise * amp);
    }
    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a loopable rocket-thrust rumble.
 *        Synthesis: two sine waves (56 Hz and 112 Hz — the nearest multiples
 *        of 4 Hz to 55/110 Hz so each completes an integer number of cycles in
 *        0.25 s, ensuring seamless looping) blended with 1-pole low-pass
 *        filtered white noise to produce a warm, deep engine rumble rather than
 *        harsh hiss.  The loop boundary is click-free by construction.
 *        Duration: 0.25 s (looped by SDL_mixer).
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_thrust(int *out_len) {
    if (!out_len) return NULL;
    float duration = 0.25f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = (int16_t*)malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    /* Nearest multiples of 4 Hz to 55/110 Hz — complete 14 and 28 cycles in 0.25 s */
    float f1 = 56.0f;
    float f2 = 112.0f;

    double phase1 = 0.0;
    double phase2 = 0.0;
    float noise_lpf = 0.0f;

    for (int i = 0; i < len; i++) {
        phase1 += 2.0 * M_PI * f1 / SAMPLE_RATE;
        phase2 += 2.0 * M_PI * f2 / SAMPLE_RATE;

        /* Keep phases wrapped to prevent float overflow / precision loss */
        if (phase1 > 2.0 * M_PI) phase1 -= 2.0 * M_PI;
        if (phase2 > 2.0 * M_PI) phase2 -= 2.0 * M_PI;

        /* White noise */
        float noise = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;

        /* 1-pole low-pass filter: y[n] = y[n-1] + alpha * (x[n] - y[n-1]) */
        noise_lpf += 0.15f * (noise - noise_lpf);

        /* Mix sine fundamentals and low-pass filtered noise rumble */
        float signal = (float)(sin(phase1) + 0.5 * sin(phase2) + 0.4 * noise_lpf);

        buf[i] = (int16_t)(signal * 8000.0f);
    }
    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a retro percussive drum beat (kick).
 *        Synthesis: sine wave with a downward pitch sweep (freq * 1.5 -> freq * 0.8)
 *        mimicking an analogue kick drum, with a rapid exponential amplitude
 *        decay (exp(-15 * t)).  Starting frequency is caller-supplied so two
 *        distinct pitches can share the same generator (SFX_BEAT1 / SFX_BEAT2).
 *        Duration: ~0.12 s.
 * @param freq    Starting frequency in Hz.
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_beat(float freq, int *out_len) {
    if (!out_len) return NULL;
    float duration = 0.12f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = (int16_t*)malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    double phase = 0.0;
    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        float progress = t / duration;

        /* Pitch sweep downwards for a retro analogue drum thump */
        float current_freq = freq * (1.5f - 0.7f * progress);
        phase += 2.0 * M_PI * current_freq / SAMPLE_RATE;
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;

        /* Rapid exponential decay */
        float amp = 18000.0f * (float)exp(-15.0 * t);

        buf[i] = (int16_t)(sin(phase) * amp);
    }
    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a loopable UFO ambient warble.
 *        Synthesis: FM synthesis — a 600 Hz carrier frequency-modulated by an
 *        8 Hz LFO with 150 Hz depth.  Both the carrier (600 * 0.25 = 150 cycles)
 *        and the modulator (8 * 0.25 = 2 cycles) complete an integer number of
 *        cycles in 0.25 s, guaranteeing a click-free loop boundary.
 *        Duration: 0.25 s (looped by SDL_mixer).
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_ufo_loop(int *out_len) {
    if (!out_len) return NULL;
    float duration = 0.25f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = (int16_t*)malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    float fc = 600.0f;    /* Carrier frequency in Hz */
    float fm = 8.0f;      /* Modulator frequency (LFO) in Hz */
    float index = 150.0f; /* Modulation depth in Hz */

    double phase = 0.0;
    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;

        /* FM formula: freq(t) = fc + index * sin(2 * PI * fm * t) */
        float freq = fc + index * (float)sin(2.0 * M_PI * fm * t);
        phase += 2.0 * M_PI * freq / SAMPLE_RATE;
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;

        buf[i] = (int16_t)(sin(phase) * 8000.0f);
    }
    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a standard UFO weapon-fire sound.
 *        Synthesis: FM synthesis with a simultaneous linear frequency sweep.
 *        The carrier sweeps from 1200 Hz down to 400 Hz while a 45 Hz modulator
 *        at 150 Hz depth imposes a warbling alien quality.  Amplitude uses a
 *        combined exponential and linear decay for a punchy attack and smooth
 *        tail.
 *        Duration: ~0.22 s.
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_ufo_fire(int *out_len) {
    if (!out_len) return NULL;
    float duration = 0.22f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = (int16_t*)malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    double phase = 0.0;
    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        float progress = t / duration;

        /* Modulated frequency sweep */
        float base_freq = 1200.0f - 800.0f * progress;
        float mod = 150.0f * (float)sin(2.0 * M_PI * 45.0 * t);
        float freq = base_freq + mod;

        phase += 2.0 * M_PI * freq / SAMPLE_RATE;
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;

        /* Blend of exponential and linear volume decay for a punchy synth shape */
        float amp = 12000.0f * (float)exp(-6.0 * t) * (1.0f - progress);

        buf[i] = (int16_t)(sin(phase) * amp);
    }
    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a quick, punchy turret energy-weapon shot.
 *        Synthesis: a downward sine sweep (800 -> 200 Hz) with fast exponential
 *        decay, layered with a brief noise transient at the start (< 40 ms)
 *        to simulate a capacitor discharge.  Output is hard-clamped to prevent
 *        clipping from the layer sum.
 *        Duration: ~0.15 s.
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_turret_fire(int *out_len) {
    float duration = 0.15f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    float phase = 0.0f;
    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        float progress = t / duration;

        /* Sweep from 800 Hz down to 200 Hz */
        float freq = 800.0f - 600.0f * progress;
        phase += 2.0f * (float)M_PI * freq / SAMPLE_RATE;

        /* Sine wave with fast exponential decay */
        float sine_amp = 14000.0f * expf(-18.0f * t);
        float sine_val = sinf(phase) * sine_amp;

        /* Brief noise burst at the start (decays within 0.04 s) */
        float noise_val = 0.0f;
        if (t < 0.04f) {
            float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            float noise_amp = 12000.0f * (1.0f - t / 0.04f);
            noise_val = noise * noise_amp;
        }

        float combined = sine_val + noise_val;
        /* Clip protection */
        if (combined > 32767.0f) combined = 32767.0f;
        if (combined < -32768.0f) combined = -32768.0f;

        buf[i] = (int16_t)combined;
    }

    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a heavy plasma blaster shot.
 *        Synthesis: deep sine sweep from 150 Hz down to 50 Hz with a
 *        sub-harmonic (0.5x frequency) for massive low-end presence.  A
 *        1-pole IIR low-pass filtered noise layer adds rumbling texture that
 *        decays faster than the fundamental.
 *        Duration: ~0.5 s.
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_ufo_fire_lg(int *out_len) {
    float duration = 0.5f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    float phase = 0.0f;
    float noise_lpf = 0.0f; /* State for low-pass filter that shapes the rumble layer */

    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        float progress = t / duration;

        /* Sweep from 150 Hz down to 50 Hz */
        float freq = 150.0f - 100.0f * progress;
        phase += 2.0f * (float)M_PI * freq / SAMPLE_RATE;

        /* Rich sine with a sub-harmonic (0.5x frequency) for heavy presence */
        float sine_val = sinf(phase) + 0.4f * sinf(phase * 0.5f);
        float sine_amp = 18000.0f * (1.0f - progress);
        float wave = sine_val * sine_amp;

        /* Rumbling noise: 1-pole IIR low-pass filter (alpha = 0.1) */
        float raw_noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        noise_lpf += 0.1f * (raw_noise - noise_lpf);
        float rumble = noise_lpf * 10000.0f * expf(-6.0f * t);

        float combined = wave + rumble;
        if (combined > 32767.0f) combined = 32767.0f;
        if (combined < -32768.0f) combined = -32768.0f;

        buf[i] = (int16_t)combined;
    }

    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a high-speed light laser chirp.
 *        Synthesis: short sine sweep from 1500 Hz down to 800 Hz with a
 *        fast linear amplitude decay.  Brevity and high pitch give it a
 *        crisp, rapid-fire feel.
 *        Duration: ~0.08 s.
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_ufo_fire_sm(int *out_len) {
    float duration = 0.08f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    float phase = 0.0f;
    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        float progress = t / duration;

        /* Frequency sweep from 1500 Hz down to 800 Hz */
        float freq = 1500.0f - 700.0f * progress;
        phase += 2.0f * (float)M_PI * freq / SAMPLE_RATE;

        /* Fast linear decay amplitude envelope */
        float amp = 16000.0f * (1.0f - progress);
        float val = sinf(phase) * amp;

        buf[i] = (int16_t)val;
    }

    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a kamikaze-ship warning siren.
 *        Synthesis: rising sine sweep from 300 Hz to 1200 Hz (pitch urgency)
 *        combined with a 15 Hz pitch LFO (vibrato, 150 Hz depth) and an 8 Hz
 *        amplitude LFO (tremolo).  5 ms fade-in and fade-out prevent clicks at
 *        the buffer boundaries.
 *        Duration: ~0.8 s.
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_ufo_fire_kamikaze(int *out_len) {
    float duration = 0.8f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    float phase = 0.0f;
    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        float progress = t / duration;

        /* Base siren pitch rises from 300 Hz to 1200 Hz */
        float base_freq = 300.0f + 900.0f * progress;

        /* 15 Hz vibrato pitch modulation (mod depth = 150 Hz) */
        float lfo = sinf(2.0f * (float)M_PI * 15.0f * t);
        float freq = base_freq + 150.0f * lfo;

        phase += 2.0f * (float)M_PI * freq / SAMPLE_RATE;

        /* 8 Hz amplitude modulation (tremolo pulse) */
        float tremolo = 0.8f + 0.2f * sinf(2.0f * (float)M_PI * 8.0f * t);
        float amp = 14000.0f * tremolo;

        /* Smooth fade-in and fade-out to prevent clicks at buffer boundaries */
        float env = 1.0f;
        if (t < 0.05f) {
            env = t / 0.05f;
        } else if (t > duration - 0.05f) {
            env = (duration - t) / 0.05f;
        }

        float val = sinf(phase) * amp * env;
        buf[i] = (int16_t)val;
    }

    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a heavy bomb-drop thud.
 *        Synthesis: sine wave with an exponential pitch drop (340 Hz -> 40 Hz
 *        via exp(-20 * t)) and a slow exponential amplitude decay, producing a
 *        heavy, lingering sub-bass impact.
 *        Duration: ~0.8 s.
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_ufo_fire_bomber(int *out_len) {
    float duration = 0.8f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    float phase = 0.0f;
    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;

        /* Rapid frequency drop from 340 Hz to 40 Hz */
        float freq = 300.0f * expf(-20.0f * t) + 40.0f;
        phase += 2.0f * (float)M_PI * freq / SAMPLE_RATE;

        /* Slow exponential decay for a lingering tail */
        float amp = 20000.0f * expf(-4.0f * t);
        float val = sinf(phase) * amp;

        buf[i] = (int16_t)val;
    }

    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a glassy, futuristic space-beam sound.
 *        Synthesis: phase modulation (FM) — a 450 Hz modulator with a
 *        time-varying index (3.0 -> 0) shifts the phase of a 600 Hz carrier,
 *        creating bell-like FM spectra.  A 25 Hz amplitude ring modulator
 *        adds a pulsing, other-worldly quality.  Exponential envelope provides
 *        a smooth overall decay.
 *        Duration: ~0.6 s.
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_ufo_fire_eye(int *out_len) {
    float duration = 0.6f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        float progress = t / duration;

        /* Modulator: 450 Hz wave */
        float mod_phase = 2.0f * (float)M_PI * 450.0f * t;
        /* Modulation index starting at 3.0 and decaying */
        float mod_val = sinf(mod_phase) * 3.0f * (1.0f - progress);

        /* Carrier: 600 Hz phase-modulated by modulator (FM) */
        float car_freq = 600.0f;
        float car_phase = 2.0f * (float)M_PI * car_freq * t + mod_val;
        float signal = sinf(car_phase);

        /* 25 Hz amplitude ring modulation */
        float ring = sinf(2.0f * (float)M_PI * 25.0f * t);
        float amp = 16000.0f * (0.6f + 0.4f * ring) * expf(-2.0f * t);

        float val = signal * amp;
        buf[i] = (int16_t)val;
    }

    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a creepy boss/eldritch attack screech.
 *        Synthesis: additive drone from three dissonant low-frequency sines
 *        (90, 135, 45 Hz — non-harmonic ratios produce a beating, metallic
 *        quality) combined with a high-frequency screech swept by a 45 Hz LFO
 *        and pulsed by a 5 Hz amplitude envelope.  A ring-modulation term
 *        (drone * screech) adds harsh, inharmonic overtones.  The quadratic
 *        amplitude decay preserves the body of the sound longer than a linear
 *        or exponential decay.  5 ms fade-in suppresses the initial pop.
 *        Duration: ~1.0 s.
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_ufo_fire_eldritch(int *out_len) {
    float duration = 1.0f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float phase3 = 0.0f;
    float screech_phase = 0.0f;

    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        float progress = t / duration;

        /* Dissonant low-frequency drone sines */
        phase1 += 2.0f * (float)M_PI * 90.0f / SAMPLE_RATE;
        phase2 += 2.0f * (float)M_PI * 135.0f / SAMPLE_RATE;
        phase3 += 2.0f * (float)M_PI * 45.0f / SAMPLE_RATE; /* Deep sub-harmonic */

        /* Rapid screech (high freq modulated by 45 Hz LFO) */
        float screech_freq = 1200.0f + 300.0f * sinf(2.0f * (float)M_PI * 45.0f * t);
        screech_phase += 2.0f * (float)M_PI * screech_freq / SAMPLE_RATE;

        float drone = sinf(phase1) * 0.4f + sinf(phase2) * 0.3f + sinf(phase3) * 0.5f;
        /* Screech amplitude pulses slowly */
        float screech_pulse = 0.5f + 0.5f * sinf(2.0f * (float)M_PI * 5.0f * t);
        float screech = sinf(screech_phase) * 0.4f * screech_pulse;

        /* Combine drone, screech, and metallic ring modulation */
        float signal = drone + screech + 0.3f * drone * sinf(screech_phase);

        /* Slow metallic envelope decay (quadratic holds the body) */
        float amp = 12000.0f * (1.0f - progress * progress);

        /* 0.05 s fade-in to prevent initial pop */
        float env = 1.0f;
        if (t < 0.05f) {
            env = t / 0.05f;
        }

        float combined = signal * amp * env;
        if (combined > 32767.0f) combined = 32767.0f;
        if (combined < -32768.0f) combined = -32768.0f;

        buf[i] = (int16_t)combined;
    }

    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a chaotic bitcrushed weapon sound.
 *        Synthesis: dynamic sample-and-hold (downsampling factor sweeps from 4
 *        to 32 samples over the duration) combined with 3-level amplitude
 *        quantisation produces a bitcrushed, lo-fi character.  Each held sample
 *        mixes an exponentially sweeping sine (2000 -> 300 Hz) with white noise.
 *        Duration: ~0.5 s.
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_ufo_fire_daemon(int *out_len) {
    float duration = 0.5f;
    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    float last_val = 0.0f;

    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        float progress = t / duration;

        /* Dynamic downsampling factor (sweeps from 4 to 32 samples) */
        int downsample_factor = 4 + (int)(28.0f * progress);

        if (i % downsample_factor == 0) {
            /* Chaotic pitch sweep from 2000 Hz down to 300 Hz */
            float freq = 2000.0f * expf(-4.0f * progress) + 300.0f;
            float base_phase = 2.0f * (float)M_PI * freq * t;

            /* Mix sine wave with white noise */
            float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            float raw_signal = sinf(base_phase) * 0.6f + noise * 0.4f;

            /* Quantise to 8 discrete amplitude levels (bitcrushed style) */
            last_val = roundf(raw_signal * 3.0f) / 3.0f;
        }

        float amp = 14000.0f * (1.0f - progress);
        float combined = last_val * amp;

        if (combined > 32767.0f) combined = 32767.0f;
        if (combined < -32768.0f) combined = -32768.0f;

        buf[i] = (int16_t)combined;
    }

    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a rock/asteroid debris explosion at one of three sizes.
 *        Synthesis: 2-pole IIR low-pass filtered white noise (tunable cutoff
 *        per size) provides a crumbling rumble texture.  A 14 Hz amplitude LFO
 *        cross-modulated by a 3.7 Hz envelope creates uneven crumbling peaks.
 *        A rapidly decaying pitch-sweep sine (thud) adds low-frequency impact
 *        weight.  The noise and thud layers are mixed 65/35 and surrounded by
 *        5 ms fade-in / 50 ms fade-out to eliminate boundary clicks.
 * @param level   Explosion size: 0 = small (~0.4 s), 1 = medium (~0.8 s),
 *                2 = large (~1.6 s).
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_expl_rock(int level, int *out_len) {
    float duration, decay_rate, cutoff, thud_start_freq, thud_decay;

    /* Set parameters based on explosion size */
    if (level == 0) { /* Small */
        duration = 0.4f;
        decay_rate = 8.0f;
        cutoff = 350.0f;
        thud_start_freq = 130.0f;
        thud_decay = 15.0f;
    } else if (level == 1) { /* Medium */
        duration = 0.8f;
        decay_rate = 4.5f;
        cutoff = 220.0f;
        thud_start_freq = 90.0f;
        thud_decay = 8.0f;
    } else { /* Large */
        duration = 1.6f;
        decay_rate = 2.5f;
        cutoff = 130.0f;
        thud_start_freq = 65.0f;
        thud_decay = 4.0f;
    }

    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = (int16_t*)malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    /* Single-pole cascade (12 dB/octave low-pass filter) coefficient */
    float alpha = 2.0f * (float)M_PI * cutoff / SAMPLE_RATE;
    if (alpha > 1.0f) alpha = 1.0f;

    float lp1 = 0.0f;
    float lp2 = 0.0f;
    float thud_phase = 0.0f;

    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;

        /* 1. White noise generation */
        float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;

        /* 2. 2-pole low-pass filter */
        lp1 += alpha * (noise - lp1);
        lp2 += alpha * (lp1 - lp2);
        float filtered_noise = lp2;

        /* 3. Crumble modulation: 14 Hz LFO cross-modulated by a 3.7 Hz envelope */
        float crumble_lfo = sinf(2.0f * (float)M_PI * 14.0f * t)
                            * cosf(2.0f * (float)M_PI * 3.7f * t);
        float crumble_mod = 0.6f + 0.4f * crumble_lfo;

        /* Apply decay envelope to noise */
        float noise_amp = 18000.0f * expf(-decay_rate * t) * crumble_mod;
        float noise_signal = filtered_noise * noise_amp;

        /* 4. Low-frequency thud (rapidly decaying pitch sweep) */
        float current_freq = thud_start_freq * expf(-thud_decay * t);
        thud_phase += 2.0f * (float)M_PI * current_freq / SAMPLE_RATE;
        float thud_amp = 14000.0f * expf(-thud_decay * t);
        float thud_signal = sinf(thud_phase) * thud_amp;

        /* Mix the low-passed rumble and the thud */
        float mixed = noise_signal * 0.65f + thud_signal * 0.35f;

        /* Quick fade-in/fade-out to prevent clicks at buffer boundaries */
        float fade_in = (t < 0.005f) ? (t / 0.005f) : 1.0f;
        float fade_out = 1.0f;
        if (t > duration - 0.05f) {
            fade_out = (duration - t) / 0.05f;
        }
        mixed *= fade_in * fade_out;

        /* Hard clamp to prevent overflow */
        if (mixed > 32767.0f) mixed = 32767.0f;
        if (mixed < -32768.0f) mixed = -32768.0f;

        buf[i] = (int16_t)mixed;
    }

    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Generates a metallic/ship debris explosion at one of three sizes.
 *        Synthesis: two-stage model.  (1) A high-pass filtered noise transient
 *        (strike) decays extremely rapidly (~exp(-120 * t)) to simulate the
 *        impact impulse.  (2) Seven inharmonic resonant modes (ratios taken from
 *        a thin metallic plate: 1.0, 1.27, 1.50, 1.87, 2.31, 3.25, 4.10x
 *        base_freq) each ring with individual exponential decays; higher modes
 *        decay faster.  Both layers are mixed and surrounded by 1 ms / 50 ms
 *        anti-click fade envelopes.
 * @param level   Explosion size: 0 = small (~0.6 s), 1 = medium (~1.2 s),
 *                2 = large (~2.4 s).
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_expl_metal(int level, int *out_len) {
    float base_freq, duration, base_decay;
    if (level == 0) { /* Small */
        base_freq = 600.0f;
        duration = 0.6f;
        base_decay = 7.0f;
    } else if (level == 1) { /* Medium */
        base_freq = 320.0f;
        duration = 1.2f;
        base_decay = 3.5f;
    } else { /* Large */
        base_freq = 160.0f;
        duration = 2.4f;
        base_decay = 1.5f;
    }

    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = (int16_t*)malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    /* Inharmonic modes of a thin metallic plate */
    #define NUM_METAL_MODES 7
    float modes[NUM_METAL_MODES] = {1.0f, 1.27f, 1.50f, 1.87f, 2.31f, 3.25f, 4.10f};
    float phases[NUM_METAL_MODES] = {0.0f};
    float amplitudes[NUM_METAL_MODES] = {1.0f, 0.8f, 0.7f, 0.5f, 0.4f, 0.3f, 0.2f};

    float hp_state = 0.0f;

    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;

        /* 1. Strike transient: high-passed noise with immediate decay */
        float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        float hp_signal = noise - hp_state;
        hp_state += 0.25f * hp_signal; /* High-pass cut ~10 kHz */

        float strike_amp = 15000.0f * expf(-120.0f * t);
        float strike_val = hp_signal * strike_amp;

        /* 2. Ringing modes sum */
        float resonance_val = 0.0f;
        for (int k = 0; k < NUM_METAL_MODES; k++) {
            float mode_freq = base_freq * modes[k];
            /* Advance phase for this mode */
            phases[k] += 2.0f * (float)M_PI * mode_freq / SAMPLE_RATE;
            /* Higher modes decay faster */
            float mode_decay = base_decay * (1.0f + 0.4f * k);
            float mode_amp = amplitudes[k] * expf(-mode_decay * t);
            resonance_val += sinf(phases[k]) * mode_amp;
        }
        float res_signal = resonance_val * 12000.0f;

        float mixed = strike_val + res_signal;

        /* Anti-click fade */
        float fade_in = (t < 0.001f) ? (t / 0.001f) : 1.0f;
        float fade_out = 1.0f;
        if (t > duration - 0.05f) {
            fade_out = (duration - t) / 0.05f;
        }
        mixed *= fade_in * fade_out;

        /* Hard clamp */
        if (mixed > 32767.0f) mixed = 32767.0f;
        if (mixed < -32768.0f) mixed = -32768.0f;

        buf[i] = (int16_t)mixed;
    }

    *out_len = len * sizeof(int16_t);
    return buf;
}

/**
 * @brief Ice crackle grain: one active high-frequency shard oscillator.
 *        Used internally by generate_expl_ice().
 */
typedef struct {
    float phase;
    float freq;
    float amp;
    float decay;
    int active;
} IceCrackle;

/**
 * @brief Generates an ice/crystal shattering explosion at one of three sizes.
 *        Synthesis: granular + filtered noise approach.  (1) A 2-pole high-pass
 *        filtered noise layer (cutoff tuned per size) supplies the airy hiss of
 *        shattering.  (2) Up to 16 concurrent sinusoidal crackle grains are
 *        spawned probabilistically every 100 samples; each grain is a short,
 *        rapidly decaying sine in the 2500–9000 Hz range.  The spawn probability
 *        itself decays exponentially so crackles thin out over time.  Both
 *        layers are combined with fade-in/out anti-click envelopes.
 * @param level   Explosion size: 0 = small (~0.4 s), 1 = medium (~0.8 s),
 *                2 = large (~1.6 s).
 * @param out_len Set to the byte length on success, or 0 on failure.
 * @return Heap-allocated int16_t buffer, or NULL on allocation failure.
 */
static int16_t* generate_expl_ice(int level, int *out_len) {
    float duration, noise_decay, hp_cutoff, base_prob, decay_prob;
    if (level == 0) { /* Small */
        duration = 0.4f;
        noise_decay = 8.0f;
        hp_cutoff = 3000.0f;
        base_prob = 0.15f;
        decay_prob = 6.0f;
    } else if (level == 1) { /* Medium */
        duration = 0.8f;
        noise_decay = 5.0f;
        hp_cutoff = 2400.0f;
        base_prob = 0.25f;
        decay_prob = 3.5f;
    } else { /* Large */
        duration = 1.6f;
        noise_decay = 3.0f;
        hp_cutoff = 1800.0f;
        base_prob = 0.35f;
        decay_prob = 2.0f;
    }

    int len = (int)(SAMPLE_RATE * duration);
    int16_t *buf = (int16_t*)malloc(len * sizeof(int16_t));
    if (!buf) {
        *out_len = 0;
        return NULL;
    }

    /* 1-pole high-pass filter coefficient */
    float alpha = 2.0f * (float)M_PI * hp_cutoff / SAMPLE_RATE;
    if (alpha > 1.0f) alpha = 1.0f;

    float hp1_state = 0.0f;
    float hp2_state = 0.0f;

    /* Active crackle grain pool */
    #define MAX_CRACKLES 16
    IceCrackle crackles[MAX_CRACKLES];
    for (int c = 0; c < MAX_CRACKLES; c++) {
        crackles[c].active = 0;
    }

    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;

        /* 1. Dynamic crackle grain triggering — checked every 100 samples (~2.27 ms) */
        if (i % 100 == 0) {
            float prob = base_prob * expf(-decay_prob * t);
            float r = (float)rand() / RAND_MAX;
            if (r < prob) {
                /* Find first free slot in the grain pool */
                for (int c = 0; c < MAX_CRACKLES; c++) {
                    if (!crackles[c].active) {
                        crackles[c].active = 1;
                        crackles[c].phase = 0.0f;
                        /* High-pitch ice shard: 2500 Hz to 9000 Hz */
                        crackles[c].freq = 2500.0f + 6500.0f * ((float)rand() / RAND_MAX);
                        /* Randomised initial amplitude */
                        crackles[c].amp = 4000.0f + 6000.0f * ((float)rand() / RAND_MAX);
                        /* Very fast exponential decay rate (s^-1) */
                        crackles[c].decay = 150.0f + 300.0f * ((float)rand() / RAND_MAX);
                        break;
                    }
                }
            }
        }

        /* 2. Accumulate active crackle grains */
        float crackle_sum = 0.0f;
        for (int c = 0; c < MAX_CRACKLES; c++) {
            if (crackles[c].active) {
                crackles[c].phase += 2.0f * (float)M_PI * crackles[c].freq / SAMPLE_RATE;
                float c_val = sinf(crackles[c].phase) * crackles[c].amp;
                crackle_sum += c_val;
                /* Decay the grain amplitude exponentially */
                crackles[c].amp *= expf(-crackles[c].decay / SAMPLE_RATE);
                if (crackles[c].amp < 1.0f) {
                    crackles[c].active = 0;
                }
            }
        }

        /* 3. Crisp noise: 2-pole high-pass filtered white noise */
        float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        float hp1 = noise - hp1_state;
        hp1_state += alpha * hp1;
        float hp2 = hp1 - hp2_state;
        hp2_state += alpha * hp2;

        float noise_amp = 14000.0f * expf(-noise_decay * t);
        float noise_val = hp2 * noise_amp;

        /* 4. Mix noise and crackle layers */
        float mixed = noise_val + crackle_sum;

        /* Anti-click envelope */
        float fade_in = (t < 0.002f) ? (t / 0.002f) : 1.0f;
        float fade_out = 1.0f;
        if (t > duration - 0.05f) {
            fade_out = (duration - t) / 0.05f;
        }
        mixed *= fade_in * fade_out;

        /* Hard clamp */
        if (mixed > 32767.0f) mixed = 32767.0f;
        if (mixed < -32768.0f) mixed = -32768.0f;

        buf[i] = (int16_t)mixed;
    }

    *out_len = len * sizeof(int16_t);
    return buf;
}

/* ===========================================================================
 * DYNAMIC MUSIC
 * =========================================================================== */

/* Global external variables defined elsewhere in the game */
extern int settings_volume;
extern int settings_music_vol;
extern int settings_sfx_vol;
extern int settings_dynamic_range;
extern int settings_mute_unfocused;
extern SDL_Window *g_window;

/* SynthState holds all smoothly interpolated parameters for the music engine */
typedef struct {
    double time;             /* Overall music clock in seconds */
    float menu_fade;         /* 1.0 in menus, 0.0 during gameplay */
    float play_fade;         /* 1.0 during gameplay, 0.0 in menus */
    float combat_level;      /* 0.0 (quiet) to 1.0 (intense combat) */
    float spookiness;        /* 0.0 (near home) to 1.0 (deep abyss) */
    float paused_fraction;   /* 0.0 (playing) to 1.0 (paused) */
    float lpf_state;         /* Low-pass filter state for pause attenuation */
    float intensity;         /* Dynamic music tempo / intensity */
} SynthState;

static SynthState synth_state = {
    .time = 0.0,
    .menu_fade = 1.0f,
    .play_fade = 0.0f,
    .combat_level = 0.0f,
    .spookiness = 0.0f,
    .paused_fraction = 0.0f,
    .lpf_state = 0.0f,
    .intensity = 0.0f
};

/* Global mute state tracker */
static int audio_is_muted = 0;

/* Global SFX muffle state */
static int sfx_muffled = 0;

/* Chord pad base frequencies: Am, F, Dm, E */
static const float pad_freqs[4][3] = {
    {220.0f, 261.63f, 329.63f}, /* Am (A3, C4, E4) */
    {174.61f, 220.0f, 261.63f}, /* F  (F3, A3, C4) */
    {146.83f, 174.61f, 220.0f}, /* Dm (D3, F3, A3) */
    {164.81f, 207.65f, 246.94f}  /* E  (E3, G#3, B3) */
};

/* --- Music synthesizer helper functions --- */

/**
 * @brief Renders a single chord as a mellow pad voice at time t.
 *        Employs additive synthesis: fundamental + sub-octave (0.5x) +
 *        mild second harmonic (2x) across the three notes of the chord.
 * @param idx Chord index into pad_freqs [0, 3].
 * @param t   Current music time in seconds.
 * @return Mixed and normalised sample in roughly [-1.0, 1.0].
 */
static float eval_chord(int idx, double t) {
    float out = 0.0f;
    for (int i = 0; i < 3; i++) {
        double freq = (double)pad_freqs[idx][i];
        double phase = 2.0 * M_PI * freq * t;
        /* Combine fundamental, sub-octave, and mild second harmonic */
        out += (float)(sin(phase) + 0.4 * sin(phase * 0.5) + 0.2 * sin(phase * 2.0));
    }
    return out * 0.25f; /* Normalise to prevent clipping */
}

/**
 * @brief Evaluates the ambient chord pad that cycles through Am -> F -> Dm -> E,
 *        smoothly blending between chords using smoothstep interpolation during
 *        the final 2 s of each 8 s chord slot.
 * @param t Current music time in seconds.
 * @return Sample value in roughly [-1.0, 1.0].
 */
static float eval_pad(double t) {
    double chord_dur = 8.0;
    double trans_dur = 2.0;
    double cycle = 4.0 * chord_dur;
    double t_mod = fmod(t, cycle);
    int idx1 = (int)(t_mod / chord_dur) % 4;
    int idx2 = (idx1 + 1) % 4;
    double local_t = fmod(t_mod, chord_dur);

    float blend = 0.0f;
    if (local_t > (chord_dur - trans_dur)) {
        double trans_t = (local_t - (chord_dur - trans_dur)) / trans_dur;
        /* Smoothstep interpolation */
        blend = (float)(trans_t * trans_t * (3.0 - 2.0 * trans_t));
    }

    float val1 = eval_chord(idx1, t);
    float val2 = eval_chord(idx2, t);
    return (1.0f - blend) * val1 + blend * val2;
}

/**
 * @brief Evaluates a single voice of the menu arpeggiator at an offset step.
 *        Each voice plays the chord's notes in an 8-step pattern at 4 notes/s
 *        (250 ms per step), with an exponential decay envelope per note.
 *        Layering three voices at decreasing attenuation simulates a digital
 *        delay/echo effect.
 * @param t           Current music time in seconds.
 * @param step_offset Delay offset in 250 ms steps (0 = no delay).
 * @param attenuation Amplitude scalar for this tap (1.0, 0.4, 0.15, etc.).
 * @return Sample value for this voice.
 */
static float eval_arp_voice(double t, int step_offset, float attenuation) {
    double note_t = t - (double)step_offset * 0.25;
    if (note_t < 0.0) return 0.0f;

    int total_step = (int)(note_t / 0.25);
    int step = total_step % 8;
    int chord_idx = (int)(note_t / 8.0) % 4;

    float base_freq = 0.0f;
    float scale_factor = 1.0f;

    /* Arpeggio pattern over the 3 chord notes */
    switch (step) {
        case 0: base_freq = pad_freqs[chord_idx][0]; scale_factor = 1.0f; break;
        case 1: base_freq = pad_freqs[chord_idx][1]; scale_factor = 1.0f; break;
        case 2: base_freq = pad_freqs[chord_idx][2]; scale_factor = 1.0f; break;
        case 3: base_freq = pad_freqs[chord_idx][1]; scale_factor = 1.0f; break;
        case 4: base_freq = pad_freqs[chord_idx][0]; scale_factor = 2.0f; break;
        case 5: base_freq = pad_freqs[chord_idx][1]; scale_factor = 2.0f; break;
        case 6: base_freq = pad_freqs[chord_idx][2]; scale_factor = 1.0f; break;
        case 7: base_freq = pad_freqs[chord_idx][1]; scale_factor = 2.0f; break;
    }

    float freq = base_freq * scale_factor;
    double step_t = fmod(note_t, 0.25);

    /* Exponential decay envelope with 10 ms attack ramp */
    float env = 0.0f;
    if (step_t < 0.01) {
        env = (float)(step_t / 0.01);
    } else {
        env = expf(-8.0f * (float)(step_t - 0.01));
    }

    double phase = 2.0 * M_PI * (double)freq * step_t;
    float wave = (float)(sin(phase) + 0.25 * sin(phase * 2.0));

    return wave * env * attenuation;
}

/**
 * @brief Evaluates the full menu arpeggiator as three delay-tapped voices,
 *        scaled by synth_state.menu_fade so it crossfades silently when
 *        transitioning into gameplay.
 * @param t Current music time in seconds.
 * @return Sample value in roughly [-1.0, 1.0].
 */
static float eval_menu_arp(double t) {
    if (synth_state.menu_fade <= 0.01f) return 0.0f;

    /* Three voices at decreasing attenuation simulate a spacey echo */
    float voice1 = eval_arp_voice(t, 0, 1.0f);
    float voice2 = eval_arp_voice(t, 1, 0.40f);
    float voice3 = eval_arp_voice(t, 2, 0.15f);

    float mix = (voice1 + voice2 + voice3) * 0.22f;
    return mix * synth_state.menu_fade;
}

/**
 * @brief Evaluates the gameplay ambient drone.
 *        At low spookiness: a deep A1 (55 Hz) fundamental with two detuned
 *        chorus voices (55.3 and 110 Hz) for subtle width.  As spookiness
 *        increases, dissonant tritone (77.77 Hz) and minor-second (58.27 Hz)
 *        components blend in, pulsed by a slow 0.15 Hz LFO.
 * @param t          Current music time in seconds.
 * @param spookiness Spookiness level in [0.0, 1.0].
 * @return Sample value in roughly [-1.0, 1.0].
 */
static float eval_play_drone(double t, float spookiness) {
    /* Deep fundamental (A1 = 55 Hz) with subtle detuned chorus */
    double phase1 = 2.0 * M_PI * 55.0 * t;
    double phase2 = 2.0 * M_PI * 55.3 * t;
    double phase3 = 2.0 * M_PI * 110.0 * t;

    float base_drone = (float)(sin(phase1) + 0.7 * sin(phase2) + 0.3 * sin(phase3));

    /* Detuned tritonal and minor second components fade in with spookiness */
    if (spookiness > 0.01f) {
        double spooky_phase1 = 2.0 * M_PI * 77.77 * t; /* Tritone (detuned) */
        double spooky_phase2 = 2.0 * M_PI * 58.27 * t; /* Minor second (detuned) */

        /* Slow LFO pulses the dissonance */
        float lfo = (float)(0.5 + 0.5 * sin(2.0 * M_PI * 0.15 * t));
        float spooky_drone = (float)(sin(spooky_phase1) + sin(spooky_phase2)) * lfo;

        base_drone = (1.0f - spookiness * 0.5f) * base_drone
                     + spookiness * 0.8f * spooky_drone;
    }

    return base_drone * 0.28f;
}

/**
 * @brief Evaluates slow pitch-glide voices that grow more prominent with
 *        spookiness.  Uses stateless closed-form integration of FM sweep
 *        equations (phase = integral of 2*PI*freq(t) dt) for three independent
 *        glide voices at different rates (0.05, 0.03, 0.02 Hz).
 * @param t          Current music time in seconds.
 * @param spookiness Spookiness level in [0.0, 1.0].
 * @return Sample value in roughly [-1.0, 1.0].
 */
static float eval_play_spooky_glides(double t, float spookiness) {
    if (spookiness <= 0.01f) return 0.0f;

    /* Slide 1: 150 Hz base, +/- 70 Hz every ~20 s */
    double f0_1 = 150.0;
    double A1 = 70.0;
    double flfo1 = 0.05;
    double phase1 = 2.0 * M_PI * f0_1 * t - (A1 / flfo1) * cos(2.0 * M_PI * flfo1 * t);

    /* Slide 2: 250 Hz base, +/- 120 Hz every ~33 s */
    double f0_2 = 250.0;
    double A2 = 120.0;
    double flfo2 = 0.03;
    double phase2 = 2.0 * M_PI * f0_2 * t - (A2 / flfo2) * sin(2.0 * M_PI * flfo2 * t);

    /* Slide 3: whistling wind, +/- 300 Hz every ~50 s */
    double f0_3 = 800.0;
    double A3 = 300.0;
    double flfo3 = 0.02;
    double phase3 = 2.0 * M_PI * f0_3 * t - (A3 / flfo3) * cos(2.0 * M_PI * flfo3 * t);
    float lfo_wind = (float)(0.3 + 0.3 * sin(2.0 * M_PI * 0.1 * t));

    float wave1 = (float)sin(phase1);
    float wave2 = (float)sin(phase2);
    float wave3 = (float)sin(phase3) * lfo_wind;

    float mix = (wave1 + wave2 * 0.8f + wave3 * 0.3f) * 0.18f;
    return mix * spookiness;
}

/**
 * @brief Evaluates a single voice of the driving combat arpeggiator.
 *        Notes fire at 120 ms intervals in an 8-step pattern anchored to the
 *        pad_freqs chord progression.  A very short 5 ms attack ramp followed
 *        by aggressive exp(-18 * t) decay gives a snappy, percussive feel.
 *        The wave shape approximates a sawtooth via harmonic additive synthesis
 *        (fundamental + 2nd + 4th harmonic) for a bright, cutting timbre.
 * @param t           Current music time in seconds.
 * @param step_offset Delay offset in 120 ms steps.
 * @param attenuation Amplitude scalar for this voice.
 * @return Sample value for this voice.
 */
static float eval_combat_voice(double t, int step_offset, float attenuation) {
    double note_t = t - (double)step_offset * 0.12;
    if (note_t < 0.0) return 0.0f;

    int total_step = (int)(note_t / 0.12);
    int step = total_step % 8;
    int chord_idx = (int)(note_t / 8.0) % 4;

    float base_freq = 0.0f;
    float scale_factor = 1.0f;

    /* Driving minor/dissonant bass-octave progression */
    switch (step) {
        case 0: base_freq = pad_freqs[chord_idx][0]; scale_factor = 0.5f; break; /* Low root */
        case 1: base_freq = pad_freqs[chord_idx][0]; scale_factor = 0.5f; break;
        case 2: base_freq = pad_freqs[chord_idx][1]; scale_factor = 0.5f; break;
        case 3: base_freq = pad_freqs[chord_idx][0]; scale_factor = 0.5f; break;
        case 4: base_freq = pad_freqs[chord_idx][2]; scale_factor = 1.0f; break;
        case 5: base_freq = pad_freqs[chord_idx][1]; scale_factor = 1.0f; break;
        case 6: base_freq = pad_freqs[chord_idx][2]; scale_factor = 1.0f; break;
        case 7: base_freq = pad_freqs[chord_idx][0]; scale_factor = 1.0f; break;
    }

    float freq = base_freq * scale_factor;
    double step_t = fmod(note_t, 0.12);

    /* Aggressive snappy envelope: 5 ms attack, then steep decay */
    float env = 0.0f;
    if (step_t < 0.005) {
        env = (float)(step_t / 0.005);
    } else {
        env = expf(-18.0f * (float)(step_t - 0.005));
    }

    double phase = 2.0 * M_PI * (double)freq * step_t;
    /* Approximate sawtooth (fundamental + 2nd + 4th harmonic) */
    float wave = (float)(sin(phase) + 0.5 * sin(phase * 2.0) + 0.25 * sin(phase * 4.0));

    return wave * env * attenuation;
}

/**
 * @brief Evaluates the full combat arpeggiator as three delay-tapped voices,
 *        scaled by combat_level.  Silent when combat_level <= 0.01.
 * @param t           Current music time in seconds.
 * @param combat_level Combat intensity in [0.0, 1.0].
 * @return Sample value in roughly [-1.0, 1.0].
 */
static float eval_play_combat_arp(double t, float combat_level) {
    if (combat_level <= 0.01f) return 0.0f;

    float voice1 = eval_combat_voice(t, 0, 1.0f);
    float voice2 = eval_combat_voice(t, 1, 0.35f);
    float voice3 = eval_combat_voice(t, 2, 0.15f);

    float mix = (voice1 + voice2 + voice3) * 0.18f;
    return mix * combat_level;
}

/**
 * @brief SDL_mixer post-mix callback: synthesizes music in real-time and
 *        mixes it onto the stream that already contains SFX from mixer channels.
 *        Also applies volume scaling, focus muting, pause low-pass filtering,
 *        pause amplitude damping, and optional soft dynamic range compression.
 *        Registered via Mix_SetPostMix(); called on the audio thread.
 * @param udata Unused (NULL).
 * @param stream Pointer to the 16-bit mono interleaved output buffer.
 * @param len   Byte length of the buffer.
 */
static void mix_music_callback(void *udata, Uint8 *stream, int len) {
    (void)udata;
    if (!stream || len <= 0) return;

    int16_t *buf = (int16_t *)stream;
    int num_samples = len / (int)sizeof(int16_t);

    /* Check window input focus if requested by settings */
    int focused = 1;
    if (settings_mute_unfocused && g_window) {
        Uint32 flags = SDL_GetWindowFlags(g_window);
        if (!(flags & SDL_WINDOW_INPUT_FOCUS)) {
            focused = 0;
        }
    }

    /* Master scaling factor to apply mute policies */
    float master_scale = (audio_is_muted || !focused) ? 0.0f : 1.0f;

    /* Compute volume coefficients */
    float music_scale = (settings_volume / 100.0f)
                        * (settings_music_vol / 100.0f)
                        * master_scale;
    float sfx_scale = (settings_sfx_vol / 100.0f) * master_scale;

    double dt_sample = 1.0 / (double)SAMPLE_RATE;

    for (int i = 0; i < num_samples; i++) {
        /* Retrieve and scale incoming SFX sample */
        float sfx_val = (float)buf[i] * sfx_scale;

        static float sfx_lpf_state = 0.0f;
        if (sfx_muffled) {
            sfx_lpf_state += 0.15f * (sfx_val - sfx_lpf_state);
            sfx_val = sfx_lpf_state;
        } else {
            sfx_lpf_state = sfx_val;
        }

        double t = synth_state.time;

        /* Generate music tracks */
        float menu_arp_val = eval_menu_arp(t);
        float pad_val = eval_pad(t);

        float play_drone_val = eval_play_drone(t, synth_state.spookiness);
        float play_glides_val = eval_play_spooky_glides(t, synth_state.spookiness);
        float play_combat_val = eval_play_combat_arp(t, synth_state.combat_level);

        float menu_music = menu_arp_val + pad_val * 0.7f;
        float play_music = play_drone_val + play_glides_val + play_combat_val;

        /* Crossfade menu and gameplay music */
        float music_sample = menu_music * synth_state.menu_fade
                             + play_music * synth_state.play_fade;

        /* Convert to 16-bit range and apply volume scaling */
        float music_amplitude = music_sample * 8000.0f * music_scale;

        /* Low-pass filter dynamically tightens when paused (muddies the sound) */
        float alpha = 1.0f - 0.93f * synth_state.paused_fraction;
        synth_state.lpf_state += alpha * (music_amplitude - synth_state.lpf_state);
        float filtered_music = synth_state.lpf_state;

        /* Add music signal onto the existing SFX */
        float mixed_val = sfx_val + filtered_music;

        /* Damp final output when paused (2x quieter) */
        float damp_factor = 1.0f - 0.5f * synth_state.paused_fraction;
        mixed_val *= damp_factor;

        /* Soft dynamic range compression (4:1 above threshold) */
        if (settings_dynamic_range) {
            float abs_val = fabsf(mixed_val);
            float threshold = 16000.0f;
            if (abs_val > threshold) {
                float excess = abs_val - threshold;
                float compressed_abs = threshold + excess * 0.25f;
                mixed_val = (mixed_val < 0.0f) ? -compressed_abs : compressed_abs;
            }
        }

        /* Hard clamp */
        if (mixed_val > 32767.0f) mixed_val = 32767.0f;
        if (mixed_val < -32768.0f) mixed_val = -32768.0f;

        buf[i] = (int16_t)mixed_val;

        /* Advance the music clock, speeding up based on intensity */
        float speed = 1.0f + synth_state.intensity * 0.5f;
        synth_state.time += dt_sample * speed;
    }
}

/* ===========================================================================
 * AUDIO SYSTEM API
 * =========================================================================== */

/**
 * @brief Drives the dynamic music system each frame.
 *        All SynthState parameters are smoothed toward their targets using
 *        linear interpolation at rates chosen to give musically appropriate
 *        crossfade durations (menu/play: ~0.5 s; combat: ~1 s;
 *        spookiness: ~2 s; pause: ~0.33 s).
 *        Also registers the post-mix callback on first call.
 */
void audio_set_music_params(float combat, float spookiness, int paused, int gameplay, float dt) {
    static int post_mix_registered = 0;
    if (!post_mix_registered) {
        Mix_SetPostMix(mix_music_callback, NULL);
        post_mix_registered = 1;
    }

    /* Interpolate menu_fade (1.0 in menu, 0.0 in game) */
    float menu_target = gameplay ? 0.0f : 1.0f;
    if (synth_state.menu_fade < menu_target) {
        synth_state.menu_fade += 2.0f * dt;
        if (synth_state.menu_fade > menu_target) synth_state.menu_fade = menu_target;
    } else if (synth_state.menu_fade > menu_target) {
        synth_state.menu_fade -= 2.0f * dt;
        if (synth_state.menu_fade < menu_target) synth_state.menu_fade = menu_target;
    }

    /* Interpolate play_fade (1.0 in game, 0.0 in menu) */
    float play_target = gameplay ? 1.0f : 0.0f;
    if (synth_state.play_fade < play_target) {
        synth_state.play_fade += 2.0f * dt;
        if (synth_state.play_fade > play_target) synth_state.play_fade = play_target;
    } else if (synth_state.play_fade > play_target) {
        synth_state.play_fade -= 2.0f * dt;
        if (synth_state.play_fade < play_target) synth_state.play_fade = play_target;
    }

    /* Interpolate combat_level */
    if (synth_state.combat_level < combat) {
        synth_state.combat_level += 1.0f * dt;
        if (synth_state.combat_level > combat) synth_state.combat_level = combat;
    } else if (synth_state.combat_level > combat) {
        synth_state.combat_level -= 1.0f * dt;
        if (synth_state.combat_level < combat) synth_state.combat_level = combat;
    }

    /* Interpolate spookiness */
    if (synth_state.spookiness < spookiness) {
        synth_state.spookiness += 0.5f * dt;
        if (synth_state.spookiness > spookiness) synth_state.spookiness = spookiness;
    } else if (synth_state.spookiness > spookiness) {
        synth_state.spookiness -= 0.5f * dt;
        if (synth_state.spookiness < spookiness) synth_state.spookiness = spookiness;
    }

    /* Interpolate paused_fraction */
    float paused_target = paused ? 1.0f : 0.0f;
    if (synth_state.paused_fraction < paused_target) {
        synth_state.paused_fraction += 3.0f * dt;
        if (synth_state.paused_fraction > paused_target) synth_state.paused_fraction = paused_target;
    } else if (synth_state.paused_fraction > paused_target) {
        synth_state.paused_fraction -= 3.0f * dt;
        if (synth_state.paused_fraction < paused_target) synth_state.paused_fraction = paused_target;
    }
}

/**
 * @brief Sets the dynamic music tempo / intensity.
 */
void audio_set_intensity(float factor) {
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    synth_state.intensity = factor;
}

/**
 * @brief Sets the master volume percentage and propagates to SDL_mixer.
 */
void audio_set_volume(int volume_percent) {
    if (volume_percent < 0) volume_percent = 0;
    if (volume_percent > 100) volume_percent = 100;
    settings_volume = volume_percent;
    Mix_Volume(-1, (volume_percent * MIX_MAX_VOLUME) / 100);
}

/**
 * @brief Re-applies the current settings_volume to SDL_mixer.
 */
void audio_update_volumes(void) {
    audio_set_volume(settings_volume);
}

/**
 * @brief Globally mutes or unmutes all audio output.
 */
void audio_mute(int muted) {
    audio_is_muted = muted;
    if (muted) {
        Mix_Volume(-1, 0);
    } else {
        Mix_Volume(-1, (settings_volume * MIX_MAX_VOLUME) / 100);
    }
}

/**
 * @brief Toggles a muffled low-pass filter effect for sound effects, e.g. when paused.
 */
void audio_set_muffled(int muffled) {
    sfx_muffled = muffled;
}

/**
 * @brief Opens SDL_mixer, synthesizes all sound effects into memory, and
 *        registers the dynamic-music post-mix callback.
 *
 * IMPORTANT: audio_buffers[] must remain allocated for the entire lifetime of
 * the program.  Mix_QuickLoad_RAW() stores a raw pointer into each buffer
 * rather than copying the data.  Freeing a buffer before audio_cleanup() would
 * cause SDL_mixer to read freed memory on the audio thread.
 */
int audio_init(void) {
    /* Open the audio device at the project sample rate, mono, 16-bit signed */
    if (Mix_OpenAudio(SAMPLE_RATE, AUDIO_S16SYS, 1, 1024) < 0) {
        SDL_Log("SDL_mixer could not initialize! SDL_mixer Error: %s", Mix_GetError());
        return 0;
    }

    int size;

    /* --- Player sounds --- */
    audio_buffers[SFX_FIRE] = generate_laser(&size);
    mix_chunks[SFX_FIRE] = Mix_QuickLoad_RAW((Uint8*)audio_buffers[SFX_FIRE], size);

    /* --- Generic explosions (noise-based, three sizes) --- */
    audio_buffers[SFX_EXPLOSION_LG] = generate_noise(1.0f, 4.0f, &size);
    mix_chunks[SFX_EXPLOSION_LG] = Mix_QuickLoad_RAW((Uint8*)audio_buffers[SFX_EXPLOSION_LG], size);

    audio_buffers[SFX_EXPLOSION_MD] = generate_noise(0.6f, 6.0f, &size);
    mix_chunks[SFX_EXPLOSION_MD] = Mix_QuickLoad_RAW((Uint8*)audio_buffers[SFX_EXPLOSION_MD], size);

    audio_buffers[SFX_EXPLOSION_SM] = generate_noise(0.3f, 10.0f, &size);
    mix_chunks[SFX_EXPLOSION_SM] = Mix_QuickLoad_RAW((Uint8*)audio_buffers[SFX_EXPLOSION_SM], size);

    /* --- Looping ship sounds --- */
    audio_buffers[SFX_THRUST] = generate_thrust(&size);
    mix_chunks[SFX_THRUST] = Mix_QuickLoad_RAW((Uint8*)audio_buffers[SFX_THRUST], size);

    /* --- Percussion beats (two pitches) --- */
    audio_buffers[SFX_BEAT1] = generate_beat(110.0f, &size);
    mix_chunks[SFX_BEAT1] = Mix_QuickLoad_RAW((Uint8*)audio_buffers[SFX_BEAT1], size);

    audio_buffers[SFX_BEAT2] = generate_beat(90.0f, &size);
    mix_chunks[SFX_BEAT2] = Mix_QuickLoad_RAW((Uint8*)audio_buffers[SFX_BEAT2], size);

    /* --- Basic enemy (UFO) sounds --- */
    audio_buffers[SFX_UFO_FIRE] = generate_ufo_fire(&size);
    mix_chunks[SFX_UFO_FIRE] = Mix_QuickLoad_RAW((Uint8*)audio_buffers[SFX_UFO_FIRE], size);

    audio_buffers[SFX_UFO_LOOP] = generate_ufo_loop(&size);
    mix_chunks[SFX_UFO_LOOP] = Mix_QuickLoad_RAW((Uint8*)audio_buffers[SFX_UFO_LOOP], size);

    /* --- Typed enemy weapon sounds --- */
    audio_buffers[SFX_TURRET_FIRE] = generate_turret_fire(&size);
    mix_chunks[SFX_TURRET_FIRE] = Mix_QuickLoad_RAW((Uint8*)audio_buffers[SFX_TURRET_FIRE], size);

    audio_buffers[SFX_UFO_FIRE_LG] = generate_ufo_fire_lg(&size);
    mix_chunks[SFX_UFO_FIRE_LG] = Mix_QuickLoad_RAW((Uint8*)audio_buffers[SFX_UFO_FIRE_LG], size);

    audio_buffers[SFX_UFO_FIRE_SM] = generate_ufo_fire_sm(&size);
    mix_chunks[SFX_UFO_FIRE_SM] = Mix_QuickLoad_RAW((Uint8*)audio_buffers[SFX_UFO_FIRE_SM], size);

    audio_buffers[SFX_UFO_FIRE_KAMIKAZE] = generate_ufo_fire_kamikaze(&size);
    mix_chunks[SFX_UFO_FIRE_KAMIKAZE] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_UFO_FIRE_KAMIKAZE], size);

    audio_buffers[SFX_UFO_FIRE_BOMBER] = generate_ufo_fire_bomber(&size);
    mix_chunks[SFX_UFO_FIRE_BOMBER] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_UFO_FIRE_BOMBER], size);

    audio_buffers[SFX_UFO_FIRE_EYE] = generate_ufo_fire_eye(&size);
    mix_chunks[SFX_UFO_FIRE_EYE] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_UFO_FIRE_EYE], size);

    audio_buffers[SFX_UFO_FIRE_ELDRITCH] = generate_ufo_fire_eldritch(&size);
    mix_chunks[SFX_UFO_FIRE_ELDRITCH] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_UFO_FIRE_ELDRITCH], size);

    audio_buffers[SFX_UFO_FIRE_DAEMON] = generate_ufo_fire_daemon(&size);
    mix_chunks[SFX_UFO_FIRE_DAEMON] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_UFO_FIRE_DAEMON], size);

    /* --- Debris/material explosions --- */
    audio_buffers[SFX_EXPL_ROCK_LG] = generate_expl_rock(2, &size);
    mix_chunks[SFX_EXPL_ROCK_LG] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_EXPL_ROCK_LG], size);

    audio_buffers[SFX_EXPL_ROCK_MD] = generate_expl_rock(1, &size);
    mix_chunks[SFX_EXPL_ROCK_MD] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_EXPL_ROCK_MD], size);

    audio_buffers[SFX_EXPL_ROCK_SM] = generate_expl_rock(0, &size);
    mix_chunks[SFX_EXPL_ROCK_SM] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_EXPL_ROCK_SM], size);

    audio_buffers[SFX_EXPL_METAL_LG] = generate_expl_metal(2, &size);
    mix_chunks[SFX_EXPL_METAL_LG] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_EXPL_METAL_LG], size);

    audio_buffers[SFX_EXPL_METAL_MD] = generate_expl_metal(1, &size);
    mix_chunks[SFX_EXPL_METAL_MD] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_EXPL_METAL_MD], size);

    audio_buffers[SFX_EXPL_METAL_SM] = generate_expl_metal(0, &size);
    mix_chunks[SFX_EXPL_METAL_SM] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_EXPL_METAL_SM], size);

    audio_buffers[SFX_EXPL_ICE_LG] = generate_expl_ice(2, &size);
    mix_chunks[SFX_EXPL_ICE_LG] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_EXPL_ICE_LG], size);

    audio_buffers[SFX_EXPL_ICE_MD] = generate_expl_ice(1, &size);
    mix_chunks[SFX_EXPL_ICE_MD] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_EXPL_ICE_MD], size);

    audio_buffers[SFX_EXPL_ICE_SM] = generate_expl_ice(0, &size);
    mix_chunks[SFX_EXPL_ICE_SM] = Mix_QuickLoad_RAW(
        (Uint8*)audio_buffers[SFX_EXPL_ICE_SM], size);

    /* Register the real-time music synthesis callback */
    Mix_SetPostMix(mix_music_callback, NULL);

    return 1;
}

/**
 * @brief Plays a sound effect on an available mixer channel.
 *        SFX_THRUST and SFX_UFO_LOOP are looped; all others play once.
 */
void audio_play(SoundEffect sfx) {
    if (sfx < 0 || sfx >= SFX_COUNT || !mix_chunks[sfx]) return;

    if (sfx == SFX_UFO_LOOP) {
        if (ufo_channel == -1 || !Mix_Playing(ufo_channel)) {
            ufo_channel = Mix_PlayChannel(-1, mix_chunks[sfx], -1); /* loop infinitely */
        }
    } else if (sfx == SFX_THRUST) {
        if (thrust_channel == -1 || !Mix_Playing(thrust_channel)) {
            thrust_channel = Mix_PlayChannel(-1, mix_chunks[sfx], -1); /* loop infinitely */
        }
    } else {
        Mix_PlayChannel(-1, mix_chunks[sfx], 0);
    }
}

/**
 * @brief Plays a sound effect with panning and pitch modulation.
 */
void audio_play_ex(SoundEffect sfx, float x, float center_x, float pitch) {
    if (sfx < 0 || sfx >= SFX_COUNT || !mix_chunks[sfx]) return;

    int channel = -1;
    if (sfx == SFX_UFO_LOOP) {
        if (ufo_channel == -1 || !Mix_Playing(ufo_channel)) {
            ufo_channel = Mix_PlayChannel(-1, mix_chunks[sfx], -1);
        }
        channel = ufo_channel;
    } else if (sfx == SFX_THRUST) {
        if (thrust_channel == -1 || !Mix_Playing(thrust_channel)) {
            thrust_channel = Mix_PlayChannel(-1, mix_chunks[sfx], -1);
        }
        channel = thrust_channel;
    } else {
        channel = Mix_PlayChannel(-1, mix_chunks[sfx], 0);
    }

    if (channel != -1) {
        /* Panning */
        float pan = (x - center_x) / center_x;
        if (pan < -1.0f) pan = -1.0f;
        if (pan > 1.0f) pan = 1.0f;
        
        Uint8 right = (Uint8)(127.0f * (pan + 1.0f));
        Uint8 left = 254 - right;
        Mix_SetPanning(channel, left, right);

        /* Pitch modulation hook (using reverse stereo as an indicator/placeholder) */
        if (pitch < 1.0f) {
            Mix_SetReverseStereo(channel, 1);
        } else {
            Mix_SetReverseStereo(channel, 0);
        }
    }
}

/**
 * @brief Stops a currently-playing looping sound (SFX_THRUST or SFX_UFO_LOOP).
 */
void audio_stop(SoundEffect sfx) {
    if (sfx == SFX_UFO_LOOP) {
        if (ufo_channel != -1 && Mix_Playing(ufo_channel)) {
            Mix_HaltChannel(ufo_channel);
            ufo_channel = -1;
        }
    } else if (sfx == SFX_THRUST) {
        if (thrust_channel != -1 && Mix_Playing(thrust_channel)) {
            Mix_HaltChannel(thrust_channel);
            thrust_channel = -1;
        }
    }
}

/**
 * @brief Frees all synthesized audio buffers, Mix_Chunks, and closes SDL_mixer.
 */
void audio_cleanup(void) {
    Mix_SetPostMix(NULL, NULL);
    for (int i = 0; i < SFX_COUNT; i++) {
        if (mix_chunks[i]) {
            Mix_FreeChunk(mix_chunks[i]);
            mix_chunks[i] = NULL;
        }
        if (audio_buffers[i]) {
            free(audio_buffers[i]);
            audio_buffers[i] = NULL;
        }
    }
    Mix_CloseAudio();
}
