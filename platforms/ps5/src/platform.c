/*
 * NetWalk PS5 platform layer -- klog, the splash, the pad, sound, time, exit.
 *
 * Derived from PS5_Dusklight's ps5/src/platform.c
 * (Copyright (C) 2026 Mihawk, MIT licence).
 *
 * Copyright (C) 2026 Mihawk
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 *
 * Every console call here is one PS5_vkQuake, PS5_RetroArch or PS5_Vulkan's
 * smoke test runs with; no SDK header declares them.
 */
#include "platform.h"

/* The payload SDK's klog.h is GPL; keep this MIT layer independent by
 * declaring the one entry point it uses. */
int klog_printf(const char *fmt, ...);

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int sceSystemServiceHideSplashScreen(void);
int sceSystemServiceLoadExec(const char *path, const char *const *argv);
int sceUserServiceInitialize(const void *params);
int sceUserServiceGetInitialUser(int32_t *user_id);
int sceUserServiceGetForegroundUser(uint32_t *user_id);
/* The signed-in users, at most four; unused entries are -1. */
struct user_list {
   int32_t user_id[4];
};
int sceUserServiceGetLoginUserIdList(struct user_list *list);
int scePadInit(void);
int scePadOpen(int32_t user_id, int32_t port_type, int32_t index, const void *params);
int scePadGetHandle(int32_t user_id, int32_t port_type, int32_t index);
int scePadSetProcessPrivilege(int32_t privilege);
int scePadRead(int32_t handle, void *samples, int32_t capacity);
int scePadClose(int32_t handle);
int scePadSetVibrationMode(int32_t handle, int32_t mode);
int scePadSetVibration(int32_t handle, const void *vibration);
int scePadSetLightBar(int32_t handle, const void *color);

/* The PS5-native system keyboard, the PS4-inherited ImeDialog API.  Sony
 * ships no header for it in the payload SDK; this is the 0x60-byte parameter
 * block homebrew on current firmware uses.  The second Init argument is a
 * newer extension and takes NULL. */
struct ime_dialog_param {
   int32_t user_id;
   uint32_t type;                 /* 0: default, 1: basic Latin, 2: URL */
   uint64_t supported_languages;  /* 0: all installed languages */
   uint32_t enter_label;          /* 0: default, 3: Go */
   uint32_t input_method;         /* 0: default */
   const void *filter;            /* NULL: no extra filter */
   uint32_t option;
   uint32_t max_text_length;
   uint16_t *input_text_buffer;
   float posx;
   float posy;
   uint32_t horizontal_alignment; /* 1: centre */
   uint32_t vertical_alignment;   /* 1: centre */
   const uint16_t *placeholder;
   const uint16_t *title;
   int8_t reserved[16];
};
_Static_assert(sizeof(struct ime_dialog_param) == 0x60, "the ImeDialog parameter block is 96 bytes");

struct ime_dialog_result {
   int32_t end_status;            /* 0: confirmed, 1: cancelled, 2: aborted */
   int8_t reserved[12];
};
_Static_assert(sizeof(struct ime_dialog_result) == 0x10, "the ImeDialog result is 16 bytes");

int sceImeDialogInit(const struct ime_dialog_param *param, const void *extended);
int sceImeDialogGetStatus(void);
int sceImeDialogGetResult(struct ime_dialog_result *result);
int sceImeDialogTerm(void);
int sceSysmoduleLoadModule(uint16_t id);
#define NW_IME_SYSMODULE 0x0096u

/* The payload SDK's kernel.h is GPL; these are the few privilege helpers
 * the payload path needs to reach the pad IPC.  They are exported by
 * libkernel and are declared here rather than including the header. */
#if !defined(NW_PS5_NATIVE_TITLE)
uint64_t kernel_get_ucred_authid(pid_t pid);
int32_t kernel_set_ucred_authid(pid_t pid, uint64_t authid);
int32_t kernel_set_ucred_caps(pid_t pid, const uint8_t caps[16]);
int32_t kernel_set_ucred_attrs(pid_t pid, const uint8_t attrs[32]);
int32_t kernel_set_ucred_uid(pid_t pid, uid_t uid);
int32_t kernel_set_ucred_ruid(pid_t pid, uid_t ruid);
int32_t kernel_set_ucred_svuid(pid_t pid, uid_t svuid);
int32_t kernel_set_ucred_rgid(pid_t pid, gid_t rgid);
int32_t kernel_set_ucred_svgid(pid_t pid, gid_t svgid);
intptr_t kernel_get_proc_rootdir(pid_t pid);
intptr_t kernel_get_proc_jaildir(pid_t pid);
intptr_t kernel_get_root_vnode(void);
int32_t kernel_set_proc_rootdir(pid_t pid, intptr_t vnode);
int32_t kernel_set_proc_jaildir(pid_t pid, intptr_t vnode);
#endif

int sceAudioOutInit(void);
int sceAudioOutOpen(int32_t user, int32_t type, int32_t index, uint32_t grain, uint32_t rate,
                    uint32_t format);
int sceAudioOutOutput(int32_t port, const void *samples);
int sceAudioOutClose(int32_t port);
int sceKernelUsleep(uint32_t microseconds);

/* High scores live in the game's NETWALK0.282 save.  The official PS5
 * SaveData API needs a successful sceSaveDataInitialize3/mount, which the
 * fake-native-title/ShadowMount baseline does not provide.  A native title's
 * supported writable sandbox is /download0, but that is a title-private image
 * behind /user/download and cannot be browsed over FTP.  Prefer /data when the
 * loader permits it so the high-score file is visible and easy to back up;
 * otherwise fall back to the title-private directories. */
