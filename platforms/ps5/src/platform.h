/*
 * NetWalk PS5 platform layer -- console services used by the SDL drivers.
 *
 * Derived from PS5_Dusklight's ps5/src/platform.h
 * (Copyright (C) 2026 Mihawk, MIT licence).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Report one startup stage to klog/stderr.  Console builds deliberately do
 * not create log files: klog and stderr are the only diagnostic channels. */
void platform_boot_mark(const char *stage);

/* Report one line to klog/stderr without drawing the on-screen boot trace.
 * This is used for recurring diagnostics that must not flash anything over
 * the game. */
void platform_log_mark(const char *stage);

/* Build the persistent save path for the leaf of source_file_path.  The title
 * prefers a writable directory that is visible over FTP (/data/netwalk/
 * PPSA99877) and falls back to the title-private /download0 location.  The
 * first call also migrates an older save from the legacy locations.
 * Returns 1 and fills out on success, 0 when no writable directory exists. */
int platform_save_path(const char *source_file_path, char *out, size_t out_size);

/* Mark the directory chosen by platform_save_path as unusable and choose the
 * next candidate on the following call. */
void platform_save_path_failed(void);

/* Delete the selected save and the legacy copies of the same leaf.  Returns 1
 * when at least one file was removed. */
int platform_delete_save_path(const char *source_file_path);

/* Open klog and dismiss the shell's splash. Call before the game starts. */
void platform_init(const char *title_name);

/* The user id the title's system services should use: the native title's
 * signed-in user, or the system user (0xff) for a payload. */
int platform_active_user(void);

/* One line to klog, flushed. */
void say(const char *format, ...) __attribute__((format(printf, 1, 2)));

double now_seconds(void);

/* The pad, in the console's button numbering. */
enum {
   PAD_L3 = 0x000002,
   PAD_R3 = 0x000004,
   PAD_OPTIONS = 0x000008,
   PAD_UP = 0x000010,
   PAD_RIGHT = 0x000020,
   PAD_DOWN = 0x000040,
   PAD_LEFT = 0x000080,
   PAD_L1 = 0x000400,
   PAD_R1 = 0x000800,
   PAD_TRIANGLE = 0x001000,
   PAD_CIRCLE = 0x002000,
   PAD_CROSS = 0x004000,
   PAD_SQUARE = 0x008000,
   PAD_TOUCH_PAD = 0x100000,
};

struct pad {
   uint32_t held;    /* buttons down now */
   uint32_t pressed; /* buttons that went down since the last poll */
   float left_x, left_y, right_x, right_y; /* -1..1, a dead zone removed; y is down-positive */
   float l2, r2;                           /* 0..1 */
   /* The first touchpad contact, normalised to 0..1.  The console reports up
    * to two contacts at 1920x943; the shared SDL bridge uses the first one as
    * a mouse pointer. */
   bool touch_down;
   float touch_x, touch_y;
   uint8_t touch_id;
};

/* Up to four players, one per signed-in user: the console pairs each controller
 * with a user. Player 0 is the user who started the title and stays theirs;
 * pad_poll reads the signed-in users again about once a second, a new user's
 * controller takes the first free player, and a player is let go when their user
 * signs out. pad_poll, pad_readings, pad_vibrate and pad_light_bar are player
 * 0's; the pad_player calls reach the others. */
#define PAD_PLAYERS 4

/* Open player 0's pad (the first user's), and every other signed-in user's;
 * false (and no input for player 0) when there is none. */
bool pad_open(void);
/* Read every player's controller; *pad is player 0's. Call once a frame. */
void pad_poll(struct pad *pad);
/* The players whose controller reported itself connected, a bit each (bit 0 is
 * player 0), as of the last pad_poll. */
uint32_t pad_players(void);
/* A player's pad as the last pad_poll read it, `pressed` against the held
 * buttons *pad had; false, and a pad at rest, when that player has none. */
bool pad_player(int player, struct pad *pad);

/* Set in a reading's buttons while the shell has the pad (the home screen, a
 * system dialog): that reading is not input for the title. */
#define PAD_INTERCEPTED 0x80000000u

/* One reading as the console took it, in its numbering. */
struct pad_reading {
   uint32_t buttons;
   uint8_t left_x, left_y, right_x, right_y; /* 0..255, 128 at rest; y is down-positive */
   uint8_t l2, r2;                           /* 0..255 */
   bool connected;
   uint64_t timestamp_us;
};

/* Every reading the last pad_poll took, oldest first (the console keeps up to
 * 64 between polls), for input models that must see a tap shorter than a
 * frame. Returns their number; *readings stays valid until the next poll. */
int pad_readings(const struct pad_reading **readings);
int pad_player_readings(int player, const struct pad_reading **readings);

/* Open the console's system keyboard over the current frame and block until
 * the user confirms or cancels.  initial is UTF-8 text to edit; on success
 * (return 1) out receives the confirmed text as UTF-8.  Returns 0 when the
 * user cancelled and -1 when the dialog is unavailable. */
int pad_keyboard_open(const char *initial, char *out, size_t out_size,
                      const char *title);

/* Rumble: the large and the small motor, 0..1 each; 0, 0 stops it. */
void pad_vibrate(float large, float small);
void pad_player_vibrate(int player, float large, float small);
/* The light bar's colour. */
void pad_light_bar(uint8_t r, uint8_t g, uint8_t b);
void pad_player_light_bar(int player, uint8_t r, uint8_t g, uint8_t b);

/* Sound: one 48 kHz output of interleaved stereo 16-bit samples. A thread of
 * its own calls fill for 256 frames at a time and blocks while the console
 * plays them, so fill paces itself and must never wait on the render loop.
 * false when there is no output (the host reference build has none). */
typedef void (*audio_fill_fn)(int16_t *frames, int count, void *user);
bool audio_start(audio_fill_fn fill, void *user);
/* Stops the thread and closes the output; call it before what fill uses goes. */
void audio_stop(void);

#ifdef __cplusplus
}
#endif
