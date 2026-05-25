/* fuligin/include/audio.h
 * Public interface for the FULIGIN procedural audio engine.
 * All sound effects are synthesized at startup; no external audio files required.
 * Uses SDL2_mixer for channel management and playback.
 *
 * Author: [author]
 * Date: 2025
 */

#ifndef AUDIO_H
#define AUDIO_H

/* ===========================================================================
 * SOUND EFFECT ENUMERATION
 * =========================================================================== */

typedef enum {
    /* --- Player weapons --- */
    SFX_FIRE,

    /* --- Generic explosions (noise-based, three sizes) --- */
    SFX_EXPLOSION_LG, SFX_EXPLOSION_MD, SFX_EXPLOSION_SM,

    /* --- Looping player-ship sounds --- */
    SFX_THRUST,

    /* --- Heartbeat percussion (two pitches) --- */
    SFX_BEAT1, SFX_BEAT2,

    /* --- Basic enemy (UFO) sounds --- */
    SFX_UFO_FIRE, SFX_UFO_LOOP,

    /* --- Typed enemy weapon sounds (turret through eldritch varieties) --- */
    SFX_TURRET_FIRE,
    SFX_UFO_FIRE_LG, SFX_UFO_FIRE_SM, SFX_UFO_FIRE_KAMIKAZE,
    SFX_UFO_FIRE_BOMBER, SFX_UFO_FIRE_EYE, SFX_UFO_FIRE_ELDRITCH,
    SFX_UFO_FIRE_DAEMON,

    /* --- Debris/material explosions (rock, metal, ice; three sizes each) --- */
    SFX_EXPL_ROCK_LG, SFX_EXPL_ROCK_MD, SFX_EXPL_ROCK_SM,
    SFX_EXPL_METAL_LG, SFX_EXPL_METAL_MD, SFX_EXPL_METAL_SM,
    SFX_EXPL_ICE_LG, SFX_EXPL_ICE_MD, SFX_EXPL_ICE_SM,

    SFX_COUNT
} SoundEffect;

/* ===========================================================================
 * PUBLIC API
 * =========================================================================== */

/**
 * @brief Opens SDL_mixer, synthesizes all sound effects into memory, and
 *        registers the dynamic-music post-mix callback.
 *        Must be called once before any other audio function.
 * @return 1 on success, 0 on failure.
 */
int audio_init(void);

/**
 * @brief Plays a sound effect on an available mixer channel.
 *        SFX_THRUST and SFX_UFO_LOOP are looped; all others play once.
 *        Calling this for a looping effect while it is already playing is a no-op.
 * @param sfx The sound effect to play.
 */
void audio_play(SoundEffect sfx);

/**
 * @brief Plays a sound effect with panning and pitch modulation.
 * @param sfx The sound effect to play.
 * @param x_coord The X coordinate of the sound source.
 * @param center_x The X coordinate of the screen center.
 * @param pitch The pitch modulation multiplier.
 */
void audio_play_ex(SoundEffect sfx, float x_coord, float center_x, float pitch);

/**
 * @brief Toggles a muffled low-pass filter effect for sound effects, e.g. when paused.
 * @param muffled Non-zero to enable muffle, zero to disable.
 */
void audio_set_muffled(int muffled);

/**
 * @brief Stops a currently-playing looping sound effect (SFX_THRUST or SFX_UFO_LOOP).
 *        Has no effect on one-shot effects or if the effect is not playing.
 * @param sfx The looping sound effect to halt.
 */
void audio_stop(SoundEffect sfx);

/**
 * @brief Frees all synthesized audio buffers, Mix_Chunks, and closes SDL_mixer.
 *        Must be called once at shutdown.
 */
void audio_cleanup(void);

/**
 * @brief Sets the master output volume and propagates the change to SDL_mixer.
 * @param volume_percent Master volume in the range [0, 100].
 */
void audio_set_volume(int volume_percent);

/**
 * @brief Re-applies the current settings_volume to SDL_mixer without changing
 *        the stored value.  Call after settings are reloaded from disk.
 */
void audio_update_volumes(void);

/**
 * @brief Globally mutes or unmutes all audio output.
 *        Muting silences SDL_mixer immediately; unmuting restores settings_volume.
 * @param muted Non-zero to mute, zero to unmute.
 */
void audio_mute(int muted);

/**
 * @brief Drives the dynamic music system each frame.
 *        Smoothly interpolates internal SynthState parameters toward the
 *        supplied targets, controlling the menu/gameplay crossfade, combat
 *        intensity, spookiness drone, and pause attenuation.
 *        Also registers the post-mix callback on the first call.
 * @param combat     Target combat intensity in [0.0, 1.0].
 * @param spookiness Target spookiness level in [0.0, 1.0].
 * @param paused     Non-zero when the game is paused.
 * @param gameplay   Non-zero when in gameplay (as opposed to menus).
 * @param dt         Elapsed time in seconds since the last call.
 */
void audio_set_music_params(float combat, float spookiness, int paused, int gameplay, float dt);

/**
 * @brief Alters playback speed or fades in an intense percussion track based on player health.
 * @param factor Intensity factor in [0.0, 1.0].
 */
void audio_set_intensity(float factor);

#endif /* AUDIO_H */