static const char *const platform_save_candidates[] = {
   "/data/netwalk/PPSA99877",
   "/download0/netwalk/PPSA99877",
   "/app0/userdata/netwalk/PPSA99877",
   "/tmp/netwalk/PPSA99877"
};

static const char *const platform_legacy_save_dirs[] = {
   "/download0",
   "/data",
   "/app0/userdata",
   "/app0",
   "/tmp",
   "/data/netwalk",
   "/download0/netwalk",
   "/app0/userdata/netwalk",
   "/download0/netwalk/PPSA99877",
   "/app0/userdata/netwalk/PPSA99877",
   "/data/homebrew/PPSA99877",
   "/data/homebrew"
};

static char platform_save_dir[256];
static unsigned platform_save_candidate;
static int platform_save_dir_ready;
static char platform_migrated_leaf[128];

static int
platform_dir_writable(const char *path)
{
   struct stat st;

   if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
      return 0;
   return access(path, R_OK | W_OK | X_OK) == 0;
}

static int
platform_make_dirs(const char *path)
{
   char buffer[256];
   size_t length = strlen(path);

   if (length == 0 || length >= sizeof(buffer))
      return 0;
   memcpy(buffer, path, length + 1);
   for (char *p = buffer + 1; *p != '\0'; p++) {
      if (*p != '/')
         continue;
      *p = '\0';
      if (mkdir(buffer, 0777) != 0 && errno != EEXIST)
         return 0;
      *p = '/';
   }
   if (mkdir(buffer, 0777) != 0 && errno != EEXIST)
      return 0;
   return platform_dir_writable(buffer);
}

static const char *
platform_path_leaf(const char *path)
{
   const char *slash;
   const char *bslash;

   if (path == NULL || path[0] == '\0')
      return NULL;
   slash = strrchr(path, '/');
   bslash = strrchr(path, '\\');
   if (bslash != NULL && (slash == NULL || bslash > slash))
      slash = bslash;
   return slash != NULL ? slash + 1 : path;
}

static void
platform_copy_save(const char *source, const char *destination)
{
   char temporary[512];
   char buffer[4096];
   int in;
   int out;
   int failed = 0;
   ssize_t got;

   if (source == NULL || destination == NULL ||
       strcmp(source, destination) == 0)
      return;
   if (access(destination, F_OK) == 0)
      return;
   if (snprintf(temporary, sizeof(temporary), "%s.tmp", destination) >=
       (int)sizeof(temporary))
      return;

   in = open(source, O_RDONLY);
   if (in < 0)
      return;
   out = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0644);
   if (out < 0) {
      close(in);
      return;
   }

   while ((got = read(in, buffer, sizeof(buffer))) > 0) {
      ssize_t written = 0;
      while (written < got) {
         ssize_t chunk = write(out, buffer + written, (size_t)(got - written));
         if (chunk <= 0) {
            failed = 1;
            break;
         }
         written += chunk;
      }
      if (failed)
         break;
   }
   if (got < 0)
      failed = 1;

   close(out);
   close(in);
   if (failed) {
      (void)unlink(temporary);
      return;
   }
   if (rename(temporary, destination) != 0)
      (void)unlink(temporary);
}

static int
platform_select_save_dir(void)
{
   if (platform_save_dir_ready)
      return 1;

   for (unsigned i = platform_save_candidate;
        i < sizeof(platform_save_candidates) / sizeof(platform_save_candidates[0]);
        i++) {
      const char *candidate = platform_save_candidates[i];
      if (!platform_make_dirs(candidate))
         continue;
      snprintf(platform_save_dir, sizeof(platform_save_dir), "%s", candidate);
      platform_save_candidate = i + 1;
      platform_save_dir_ready = 1;
#ifdef NW_DEBUG_TOOLS
      klog_printf("[NetWalk] save: %s\n", platform_save_dir);
#endif
      return 1;
   }
   platform_save_candidate =
       sizeof(platform_save_candidates) / sizeof(platform_save_candidates[0]);
   return 0;
}

static void
platform_migrate_save(const char *leaf, const char *destination)
{
   if (strcmp(platform_migrated_leaf, leaf) == 0)
      return;
   snprintf(platform_migrated_leaf, sizeof(platform_migrated_leaf), "%s", leaf);
   if (access(destination, F_OK) == 0)
      return;

   for (unsigned i = 0;
        i < sizeof(platform_legacy_save_dirs) /
                sizeof(platform_legacy_save_dirs[0]);
        i++) {
      char source[512];
      if (snprintf(source, sizeof(source), "%s/%s",
                   platform_legacy_save_dirs[i], leaf) >= (int)sizeof(source))
         continue;
      platform_copy_save(source, destination);
   }
   /* Older payload builds sometimes ran with the save beside the module. */
   platform_copy_save(leaf, destination);
}

int
platform_save_path(const char *source_file_path, char *out, size_t out_size)
{
   const char *leaf = platform_path_leaf(source_file_path);

   if (leaf == NULL || leaf[0] == '\0' || out == NULL || out_size == 0)
      return 0;
   if (!platform_select_save_dir())
      return 0;
   if (snprintf(out, out_size, "%s/%s", platform_save_dir, leaf) >=
       (int)out_size)
      return 0;

   platform_migrate_save(leaf, out);
   return 1;
}

void
platform_save_path_failed(void)
{
   platform_save_dir[0] = '\0';
   platform_save_dir_ready = 0;
   platform_migrated_leaf[0] = '\0';
}

int
platform_delete_save_path(const char *source_file_path)
{
   const char *leaf = platform_path_leaf(source_file_path);
   int deleted = 0;

   if (leaf == NULL || leaf[0] == '\0')
      return 0;

   for (unsigned i = 0;
        i < sizeof(platform_save_candidates) /
                sizeof(platform_save_candidates[0]);
        i++) {
      char path[512];
      if (snprintf(path, sizeof(path), "%s/%s",
                   platform_save_candidates[i], leaf) < (int)sizeof(path) &&
          unlink(path) == 0)
         deleted = 1;
   }
   for (unsigned i = 0;
        i < sizeof(platform_legacy_save_dirs) /
                sizeof(platform_legacy_save_dirs[0]);
        i++) {
      char path[512];
      if (snprintf(path, sizeof(path), "%s/%s",
                   platform_legacy_save_dirs[i], leaf) < (int)sizeof(path) &&
          unlink(path) == 0)
         deleted = 1;
   }
   if (unlink(leaf) == 0)
      deleted = 1;
   return deleted;
}

/* Console diagnostics use klog and stderr only.  No file is opened here, so a
 * slow or read-only filesystem cannot stall startup and no log file is left
 * beside the deployed title. */
void
platform_log_mark(const char *stage)
{
#ifdef NW_DEBUG_TOOLS
   klog_printf("[NetWalk] %s\n", stage);
#else
   (void)stage;
#endif
}

void
platform_boot_mark(const char *stage)
{
#ifdef NW_DEBUG_TOOLS
   klog_printf("[NetWalk] boot: %s\n", stage);
#else
   (void)stage;
#endif
}

static void ime_prepare(void);

void
platform_init(const char *title_name)
{
   (void)title_name;
   platform_boot_mark("platform_init");
   platform_boot_mark("platform_say");
   /* The IME system module must be requested before the loader closes the
    * title sandbox (on the first frame), not on the first keyboard press. */
   ime_prepare();
   /* The shell's splash covers the title's frames until it is dismissed. */
   (void)sceSystemServiceHideSplashScreen();
   platform_boot_mark("platform_splash");
}

void
say(const char *format, ...)
{
#ifdef NW_DEBUG_TOOLS
   char line[1024];
   va_list args;
   va_start(args, format);
   vsnprintf(line, sizeof(line), format, args);
   va_end(args);
   klog_printf("%s%s\n", "[NetWalk] ", line);
   fprintf(stderr, "%s\n", line);
   fflush(stderr);
#else
   (void)format;
#endif
}

double
now_seconds(void)
{
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

int
platform_active_user(void)
{
#if defined(NW_PS5_NATIVE_TITLE)
   static int cached = -2;
   if (cached != -2)
      return cached;
   (void)sceUserServiceInitialize(NULL);
   int32_t user = -1;
   if (sceUserServiceGetInitialUser(&user) < 0 || user < 0) {
      uint32_t foreground = 0;
      if (sceUserServiceGetForegroundUser(&foreground) >= 0 && foreground != 0)
         user = (int32_t)foreground;
   }
   if (user < 0)
      user = 0x10000000;
   /* Keep a session-context id as-is: AudioOut may accept it, and the SDL
    * driver also tries the canonical primary user and the system user. */
   cached = (int)user;
   say("audio: active user 0x%x", (unsigned)cached);
   return cached;
#else
   return 0xff;
#endif
}

/* The title ends by asking the shell to close it. exit() and returning from
 * _start both kill it, and the console then reports a crash over a run that
 * worked. The CRT calls this after main returns; the shell closes the title
 * asynchronously, so it must wait and never return. */
void
catchReturnFromMain(int status)
{
   say("exit status %d: asking the shell to close the title", status);
   fflush(NULL);
   (void)sceSystemServiceLoadExec("exit", NULL);
   for (;;)
      sceKernelUsleep(100000);
}

/* ----------------------------------------------------------------- the pad */

/* One touchpad contact as the console writes it: x and y are in the pad's
 * 1920x943 coordinate space, id identifies one continuous contact. */
struct pad_touch_contact {
   uint16_t x;
   uint16_t y;
   uint8_t id;
   uint8_t reserved[3];
};
_Static_assert(sizeof(struct pad_touch_contact) == 8, "a touchpad contact is 8 bytes");

/* One sample as the console writes it: 120 bytes, the layout PS5_vkQuake and
 * ProsperoLight read.  The live touch fields sit at 0x34..0x4b; the rest of
 * the motion block is kept only so the offsets stay explicit. */
struct pad_sample {
   uint32_t buttons;
   uint8_t left_x, left_y, right_x, right_y, l2, r2;
   uint8_t reserved_a[2];
   float orientation[4];
   float acceleration[3];
   float angular_velocity[3];
   uint8_t touch_count;
   uint8_t touch_reserved[3];
   uint32_t touch_reserved1;
   struct pad_touch_contact touches[2];
   int32_t connected;
   uint64_t timestamp_us;
   uint8_t extension[16];
   uint8_t connected_count;
   uint8_t remaining[15];
};
_Static_assert(sizeof(struct pad_sample) == 120, "the console's pad samples are 120 bytes");
_Static_assert(offsetof(struct pad_sample, touch_count) == 0x34, "the touch count sits at 0x34");
_Static_assert(offsetof(struct pad_sample, touches) == 0x3c, "the contacts start at 0x3c");
_Static_assert(offsetof(struct pad_sample, connected) == 0x4c, "connection state sits at 0x4c");
_Static_assert(offsetof(struct pad_sample, timestamp_us) == 0x50, "the timestamp sits at 0x50");

/* The two-motor rumble, as PS5_vkQuake and ProsperoLight drive it (the other
 * mode is the DualSense's haptics). */
#define PAD_VIBRATION_COMPATIBLE 2

/* A player: a signed-in user's controller, PS5_ProsperoEden's way. The console
 * pairs each controller with a user, and scePadOpen opens a user's. Player 0 is
 * the user who started the title and stays theirs; about once a second
 * pad_poll reads the signed-in users again, a new user's controller takes the
 * first free player, and a player is let go when their user signs out. */
struct pad_player_state {
   int32_t user;
   int32_t handle;
   bool connected;
   struct pad_sample last;
   struct pad_reading taken[64];
   int taken_count;
};

static struct pad_player_state pad_states[PAD_PLAYERS];
static bool pad_service;
static double pad_next_scan;
/* The canonical user for player 0's direct scePadOpen.  A payload's
 * foreground user is a session-context id; the pad IPC accepts the primary
 * user instead, and the pad service may not be ready at boot. */
static int32_t pad_primary_user = -1;
/* A payload's foreground user is a session-context id.  Sonicloader passes
 * that id straight to scePadOpen, while other payload clients use the
 * canonical primary user; try both. */
static int32_t pad_session_user = -1;
/* A user whose controller would not open, said once rather than every scan. */
static int32_t pad_unopened = -1;
#define PAD_SCAN_SECONDS 1.0

/* A player's state while they have a controller, else NULL (and always NULL
 * before pad_open has set the players up). */
static struct pad_player_state *
pad_state(int player)
{
   if (!pad_service || player < 0 || player >= PAD_PLAYERS || pad_states[player].handle < 0)
      return NULL;
   return &pad_states[player];
}

static void
pad_rest(struct pad_player_state *state)
{
   memset(state, 0, sizeof(*state));
   state->user = state->handle = -1;
   state->last.left_x = state->last.left_y = state->last.right_x = state->last.right_y = 128;
}

#if !defined(NW_PS5_NATIVE_TITLE)
/* A payload process is spawned from SceSpZeroConf with the host process's
 * credentials.  The pad IPC rejects scePadOpen from that identity with
 * 0x809b0081 (an IPMI permission error), even after
 * scePadSetProcessPrivilege.  Adopt the same jailbroken credentials a
 * homebrew payload uses before it touches the pad service.  Keep the
 * filesystem root/jail unchanged: the game's /data and /app0 mounts must
 * remain exactly where the overlay and save code expect them. */
#define PAD_ESCALATION_AUTHID 0x4801000000000013ULL

static void
pad_escalate_credentials(void)
{
   const pid_t pid = getpid();
   const uint64_t before = kernel_get_ucred_authid(pid);
   uint8_t caps[16];
   uint8_t attrs[32];

   memset(caps, 0xff, sizeof(caps));
   memset(attrs, 0, sizeof(attrs));
   attrs[0] = 0x80;

   const int auth_result = kernel_set_ucred_authid(pid, PAD_ESCALATION_AUTHID);
   const int caps_result = kernel_set_ucred_caps(pid, caps);
   const int attrs_result = kernel_set_ucred_attrs(pid, attrs);
   const int uid_result = kernel_set_ucred_uid(pid, 0);
   const int ruid_result = kernel_set_ucred_ruid(pid, 0);
   const int svuid_result = kernel_set_ucred_svuid(pid, 0);
   const int rgid_result = kernel_set_ucred_rgid(pid, 0);
   const int svgid_result = kernel_set_ucred_svgid(pid, 0);

   say("pad: escalate authid 0x%llx->0x%x caps=0x%x attrs=0x%x uid=0x%x/0x%x/0x%x gid=0x%x/0x%x",
       (unsigned long long)before, auth_result, caps_result, attrs_result, uid_result,
       ruid_result, svuid_result, rgid_result, svgid_result);
}

/* Some pad-service checks inspect the process root.  Try that only as a
 * second step, and put the original root/jail back immediately so the
 * game's file paths do not change underneath it. */
static intptr_t pad_old_rootdir;
static intptr_t pad_old_jaildir;
static bool pad_rootdir_changed;

static void
pad_escalate_rootdir(void)
{
   const pid_t pid = getpid();
   const intptr_t root = kernel_get_root_vnode();
   pad_old_rootdir = kernel_get_proc_rootdir(pid);
   pad_old_jaildir = kernel_get_proc_jaildir(pid);
   int root_result = -1;
   int jail_result = -1;
   if (root != 0 && pad_old_rootdir != 0 && pad_old_jaildir != 0) {
      root_result = kernel_set_proc_rootdir(pid, root);
      jail_result = kernel_set_proc_jaildir(pid, root);
      pad_rootdir_changed = root_result == 0 || jail_result == 0;
   }
   say("pad: rootdir escape root=0x%llx old=0x%llx/0x%llx set=0x%x/0x%x",
       (unsigned long long)root, (unsigned long long)pad_old_rootdir,
       (unsigned long long)pad_old_jaildir, root_result, jail_result);
}

static void
pad_restore_rootdir(void)
{
   if (!pad_rootdir_changed)
      return;
   const pid_t pid = getpid();
   const int root_result = kernel_set_proc_rootdir(pid, pad_old_rootdir);
   const int jail_result = kernel_set_proc_jaildir(pid, pad_old_jaildir);
   pad_rootdir_changed = false;
   say("pad: rootdir restored 0x%x/0x%x", root_result, jail_result);
}
#endif

static bool
pad_player_open(int player, int32_t user, int attempts)
{
   struct pad_player_state *state = &pad_states[player];
   pad_rest(state);
   /* A title can start before the pad service has published the device. */
   int32_t handle = -1;
   int32_t open_result = -1;
   for (int attempt = 0; attempt < attempts && handle < 0; attempt++) {
      open_result = scePadOpen(user, 0, 0, NULL);
      handle = open_result;
      if (handle < 0 && attempt + 1 < attempts)
         sceKernelUsleep(100000);
   }
   /* A handle this process already holds for the user is reused. */
   int32_t get_result = -1;
   if (handle < 0) {
      get_result = scePadGetHandle(user, 0, 0);
      handle = get_result;
   }
   if (handle < 0) {
      if (user != pad_unopened || attempts > 1)
         say("pad: scePadOpen(0x%x) = 0x%x, scePadGetHandle = 0x%x (player %d)",
             (unsigned)user, (unsigned)open_result, (unsigned)get_result, player);
      pad_unopened = user;
      return false;
   }
   state->user = user;
   state->handle = handle;
   say("pad: player %d opened for user %d, rumble mode %d", player, (int)user,
       scePadSetVibrationMode(handle, PAD_VIBRATION_COMPATIBLE));
   return true;
}

static void
pad_player_close(int player, const char *why)
{
   struct pad_player_state *state = &pad_states[player];
   if (state->handle < 0)
      return;
   say("pad: player %d let go (user %d %s), close %d", player, (int)state->user, why,
       scePadClose(state->handle));
   pad_rest(state);
}

/* Player 0 is the title's controller.  The foreground session id and the
 * canonical primary user are both worth a try; after the credentials are
 * elevated either form may be accepted by the pad IPC. */
static bool
pad_try_open_player0(int attempts)
{
   if (pad_session_user >= 0 && pad_player_open(0, pad_session_user, attempts))
      return true;
   if (pad_primary_user >= 0 && pad_player_open(0, pad_primary_user, attempts))
      return true;
   return false;
}

/* The signed-in users again: player 0 stays; a player whose user signed out is
 * let go; a new user's controller takes the first free player. */
static void
pad_scan(void)
{
   struct user_list list = {{-1, -1, -1, -1}};
   if (sceUserServiceGetLoginUserIdList(&list) < 0) {
      /* The list can be unavailable in a payload even though the pad service
       * comes up later; keep retrying player 0's canonical user. */
      if (pad_states[0].handle < 0)
         (void)pad_try_open_player0(1);
      return;
   }
   for (int player = 1; player < PAD_PLAYERS; player++) {
      if (pad_states[player].handle < 0)
         continue;
      bool signed_in = false;
      for (int i = 0; i < 4; i++)
         signed_in |= list.user_id[i] >= 0 && list.user_id[i] == pad_states[player].user;
      if (!signed_in)
         pad_player_close(player, "signed out");
   }
   for (int i = 0; i < 4; i++) {
      const int32_t user = list.user_id[i];
#if !defined(NW_PS5_NATIVE_TITLE)
      /* A payload's login list contains session-context ids; the pad IPC
       * only accepts the canonical primary-user range. */
      if ((uint32_t)user < 0x10000000u || (uint32_t)user > 0x1000000Fu)
         continue;
#endif
      bool known = user < 0;
      for (int player = 0; player < PAD_PLAYERS && !known; player++)
         known = pad_states[player].handle >= 0 && pad_states[player].user == user;
      for (int player = 0; player < PAD_PLAYERS && !known; player++) {
         if (pad_states[player].handle < 0) {
            (void)pad_player_open(player, user, 1);
            break;
         }
      }
   }
   if (pad_states[0].handle < 0)
      (void)pad_try_open_player0(1);
}

bool
pad_open(void)
{
   say("pad: open begin");
   /* SDL initializes the pad before the first frame; retry the IME load here
    * in case the constructor ran before the module manager was ready. */
   ime_prepare();
   for (int player = 0; player < PAD_PLAYERS; player++)
      pad_rest(&pad_states[player]);
#if defined(NW_PS5_NATIVE_TITLE)
   (void)sceUserServiceInitialize(NULL);
#endif

   /* A payload runs inside a repurposed SceSpZeroConf process, not a title.
    * ShellCore owns scePadInit there, so it is skipped entirely; scePadOpen
    * creates this process's own connection to the pad service. */
   int32_t user = -1;
   const int initial_result = sceUserServiceGetInitialUser(&user);
   if (initial_result < 0 || user < 0) {
      uint32_t foreground = 0;
      const int foreground_result = sceUserServiceGetForegroundUser(&foreground);
      say("pad: initial user failed (0x%x), foreground user result 0x%x, user 0x%x",
          (unsigned)initial_result, (unsigned)foreground_result,
          (unsigned)foreground);
      if (foreground_result >= 0 && foreground != 0)
         user = (int32_t)foreground;
   }
   pad_session_user = user;
   if (user < 0) {
      user = 0x10000000; /* the primary user, as payload pad clients use */
      say("pad: using primary user fallback 0x%x", (unsigned)user);
   } else if ((uint32_t)user < 0x10000000u || (uint32_t)user > 0x1000000Fu) {
      /* ForegroundUser returns a session-context id in a payload; the pad IPC
       * may accept either that id or the canonical primary-user range. */
      say("pad: user 0x%x is a session id, also trying primary user 0x10000000",
          (unsigned)user);
      user = 0x10000000;
   }
   pad_primary_user = user;
   say("pad: candidate users session=0x%x primary=0x%x",
       (unsigned)pad_session_user, (unsigned)pad_primary_user);

#if defined(NW_PS5_NATIVE_TITLE)
   const int pad_result = scePadInit();
   if (pad_result < 0)
      say("pad: scePadInit failed 0x%x", (unsigned)pad_result);
#else
   /* The pad service rejects the host's SceSpZeroConf credentials with
    * 0x809b0081.  Adopt the credentials a jailbroken payload uses. */
   pad_escalate_credentials();
   /* Once the credentials are elevated scePadOpen reports 0x80920005
    * (SCE_PAD_ERROR_NOT_INITIALIZED) until this process initializes its own
    * pad library.  ShellCore's global initialization is not enough. */
   const int init_result = scePadInit();
   say("pad: scePadInit (payload) = 0x%x", (unsigned)init_result);
#endif
   const int privilege_result = scePadSetProcessPrivilege(1);
   say("pad: scePadSetProcessPrivilege(1) = 0x%x", (unsigned)privilege_result);

   pad_service = true;
   bool opened = pad_try_open_player0(10);
#if !defined(NW_PS5_NATIVE_TITLE)
   if (!opened) {
      /* A few pad clients also need the process root to be the real root.
       * Restore it after the attempt so /data and /app0 stay unchanged. */
      pad_escalate_rootdir();
      pad_unopened = -1;
      opened = pad_try_open_player0(10);
      pad_restore_rootdir();
   }
#endif
   if (!opened)
      say("pad: player 0 unavailable for user 0x%x", (unsigned)user);
   pad_scan();
   pad_next_scan = now_seconds() + PAD_SCAN_SECONDS;
   return opened;
}

static float
stick(uint8_t value)
{
   const float v = ((float)value - 128.0f) / 127.0f;
   const float dead = 0.12f;
   if (v > -dead && v < dead)
      return 0.0f;
   return v > 0 ? (v - dead) / (1.0f - dead) : (v + dead) / (1.0f - dead);
}

static void
pad_fill(const struct pad_player_state *state, struct pad *pad)
{
   const uint32_t before = pad->held;
   const struct pad_touch_contact *touch = &state->last.touches[0];
   float tx;
   float ty;

   pad->held = state->handle >= 0 ? state->last.buttons : 0;
   pad->pressed = pad->held & ~before;
   pad->left_x = stick(state->last.left_x);
   pad->left_y = stick(state->last.left_y);
   pad->right_x = stick(state->last.right_x);
   pad->right_y = stick(state->last.right_y);
   pad->l2 = state->last.l2 / 255.0f;
   pad->r2 = state->last.r2 / 255.0f;

   pad->touch_down = state->last.touch_count > 0u;
   tx = touch->x / 1919.0f;
   ty = touch->y / 942.0f;
   if (tx < 0.0f) tx = 0.0f;
   if (tx > 1.0f) tx = 1.0f;
   if (ty < 0.0f) ty = 0.0f;
   if (ty > 1.0f) ty = 1.0f;
   pad->touch_x = tx;
   pad->touch_y = ty;
   pad->touch_id = touch->id;
}

void
pad_poll(struct pad *pad)
{
   if (pad_service && now_seconds() >= pad_next_scan) {
      pad_scan();
      pad_next_scan = now_seconds() + PAD_SCAN_SECONDS;
   }
   static struct pad_sample samples[64];
   for (int player = 0; player < PAD_PLAYERS; player++) {
      struct pad_player_state *state = pad_state(player);
      if (state == NULL)
         continue;
      state->taken_count = 0;
      const int count = scePadRead(state->handle, samples, 64);
      if (count < 0) {
         state->connected = false;
         state->last.touch_count = 0;
      }
      for (int i = 0; i < count && i < 64; i++) {
         const struct pad_sample *s = &samples[i];
         state->taken[state->taken_count++] = (struct pad_reading){
            s->buttons, s->left_x, s->left_y, s->right_x, s->right_y, s->l2, s->r2,
            s->connected != 0, s->timestamp_us};
         state->connected = s->connected != 0;
         if (s->connected && !(s->buttons & PAD_INTERCEPTED))
            state->last = *s;
         else if (s->buttons & PAD_INTERCEPTED)
            state->last.touch_count = 0; /* the shell owns the pointer */
      }
   }
   (void)pad_player(0, pad);
}

uint32_t
pad_players(void)
{
   uint32_t players = 0;
   for (int player = 0; player < PAD_PLAYERS; player++) {
      const struct pad_player_state *state = pad_state(player);
      if (state != NULL && state->connected)
         players |= 1u << player;
   }
   return players;
}

bool
pad_player(int player, struct pad *pad)
{
   const struct pad_player_state *state = pad_state(player);
   if (state == NULL) {
      struct pad_player_state rest;
      pad_rest(&rest);
      pad_fill(&rest, pad);
      return false;
   }
   pad_fill(state, pad);
   return true;
}

int
pad_player_readings(int player, const struct pad_reading **readings)
{
   const struct pad_player_state *state = pad_state(player);
   *readings = state != NULL ? state->taken : NULL;
   return state != NULL ? state->taken_count : 0;
}

int
pad_readings(const struct pad_reading **readings)
{
   return pad_player_readings(0, readings);
}

void
pad_player_vibrate(int player, float large, float small)
{
   const struct pad_player_state *state = pad_state(player);
   if (state == NULL)
      return;
   const float l = large < 0.0f ? 0.0f : large > 1.0f ? 1.0f : large;
   const float s = small < 0.0f ? 0.0f : small > 1.0f ? 1.0f : small;
   const uint8_t motors[2] = {(uint8_t)(l * 255.0f), (uint8_t)(s * 255.0f)};
   (void)scePadSetVibration(state->handle, motors);
}

void
pad_vibrate(float large, float small)
{
   pad_player_vibrate(0, large, small);
}

void
pad_player_light_bar(int player, uint8_t r, uint8_t g, uint8_t b)
{
   const struct pad_player_state *state = pad_state(player);
   if (state == NULL)
      return;
   const uint8_t color[4] = {r, g, b, 0};
   (void)scePadSetLightBar(state->handle, color);
}

void
pad_light_bar(uint8_t r, uint8_t g, uint8_t b)
{
   pad_player_light_bar(0, r, g, b);
}

/* ----------------------------------------------------------- system keyboard */

/* UTF-8 is only used at this boundary: the game's field code takes one byte
 * per character, so the caller filters the confirmed text to ASCII. */
static size_t
ime_utf8_to_utf16(const char *in, uint16_t *out, size_t capacity)
{
   size_t count = 0;
   const unsigned char *p = (const unsigned char *)in;

   if (capacity == 0)
      return 0;
   while (p != NULL && *p != '\0' && count + 1 < capacity) {
      uint32_t code = *p++;
      if (code >= 0x80u) {
         int extra = 0;
         if ((code & 0xe0u) == 0xc0u) { extra = 1; code &= 0x1fu; }
         else if ((code & 0xf0u) == 0xe0u) { extra = 2; code &= 0x0fu; }
         else if ((code & 0xf8u) == 0xf0u) { extra = 3; code &= 0x07u; }
         else { code = 0xfffdu; extra = 0; }
         while (extra-- > 0 && (*p & 0xc0u) == 0x80u)
            code = (code << 6) | (uint32_t)(*p++ & 0x3fu);
      }
      if (code > 0xffffu)
         code = 0xfffdu;
      out[count++] = (uint16_t)code;
   }
   out[count] = 0;
   return count;
}

static size_t
ime_utf16_to_utf8(const uint16_t *in, char *out, size_t capacity)
{
   size_t count = 0;

   if (capacity == 0)
      return 0;
   for (const uint16_t *p = in; *p != 0; p++) {
      uint32_t code = *p;
      if (code < 0x80u) {
         if (count + 1 >= capacity) break;
         out[count++] = (char)code;
      } else if (code < 0x800u) {
         if (count + 2 >= capacity) break;
         out[count++] = (char)(0xc0u | (code >> 6));
         out[count++] = (char)(0x80u | (code & 0x3fu));
      } else {
         if (count + 3 >= capacity) break;
         out[count++] = (char)(0xe0u | (code >> 12));
         out[count++] = (char)(0x80u | ((code >> 6) & 0x3fu));
         out[count++] = (char)(0x80u | (code & 0x3fu));
      }
   }
   out[count] = 0;
   return count;
}

static void
ime_prepare(void)
{
   static int ready;

   if (ready)
      return;

   /* Load IME before the title's sandbox is opened.  The native loader starts
    * the title with libSceImeDialog already in NEEDED, but the sysmodule load
    * is what makes the system dialog available; doing it on the first L3 press
    * is too late on current firmware.  libSceImeDialog does not need
    * libSceCommonDialog: the known-working native IME path initializes the
    * user service and calls sceImeDialogInit directly. */
   (void)sceUserServiceInitialize(NULL);
   const int module_result = sceSysmoduleLoadModule(NW_IME_SYSMODULE);
   say("ime: prepare module=0x%x", (unsigned)module_result);
   if (module_result == 0 || (uint32_t)module_result == 0x80020002u)
      ready = 1;
}

static void
ime_add_user(int32_t *candidates, int *count, int capacity, int32_t user)
{
   if (user < 0)
      return;
   for (int i = 0; i < *count; i++) {
      if (candidates[i] == user)
         return;
   }
   if (*count < capacity)
      candidates[(*count)++] = user;
}

int
pad_keyboard_open(const char *initial, char *out, size_t out_size,
                  const char *title)
{
   enum { IME_TEXT_MAX = 63 };
   uint16_t text[IME_TEXT_MAX + 1];
   uint16_t title16[64];
   struct ime_dialog_param param;
   struct ime_dialog_result result;
   int32_t candidates[6];
   int candidate_count = 0;
   int init_result = -1;
   int result_status = -1;
   int status = 0;
   size_t length = 0;

   if (out == NULL || out_size == 0)
      return -1;
   out[0] = '\0';

   /* A payload might have reached the dialog before the startup preload, or
    * the startup preload might have run before the user service was ready. */
   (void)sceUserServiceInitialize(NULL);
   ime_prepare();
   ime_utf8_to_utf16(initial != NULL ? initial : "", text,
                     sizeof(text) / sizeof(text[0]));
   ime_utf8_to_utf16(title != NULL ? title : "ENTER TEXT", title16,
                     sizeof(title16) / sizeof(title16[0]));

   memset(&param, 0, sizeof(param));
   /* Type 1 is the basic-Latin keyboard.  NetWalk's font and name fields are
    * ASCII, so this avoids an input method that can produce unsupported
    * glyphs. */
   param.type = 1;
   param.supported_languages = 0;
   param.enter_label = 3; /* Go */
   param.input_method = 0;
   param.option = 0;
   param.max_text_length = IME_TEXT_MAX;
   param.input_text_buffer = text;
   /* Native PS5 dialog coordinates are 1920x1080; centre the panel. */
   param.posx = 960.0f;
   param.posy = 540.0f;
   param.horizontal_alignment = 1;
   param.vertical_alignment = 1;
   param.title = title16;

   /* The foreground user is the one the system keyboard expects.  A payload
    * may expose a session-context id there; keep the pad's canonical primary
    * user and the login list as fallbacks. */
   {
      uint32_t foreground = 0;
      if (sceUserServiceGetForegroundUser(&foreground) >= 0 && foreground != 0)
         ime_add_user(candidates, &candidate_count,
                      (int)(sizeof(candidates) / sizeof(candidates[0])),
                      (int32_t)foreground);
   }
   ime_add_user(candidates, &candidate_count,
                (int)(sizeof(candidates) / sizeof(candidates[0])),
                pad_session_user);
   ime_add_user(candidates, &candidate_count,
                (int)(sizeof(candidates) / sizeof(candidates[0])),
                pad_primary_user);
   {
      struct user_list list = {{-1, -1, -1, -1}};
      if (sceUserServiceGetLoginUserIdList(&list) >= 0) {
         for (int i = 0; i < 4; i++)
            ime_add_user(candidates, &candidate_count,
                         (int)(sizeof(candidates) / sizeof(candidates[0])),
                         list.user_id[i]);
      }
   }
   ime_add_user(candidates, &candidate_count,
                (int)(sizeof(candidates) / sizeof(candidates[0])),
                0x10000000);
   if (candidate_count == 0)
      candidates[candidate_count++] = 0x10000000;

   say("ime: open request, %d user candidate(s)", candidate_count);
   for (int i = 0; i < candidate_count; i++) {
      param.user_id = candidates[i];
      init_result = sceImeDialogInit(&param, NULL);
      if (init_result == 0) {
         say("ime: opened for user 0x%x", (unsigned)candidates[i]);
         break;
      }
      say("ime: sceImeDialogInit(user 0x%x) = 0x%x", (unsigned)candidates[i],
          (unsigned)init_result);
   }
   if (init_result != 0)
      return -1;

   status = sceImeDialogGetStatus();
   for (int i = 0; i < 312 && status == 0; i++) {
      sceKernelUsleep(16000);
      status = sceImeDialogGetStatus();
   }
   if (status <= 0) {
      say("ime: never became visible (status %d)", status);
      (void)sceImeDialogTerm();
      return -1;
   }
   say("ime: visible (status %d)", status);
   while (status == 1) {
      sceKernelUsleep(16000);
      status = sceImeDialogGetStatus();
   }

   memset(&result, 0, sizeof(result));
   result_status = sceImeDialogGetResult(&result);
   if (result_status >= 0 && result.end_status == 0)
      length = ime_utf16_to_utf8(text, out, out_size);
   else if (result_status < 0)
      say("ime: sceImeDialogGetResult = 0x%x", (unsigned)result_status);
   say("ime: finished status %d, end %d, %u byte(s)", status,
       result.end_status, (unsigned)length);
   (void)sceImeDialogTerm();
   return (result_status >= 0 && result.end_status == 0) ? 1 : 0;
}

/* --------------------------------------------------------------- the sound */

#define AUDIO_GRAIN 256
#define AUDIO_ALREADY_INITIALISED ((int)0x8026000e)

static int32_t audio_port = -1;
static pthread_t audio_thread;
static atomic_bool audio_running;
static audio_fill_fn audio_fill;
static void *audio_user;

static void *
audio_main(void *unused)
{
   (void)unused;
   static int16_t grain[AUDIO_GRAIN * 2] __attribute__((aligned(64)));
   unsigned errors = 0;
   while (atomic_load(&audio_running)) {
      audio_fill(grain, AUDIO_GRAIN, audio_user);
      /* Blocks for one grain: this is what paces the thread. An output that
       * stops taking grains is said once, not left to stall the program. */
      if (sceAudioOutOutput(audio_port, grain) < 0) {
         if (errors++ == 0)
            say("audio: sceAudioOutOutput failed");
         sceKernelUsleep(5000);
      }
   }
   (void)sceAudioOutOutput(audio_port, NULL); /* drain the grain still queued */
   return NULL;
}

bool
audio_start(audio_fill_fn fill, void *user)
{
   if (audio_port >= 0 || fill == NULL)
      return false;
   const int init = sceAudioOutInit();
   if (init < 0 && init != AUDIO_ALREADY_INITIALISED) {
      say("audio: sceAudioOutInit 0x%08x", (unsigned)init);
      return false;
   }
   /* The system user's main port: 256 frames, 48 kHz, 16-bit stereo. */
   audio_port = sceAudioOutOpen(0xff, 0, 0, AUDIO_GRAIN, 48000, 1);
   if (audio_port < 0) {
      say("audio: sceAudioOutOpen 0x%08x", (unsigned)audio_port);
      audio_port = -1;
      return false;
   }
   audio_fill = fill;
   audio_user = user;
   atomic_store(&audio_running, true);
   if (pthread_create(&audio_thread, NULL, audio_main, NULL) != 0) {
      say("audio: no thread");
      atomic_store(&audio_running, false);
      (void)sceAudioOutClose(audio_port);
      audio_port = -1;
      return false;
   }
   say("audio: 48 kHz stereo on port %d", (int)audio_port);
   return true;
}

void
audio_stop(void)
{
   if (audio_port < 0)
      return;
   atomic_store(&audio_running, false);
   pthread_join(audio_thread, NULL);
   (void)sceAudioOutClose(audio_port);
   audio_port = -1;
}
