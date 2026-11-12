/*
 * linux_compat.c -- POSIX implementations of the Win32 surface used by NetWalk.
 *
 * This file is compiled with the same -fleading-underscore flag as the game
 * objects, so every exported function below becomes the _Name symbol the game
 * calls.  The few places that talk to the C library use ordinary libc calls;
 * platforms/linux/libc_aliases.s supplies the matching underscore-prefixed
 * aliases.
 *
 * Scope: file streams and save slots, module/environment paths, semaphores and
 * thread records, multimedia timers, clock/calendar helpers, global memory and
 * the benign GDI/window entry points that remain linked on the SDL path.
 * Rendering, window creation and audio devices are implemented by the SDL3
 * backends, not here.
 */

#include "windows.h"
#include "mmsystem.h"
#include "nwarch.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <dirent.h>
#if !defined(__PROSPERO__) && !defined(__ANDROID__) && \
    !defined(NW_PORTABLE_IOS)
#include <malloc.h>
#include <ucontext.h>
#endif

#if defined(NW_PORTABLE_IOS)
#include "ios_memory.h"
#endif

/* FreeBSD's mmap has MAP_EXCL rather than Linux's MAP_FIXED_NOREPLACE.
 * Use a fixed, non-replacing mapping when the guest needs a specific low
 * address.  The zero fallback keeps an mmap hint from becoming a destructive
 * fixed mapping on a platform that has neither flag. */
#if defined(MAP_EXCL) && defined(MAP_FIXED)
#define NW_MAP_FIXED_NOREPLACE (MAP_FIXED | MAP_EXCL)
#elif defined(MAP_FIXED_NOREPLACE) && MAP_FIXED_NOREPLACE != 0
#define NW_MAP_FIXED_NOREPLACE MAP_FIXED_NOREPLACE
#else
#define NW_MAP_FIXED_NOREPLACE 0
#endif

extern char **environ;

/*
 * Native pointer side table for the 64-bit compatibility target.
 *
 * The original window stores pointers in 32-bit slots.  The snapshot has
 * already materialized those values against the host symbols, so the side
 * table starts as a zero-extended copy.  All pointer-field accessors use this
 * table on hosts with 64-bit pointers; scalar window bytes stay untouched.
 */
#if NW_WINDOW_PTR_SIDETABLE
nw_host_ptr_t g_nw_window_ptr_slots[NW_WINDOW_BYTES / 4u];

static void __attribute__((constructor)) nw_init_window_ptr_slots(void)
{
    size_t i;
    for (i = 0; i < (NW_WINDOW_BYTES / 4u); i++) {
        uint32_t value;
        memcpy(&value, nw_data_window + i * 4u, sizeof value);
        g_nw_window_ptr_slots[i] = (nw_host_ptr_t)(uintptr_t)value;
    }
}
#endif

/* stdio exposes stderr as a macro.  The game's references are rewritten to
 * _stderr by -fleading-underscore, so define that variable ourselves and point
 * it at fd 2 before main runs. */
#if !defined(NW_PORTABLE_LINUX)
/* The legacy -fleading-underscore build references _stderr.  The portable
 * build uses glibc's normal stderr symbol, so defining a second one would be
 * a duplicate at link time. */
#undef stderr
FILE *stderr = NULL;
#endif

/* Keep every allocation inside the 32-bit address range.  The VM trace and
 * several engine fields store heap pointers in uint32_t, and the original
 * Windows heap lives below 2 GB.  Without this, glibc serves large blocks from
 * high-address mmap regions and the trace's pointer gate rejects them. */
#if !defined(__PROSPERO__) && !defined(__ANDROID__) && \
    !defined(NW_PORTABLE_IOS)
/* glibc keeps large allocations below 2 GiB for the 32-bit guest pointer
 * fields.  The PS5's FreeBSD libc has no mallopt, Bionic's allocator does not
 * expose M_MMAP_THRESHOLD, Darwin has no mallopt, and Android/iOS rely on the
 * low-address arena below instead. */
static void __attribute__((constructor)) nw_init_malloc(void)
{
    (void)mallopt(M_MMAP_THRESHOLD, 0x40000000);
    (void)mallopt(M_TRIM_THRESHOLD, 0x40000000);
}
#endif

#if !defined(NW_PORTABLE_LINUX)
static void __attribute__((constructor)) nw_init_stderr(void)
{
    if (stderr == NULL) {
        stderr = fdopen(2, "w");
    }
}
#endif

#if !defined(__PROSPERO__) && !defined(__ANDROID__) && \
    !defined(NW_PORTABLE_IOS)
/*
 * For the optional fully static build, libX11 still tries to load
 * libXcursor.so.1 from XCreateGlyphCursor.  A static-glibc process that drags
 * in a second libc and libX11 copy can trap inside that mixed runtime on WSLg.
 * Keep libX11's core font-cursor fallback for that configuration.  The default
 * Linux build links glibc dynamically, so the interposer is not needed there,
 * but leaving it in place is harmless and keeps both build modes consistent.
 */
void *__real_dlopen(const char *filename, int flags);
void *__wrap_dlopen(const char *filename, int flags)
{
    if (filename != NULL && strstr(filename, "libXcursor.so") != NULL) {
        return NULL;
    }
    return __real_dlopen(filename, flags);
}

/* A small ELF-only fault reporter.  The 32-bit host GDB cannot inspect the
 * threads this static binary creates, so print the faulting EIP directly.
 * The handler runs on an alternate stack and uses only raw syscalls so it can
 * still report a stack-overflow fault. */
static char g_fault_stack[64 * 1024];

static char *nw_put_hex(char *p, unsigned value)
{
    static const char hex[] = "0123456789abcdef";
    int shift;
    *p++ = '0';
    *p++ = 'x';
    for (shift = 28; shift >= 0; shift -= 4) {
        *p++ = hex[(value >> shift) & 0xfu];
    }
    return p;
}

static void nw_fault_reporter(int sig, siginfo_t *info, void *vctx)
{
    ucontext_t *uc = (ucontext_t *)vctx;
    char buf[256];
    char *p = buf;
    unsigned eip = 0, esp = 0, ebp = 0, eax = 0;
    const char *tag = "NWSEGV eip=";
    int n;

    while (*tag != '\0') {
        *p++ = *tag++;
    }
#if defined(__i386__)
    eip = (unsigned)uc->uc_mcontext.gregs[REG_EIP];
    esp = (unsigned)uc->uc_mcontext.gregs[REG_ESP];
    ebp = (unsigned)uc->uc_mcontext.gregs[REG_EBP];
    eax = (unsigned)uc->uc_mcontext.gregs[REG_EAX];
#elif defined(__x86_64__)
    eip = (unsigned)uc->uc_mcontext.gregs[REG_RIP];
    esp = (unsigned)uc->uc_mcontext.gregs[REG_RSP];
    ebp = (unsigned)uc->uc_mcontext.gregs[REG_RBP];
    eax = (unsigned)uc->uc_mcontext.gregs[REG_RAX];
#endif
    p = nw_put_hex(p, eip);
    *p++ = ' ';
    *p++ = 's';
    *p++ = 'p';
    *p++ = '=';
    p = nw_put_hex(p, esp);
    *p++ = ' ';
    *p++ = 'b';
    *p++ = 'p';
    *p++ = '=';
    p = nw_put_hex(p, ebp);
    *p++ = ' ';
    *p++ = 'a';
    *p++ = 'x';
    *p++ = '=';
    p = nw_put_hex(p, eax);
    *p++ = ' ';
    *p++ = 'f';
    *p++ = 'a';
    *p++ = 'u';
    *p++ = 'l';
    *p++ = 't';
    *p++ = '=';
    p = nw_put_hex(p, (unsigned)(uintptr_t)(info != NULL ? info->si_addr : NULL));
    *p++ = ' ';
    *p++ = 's';
    *p++ = 'i';
    *p++ = 'g';
    *p++ = '=';
    p = nw_put_hex(p, (unsigned)sig);
    *p++ = '\n';
    n = (int)(p - buf);
    (void)write(2, buf, (size_t)n);
    _exit(128 + sig);
}

static void __attribute__((constructor)) nw_install_fault_reporter(void)
{
    struct sigaction sa;
    stack_t ss;

    memset(&ss, 0, sizeof(ss));
    ss.ss_sp = g_fault_stack;
    ss.ss_size = sizeof(g_fault_stack);
    (void)sigaltstack(&ss, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = nw_fault_reporter;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
}
#endif /* !__PROSPERO__ && !__ANDROID__ && !NW_PORTABLE_IOS */

/* ------------------------------------------------------------------ */
/* Process-wide paths and command line                                */
/* ------------------------------------------------------------------ */
static char g_module_path_posix[PATH_MAX];
static char g_module_path_win[PATH_MAX + 8];
static char g_command_line[16384];

static void nw_posix_to_win(const char *src, char *dst, size_t dst_size)
{
    size_t o = 0;

    if (dst_size == 0) {
        return;
    }
    if (src != NULL && src[0] == '/') {
        if (o + 2 < dst_size) {
            dst[o++] = 'Z';
            dst[o++] = ':';
        }
    }
    while (src != NULL && *src != '\0' && o + 1 < dst_size) {
        char c = *src++;
        dst[o++] = (c == '/') ? '\\' : c;
    }
    dst[o] = '\0';
}

static void nw_win_to_posix(const char *src, char *dst, size_t dst_size)
{
    size_t o = 0;

    if (dst_size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    /* Strip a drive designator (Z:\...), a \\?\ prefix, or a leading .\ . */
    if (isalpha((unsigned char)src[0]) && src[1] == ':') {
        src += 2;
    }
    if (src[0] == '\\' && src[1] == '\\' && src[2] == '?' && src[3] == '\\') {
        src += 4;
    }

    while (*src != '\0' && o + 1 < dst_size) {
        char c = *src++;
        if (c == '\\') {
            c = '/';
        }
        if (c == '/' && o > 0 && dst[o - 1] == '/') {
            continue;
        }
        dst[o++] = c;
    }
    if (o == 0 && dst_size > 1) {
        dst[o++] = '.';
    }
    dst[o] = '\0';
}

static void nw_init_module_path(void)
{
    if (g_module_path_posix[0] != '\0') {
        return;
    }
#if defined(__PROSPERO__)
    /* A title's own folder is mounted at /app0.  The filename is only used
     * to derive that folder, so any name in it is sufficient. */
    strcpy(g_module_path_posix, "/app0/eboot.bin");
#elif defined(__ANDROID__)
    /* /proc/self/exe names the zygote (or app_process), not the shared
     * library carrying this code.  android_main.c installs the app's internal
     * storage directory before the game starts; cwd is only a last-resort
     * fallback for unusual embedders. */
    if (getcwd(g_module_path_posix, sizeof(g_module_path_posix)) == NULL)
        strcpy(g_module_path_posix, ".");
#else
    ssize_t n = readlink("/proc/self/exe", g_module_path_posix,
                         sizeof(g_module_path_posix) - 1);
    if (n < 0) {
        if (getcwd(g_module_path_posix, sizeof(g_module_path_posix)) == NULL) {
            strcpy(g_module_path_posix, ".");
        }
    } else {
        g_module_path_posix[n] = '\0';
    }
#endif
    nw_posix_to_win(g_module_path_posix, g_module_path_win,
                    sizeof(g_module_path_win));
}

/* Android's application process has no useful module path: the native code is
 * a shared library inside the APK and /proc/self/exe points at the zygote.
 * The platform entry point sets the writable internal-storage directory and a
 * synthetic module filename here before any game code asks for a path. */
void NwLinuxSetModulePath(const char *pszPosixPath)
{
    if (pszPosixPath == NULL || pszPosixPath[0] == '\0')
        return;
    (void)snprintf(g_module_path_posix, sizeof(g_module_path_posix), "%s",
                   pszPosixPath);
    nw_posix_to_win(g_module_path_posix, g_module_path_win,
                    sizeof(g_module_path_win));
}

void NwLinuxSetArgs(int argc, char **argv)
{
    size_t o = 0;
    int i;

    nw_init_module_path();
    g_command_line[0] = '\0';
    for (i = 0; i < argc; i++) {
        const char *arg = argv[i];
        int quote = (arg[0] == '\0' || strpbrk(arg, " \t\n\"") != NULL);

        if (i != 0 && o + 1 < sizeof(g_command_line)) {
            g_command_line[o++] = ' ';
        }
        if (quote && o + 1 < sizeof(g_command_line)) {
            g_command_line[o++] = '"';
        }
        while (*arg != '\0' && o + 1 < sizeof(g_command_line)) {
            g_command_line[o++] = *arg++;
        }
        if (quote && o + 1 < sizeof(g_command_line)) {
            g_command_line[o++] = '"';
        }
    }
    g_command_line[o] = '\0';


}

const char *NwLinuxModulePathPosix(void)
{
    nw_init_module_path();
    return g_module_path_posix;
}

/* ------------------------------------------------------------------ */
/* Handles                                                            */
/* ------------------------------------------------------------------ */
enum {
    NW_HANDLE_FILE = 1,
    NW_HANDLE_SEM,
    NW_HANDLE_THREAD,
    NW_HANDLE_FIND
};

/*
 * Darwin, including iOS, does not implement unnamed POSIX semaphores
 * (sem_init fails with ENOSYS).  Use a mutex/condition counter there; the
 * other POSIX targets keep sem_t so their behaviour is unchanged.
 */
#if defined(NW_PORTABLE_IOS)
typedef struct NwSem {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    long count;
} NwSem;

static int nw_sem_init(NwSem *p, long value)
{
    if (pthread_mutex_init(&p->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&p->cond, NULL) != 0) {
        pthread_mutex_destroy(&p->mutex);
        return -1;
    }
    p->count = value;
    return 0;
}

static void nw_sem_destroy(NwSem *p)
{
    pthread_cond_destroy(&p->cond);
    pthread_mutex_destroy(&p->mutex);
}

static long nw_sem_value(NwSem *p)
{
    long value;

    pthread_mutex_lock(&p->mutex);
    value = p->count;
    pthread_mutex_unlock(&p->mutex);
    return value;
}

static void nw_sem_post(NwSem *p)
{
    pthread_mutex_lock(&p->mutex);
    p->count++;
    pthread_cond_signal(&p->cond);
    pthread_mutex_unlock(&p->mutex);
}

static DWORD nw_sem_wait(NwSem *p, DWORD dwMilliseconds)
{
    struct timespec ts;
    DWORD result = WAIT_OBJECT_0;

    if (pthread_mutex_lock(&p->mutex) != 0) {
        return WAIT_FAILED;
    }
    if (dwMilliseconds != INFINITE) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += (time_t)(dwMilliseconds / 1000);
        ts.tv_nsec += (long)(dwMilliseconds % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        }
    }
    while (p->count == 0) {
        int rc;
        if (dwMilliseconds == INFINITE) {
            rc = pthread_cond_wait(&p->cond, &p->mutex);
        } else {
            rc = pthread_cond_timedwait(&p->cond, &p->mutex, &ts);
        }
        if (rc == ETIMEDOUT) {
            result = WAIT_TIMEOUT;
            break;
        }
        if (rc != 0) {
            result = WAIT_FAILED;
            break;
        }
    }
    if (result == WAIT_OBJECT_0) {
        p->count--;
    }
    pthread_mutex_unlock(&p->mutex);
    return result;
}
#else
typedef sem_t NwSem;

static int nw_sem_init(NwSem *p, long value)
{
    return sem_init(p, 0, (unsigned)value);
}

static void nw_sem_destroy(NwSem *p)
{
    sem_destroy(p);
}

static long nw_sem_value(NwSem *p)
{
    int value = 0;
    sem_getvalue(p, &value);
    return value;
}

static void nw_sem_post(NwSem *p)
{
    sem_post(p);
}

static DWORD nw_sem_wait(NwSem *p, DWORD dwMilliseconds)
{
    if (dwMilliseconds == INFINITE) {
        while (sem_wait(p) != 0) {
            if (errno != EINTR) {
                return WAIT_FAILED;
            }
        }
        return WAIT_OBJECT_0;
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += dwMilliseconds / 1000;
        ts.tv_nsec += (long)(dwMilliseconds % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        }
        while (sem_timedwait(p, &ts) != 0) {
            if (errno == EINTR) {
                continue;
            }
            return (errno == ETIMEDOUT) ? WAIT_TIMEOUT : WAIT_FAILED;
        }
        return WAIT_OBJECT_0;
    }
}
#endif

typedef struct NwHandle {
    int kind;
    int fd;
    int standard;              /* static stdio handle, never freed */
    NwSem sem;
    pthread_t thread;
    int detached;
    DIR *dir;
    char find_path[PATH_MAX];
    char find_pattern[NAME_MAX + 1];
} NwHandle;

static NwHandle g_std_handles[3];
static int g_std_ready;

static NwHandle *nw_std_handle(DWORD which)
{
    int idx;

    if (!g_std_ready) {
        int i;
        for (i = 0; i < 3; i++) {
            g_std_handles[i].kind = NW_HANDLE_FILE;
            g_std_handles[i].fd = i;
            g_std_handles[i].standard = 1;
            g_std_handles[i].detached = 1;
        }
        g_std_ready = 1;
    }
    switch (which) {
    case STD_INPUT_HANDLE:  idx = 0; break;
    case STD_OUTPUT_HANDLE: idx = 1; break;
    case STD_ERROR_HANDLE:  idx = 2; break;
    default:                return NULL;
    }
    return &g_std_handles[idx];
}

static DWORD nw_last_error;

static DWORD nw_errno_to_win(int e)
{
    switch (e) {
    case 0:         return 0;
    case ENOENT:    return 2;       /* ERROR_FILE_NOT_FOUND */
    case EACCES:
    case EPERM:     return 5;       /* ERROR_ACCESS_DENIED  */
    case EBADF:     return 6;       /* ERROR_INVALID_HANDLE */
    case ENOMEM:    return 8;       /* ERROR_NOT_ENOUGH_MEMORY */
    case EEXIST:    return 80;      /* ERROR_FILE_EXISTS    */
    case ENOSPC:    return 112;     /* ERROR_DISK_FULL      */
    default:        return (DWORD)(e + 1000);
    }
}

DWORD GetLastError(void)
{
    return nw_last_error;
}

void SetLastError(DWORD err)
{
    nw_last_error = err;
}

DWORD GetCurrentThreadId(void)
{
    return (DWORD)(uintptr_t)pthread_self();
}

DWORD WinGetCurrentThreadId(void)
{
    return GetCurrentThreadId();
}

HANDLE GetStdHandle(DWORD which)
{
    NwHandle *h = nw_std_handle(which);
    return h != NULL ? (HANDLE)h : INVALID_HANDLE_VALUE;
}

BOOL SetStdHandle(DWORD which, HANDLE h)
{
    NwHandle *slot = nw_std_handle(which);
    NwHandle *src = (NwHandle *)h;

    if (slot == NULL || src == NULL || src->kind != NW_HANDLE_FILE) {
        return FALSE;
    }
    slot->fd = src->fd;
    return TRUE;
}

DWORD GetFileType(HANDLE h)
{
    NwHandle *nw = (NwHandle *)h;
    struct stat st;

    if (nw == NULL || nw == (NwHandle *)INVALID_HANDLE_VALUE) {
        nw_last_error = 6;
        return FILE_TYPE_UNKNOWN;
    }
    if (nw->kind == NW_HANDLE_SEM || nw->kind == NW_HANDLE_THREAD ||
        nw->kind == NW_HANDLE_FIND) {
        return FILE_TYPE_UNKNOWN;
    }
    if (fstat(nw->fd, &st) == 0) {
        if (S_ISCHR(st.st_mode)) {
            return FILE_TYPE_CHAR;
        }
        if (S_ISFIFO(st.st_mode)) {
            return FILE_TYPE_PIPE;
        }
        if (S_ISREG(st.st_mode) || S_ISBLK(st.st_mode)) {
            return FILE_TYPE_DISK;
        }
    }
    return FILE_TYPE_UNKNOWN;
}

static NwHandle *nw_new_handle(int kind)
{
    NwHandle *h = (NwHandle *)calloc(1, sizeof(*h));
    if (h != NULL) {
        h->kind = kind;
        h->fd = -1;
    }
    return h;
}

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSA, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    char path[PATH_MAX];
    int flags;
    int fd;
    NwHandle *h;
    mode_t mode = 0666;

    (void)dwShareMode;
    (void)lpSA;
    (void)hTemplateFile;

    if (lpFileName == NULL) {
        nw_last_error = 123; /* ERROR_INVALID_NAME */
        return INVALID_HANDLE_VALUE;
    }
    nw_win_to_posix(lpFileName, path, sizeof(path));

    {
        int want_read = (dwDesiredAccess &
                         (GENERIC_READ | FILE_READ_DATA)) != 0;
        int want_write = (dwDesiredAccess &
                          (GENERIC_WRITE | FILE_WRITE_DATA |
                           FILE_APPEND_DATA)) != 0;

        if (want_read && want_write) {
            flags = O_RDWR;
        } else if (want_write) {
            flags = O_WRONLY;
        } else {
            flags = O_RDONLY;
        }
        if ((dwDesiredAccess & FILE_APPEND_DATA) != 0 &&
            (dwDesiredAccess & GENERIC_WRITE) == 0) {
            flags |= O_APPEND;
        }
    }

    switch (dwCreationDisposition) {
    case CREATE_NEW:        flags |= O_CREAT | O_EXCL; break;
    case CREATE_ALWAYS:     flags |= O_CREAT | O_TRUNC; break;
    case OPEN_EXISTING:     break;
    case OPEN_ALWAYS:       flags |= O_CREAT; break;
    case TRUNCATE_EXISTING: flags |= O_TRUNC; break;
    default:                flags |= O_CREAT; break;
    }

    if (dwFlagsAndAttributes & FILE_ATTRIBUTE_READONLY) {
        mode = 0444;
    }
    fd = open(path, flags, mode);
    if (fd < 0) {
        nw_last_error = nw_errno_to_win(errno);
        return INVALID_HANDLE_VALUE;
    }
    h = nw_new_handle(NW_HANDLE_FILE);
    if (h == NULL) {
        close(fd);
        nw_last_error = 8;
        return INVALID_HANDLE_VALUE;
    }
    h->fd = fd;
    return (HANDLE)h;
}

BOOL ReadFile(HANDLE h, LPVOID lpBuffer, DWORD nBytesToRead,
              LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
    NwHandle *nw = (NwHandle *)h;
    ssize_t n;
    DWORD got = 0;

    (void)lpOverlapped;
    if (nw == NULL || nw == (NwHandle *)INVALID_HANDLE_VALUE ||
        nw->kind != NW_HANDLE_FILE) {
        nw_last_error = 6;
        return FALSE;
    }
    n = read(nw->fd, lpBuffer, nBytesToRead);
    if (n < 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    got = (DWORD)n;
    if (lpNumberOfBytesRead != NULL) {
        *lpNumberOfBytesRead = got;
    }
    return TRUE;
}

BOOL WriteFile(HANDLE h, LPCVOID lpBuffer, DWORD nBytesToWrite,
               LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
    NwHandle *nw = (NwHandle *)h;
    const char *p = (const char *)lpBuffer;
    DWORD left = nBytesToWrite;
    DWORD wrote = 0;

    (void)lpOverlapped;
    if (nw == NULL || nw == (NwHandle *)INVALID_HANDLE_VALUE ||
        nw->kind != NW_HANDLE_FILE) {
        nw_last_error = 6;
        return FALSE;
    }
    while (left > 0) {
        ssize_t n = write(nw->fd, p, left);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            nw_last_error = nw_errno_to_win(errno);
            return FALSE;
        }
        p += n;
        left -= (DWORD)n;
        wrote += (DWORD)n;
    }
    if (lpNumberOfBytesWritten != NULL) {
        *lpNumberOfBytesWritten = wrote;
    }
    return TRUE;
}

DWORD SetFilePointer(HANDLE h, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh,
                     DWORD dwMoveMethod)
{
    NwHandle *nw = (NwHandle *)h;
    off_t off;
    int whence;

    if (nw == NULL || nw == (NwHandle *)INVALID_HANDLE_VALUE ||
        nw->kind != NW_HANDLE_FILE) {
        nw_last_error = 6;
        return INVALID_SET_FILE_POINTER;
    }
    if (lpDistanceToMoveHigh != NULL) {
        off = (off_t)((((int64_t)*lpDistanceToMoveHigh) << 32) |
                       (int64_t)(uint32_t)lDistanceToMove);
    } else {
        off = (off_t)lDistanceToMove;
    }
    switch (dwMoveMethod) {
    case FILE_BEGIN:   whence = SEEK_SET; break;
    case FILE_CURRENT: whence = SEEK_CUR; break;
    case FILE_END:     whence = SEEK_END; break;
    default:           whence = SEEK_SET; break;
    }
    off = lseek(nw->fd, off, whence);
    if (off < 0) {
        nw_last_error = nw_errno_to_win(errno);
        return INVALID_SET_FILE_POINTER;
    }
    if (lpDistanceToMoveHigh != NULL) {
        *lpDistanceToMoveHigh = (LONG)(((int64_t)off) >> 32);
    }
    return (DWORD)off;
}

BOOL SetEndOfFile(HANDLE h)
{
    NwHandle *nw = (NwHandle *)h;
    off_t pos;

    if (nw == NULL || nw == (NwHandle *)INVALID_HANDLE_VALUE ||
        nw->kind != NW_HANDLE_FILE) {
        nw_last_error = 6;
        return FALSE;
    }
    pos = lseek(nw->fd, 0, SEEK_CUR);
    if (pos < 0 || ftruncate(nw->fd, pos) != 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    return TRUE;
}

DWORD GetFileSize(HANDLE h, LPDWORD lpFileSizeHigh)
{
    NwHandle *nw = (NwHandle *)h;
    struct stat st;

    if (nw == NULL || nw == (NwHandle *)INVALID_HANDLE_VALUE ||
        nw->kind != NW_HANDLE_FILE || fstat(nw->fd, &st) != 0) {
        nw_last_error = 6;
        return INVALID_FILE_SIZE;
    }
    if (lpFileSizeHigh != NULL) {
        *lpFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
    }
    return (DWORD)((uint64_t)st.st_size & 0xffffffffu);
}

BOOL GetFileSizeEx(HANDLE h, PLARGE_INTEGER lpFileSize)
{
    NwHandle *nw = (NwHandle *)h;
    struct stat st;

    if (nw == NULL || nw == (NwHandle *)INVALID_HANDLE_VALUE ||
        nw->kind != NW_HANDLE_FILE || fstat(nw->fd, &st) != 0) {
        nw_last_error = 6;
        return FALSE;
    }
    lpFileSize->QuadPart = (int64_t)st.st_size;
    return TRUE;
}

BOOL FlushFileBuffers(HANDLE h)
{
    NwHandle *nw = (NwHandle *)h;

    if (nw == NULL || nw == (NwHandle *)INVALID_HANDLE_VALUE ||
        nw->kind != NW_HANDLE_FILE) {
        nw_last_error = 6;
        return FALSE;
    }
    return fsync(nw->fd) == 0 ? TRUE : FALSE;
}

BOOL CloseHandle(HANDLE h)
{
    NwHandle *nw = (NwHandle *)h;

    if (nw == NULL || nw == (NwHandle *)INVALID_HANDLE_VALUE) {
        nw_last_error = 6;
        return FALSE;
    }
    switch (nw->kind) {
    case NW_HANDLE_FILE:
        if (!nw->standard && nw->fd >= 0) {
            close(nw->fd);
        }
        if (!nw->standard) {
            free(nw);
        }
        return TRUE;
    case NW_HANDLE_SEM:
        nw_sem_destroy(&nw->sem);
        free(nw);
        return TRUE;
    case NW_HANDLE_THREAD:
        if (!nw->detached) {
            pthread_detach(nw->thread);
        }
        free(nw);
        return TRUE;
    case NW_HANDLE_FIND:
        if (nw->dir != NULL) {
            closedir(nw->dir);
        }
        free(nw);
        return TRUE;
    default:
        free(nw);
        return TRUE;
    }
}

/* ------------------------------------------------------------------ */
/* Semaphores, threads and waits                                      */
/* ------------------------------------------------------------------ */
HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSA, LONG lInitialCount,
                        LONG lMaximumCount, LPCSTR lpName)
{
    NwHandle *h = nw_new_handle(NW_HANDLE_SEM);

    (void)lpSA;
    (void)lpName;
    if (h == NULL) {
        nw_last_error = 8;
        return NULL;
    }
    if (lMaximumCount <= 0) {
        lMaximumCount = 1;
    }
    if (lInitialCount < 0) {
        lInitialCount = 0;
    }
    if (lInitialCount > lMaximumCount) {
        lInitialCount = lMaximumCount;
    }
    if (nw_sem_init(&h->sem, lInitialCount) != 0) {
        free(h);
        nw_last_error = 8;
        return NULL;
    }
    return (HANDLE)h;
}

BOOL ReleaseSemaphore(HANDLE h, LONG lReleaseCount, LPLONG lpPreviousCount)
{
    NwHandle *nw = (NwHandle *)h;
    long value;

    if (nw == NULL || nw->kind != NW_HANDLE_SEM || lReleaseCount <= 0) {
        nw_last_error = 6;
        return FALSE;
    }
    value = nw_sem_value(&nw->sem);
    if (lpPreviousCount != NULL) {
        *lpPreviousCount = (LONG)value;
    }
    while (lReleaseCount-- > 0) {
        nw_sem_post(&nw->sem);
    }
    return TRUE;
}

DWORD WaitForSingleObject(HANDLE h, DWORD dwMilliseconds)
{
    NwHandle *nw = (NwHandle *)h;

    if (nw == NULL) {
        return WAIT_FAILED;
    }
    if (nw->kind == NW_HANDLE_SEM) {
        return nw_sem_wait(&nw->sem, dwMilliseconds);
    }
    if (nw->kind == NW_HANDLE_THREAD) {
        if (pthread_join(nw->thread, NULL) == 0) {
            nw->detached = 1;
            return WAIT_OBJECT_0;
        }
        return WAIT_FAILED;
    }
    return WAIT_OBJECT_0;
}

typedef struct NwThreadStart {
    LPTHREAD_START_ROUTINE fn;
    LPVOID arg;
} NwThreadStart;

static void *nw_thread_trampoline(void *p)
{
    NwThreadStart *s = (NwThreadStart *)p;
    DWORD rc = s->fn(s->arg);
    free(s);
    return (void *)(uintptr_t)rc;
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpSA, SIZE_T dwStackSize,
                    LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter,
                    DWORD dwCreationFlags, LPDWORD lpThreadId)
{
    NwHandle *h = nw_new_handle(NW_HANDLE_THREAD);
    NwThreadStart *s;
    pthread_attr_t attr;
    int rc;

    (void)lpSA;
    (void)dwCreationFlags;
    if (h == NULL) {
        nw_last_error = 8;
        return NULL;
    }
    s = (NwThreadStart *)malloc(sizeof(*s));
    if (s == NULL) {
        free(h);
        nw_last_error = 8;
        return NULL;
    }
    s->fn = lpStartAddress;
    s->arg = lpParameter;
    pthread_attr_init(&attr);
    if (dwStackSize != 0) {
        pthread_attr_setstacksize(&attr, dwStackSize);
    }
    rc = pthread_create(&h->thread, &attr, nw_thread_trampoline, s);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        free(s);
        free(h);
        nw_last_error = 8;
        return NULL;
    }
    if (lpThreadId != NULL) {
        *lpThreadId = (DWORD)(uintptr_t)h->thread;
    }
    return (HANDLE)h;
}

BOOL SetThreadPriority(HANDLE h, int nPriority)
{
    (void)h;
    (void)nPriority;
    return TRUE;
}

BOOL GetExitCodeThread(HANDLE h, LPDWORD lpExitCode)
{
    (void)h;
    if (lpExitCode != NULL) {
        *lpExitCode = STILL_ACTIVE;
    }
    return TRUE;
}

HANDLE GetCurrentProcess(void)
{
    static int dummy;
    return (HANDLE)&dummy;
}

HANDLE GetCurrentThread(void)
{
    static int dummy;
    return (HANDLE)&dummy;
}

#if defined(__PROSPERO__) && defined(NW_PS5_NATIVE_TITLE)
/*
 * A native title must not use the process exit syscall: the console reports
 * that as a crash even after a successful run.  platform.c's strong
 * catchReturnFromMain hands the close request to the shell and never returns.
 */
extern void catchReturnFromMain(int status);

static void __attribute__((noreturn))
nw_exit_now(int status)
{
    catchReturnFromMain(status);
    for (;;) {
    }
}
#else
static void __attribute__((noreturn))
nw_exit_now(int status)
{
    exit(status);
}
#endif

BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode)
{
    (void)hProcess;
    nw_exit_now((int)uExitCode);
    return TRUE;
}

void ExitProcess(UINT uExitCode)
{
    nw_exit_now((int)uExitCode);
}

/* ------------------------------------------------------------------ */
/* Paths and files                                                    */
/* ------------------------------------------------------------------ */
BOOL DeleteFileA(LPCSTR lpFileName)
{
    char path[PATH_MAX];
    if (lpFileName == NULL) {
        nw_last_error = 123;
        return FALSE;
    }
    nw_win_to_posix(lpFileName, path, sizeof(path));
    if (unlink(path) != 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    return TRUE;
}

BOOL MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName)
{
    char a[PATH_MAX], b[PATH_MAX];
    if (lpExistingFileName == NULL || lpNewFileName == NULL) {
        nw_last_error = 123;
        return FALSE;
    }
    nw_win_to_posix(lpExistingFileName, a, sizeof(a));
    nw_win_to_posix(lpNewFileName, b, sizeof(b));
    if (rename(a, b) != 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    return TRUE;
}

BOOL CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists)
{
    char a[PATH_MAX], b[PATH_MAX];
    int in, out;
    char buf[65536];
    ssize_t n;

    if (lpExistingFileName == NULL || lpNewFileName == NULL) {
        nw_last_error = 123;
        return FALSE;
    }
    nw_win_to_posix(lpExistingFileName, a, sizeof(a));
    nw_win_to_posix(lpNewFileName, b, sizeof(b));
    in = open(a, O_RDONLY);
    if (in < 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    out = open(b, O_WRONLY | O_CREAT | (bFailIfExists ? O_EXCL : O_TRUNC), 0666);
    if (out < 0) {
        close(in);
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        char *p = buf;
        ssize_t left = n;
        while (left > 0) {
            ssize_t w = write(out, p, (size_t)left);
            if (w < 0) {
                close(in);
                close(out);
                nw_last_error = nw_errno_to_win(errno);
                return FALSE;
            }
            p += w;
            left -= w;
        }
    }
    close(in);
    close(out);
    if (n < 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    return TRUE;
}

static int nw_mkdir_p(const char *path)
{
    char tmp[PATH_MAX];
    size_t len;
    size_t i;

    if (path == NULL || path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[--len] = '\0';
    }
    for (i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (mkdir(tmp, 0777) != 0 && errno != EEXIST) {
                return -1;
            }
            tmp[i] = '/';
        }
    }
    if (mkdir(tmp, 0777) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSA)
{
    char path[PATH_MAX];
    (void)lpSA;
    if (lpPathName == NULL) {
        nw_last_error = 123;
        return FALSE;
    }
    nw_win_to_posix(lpPathName, path, sizeof(path));
    if (nw_mkdir_p(path) != 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    return TRUE;
}

BOOL RemoveDirectoryA(LPCSTR lpPathName)
{
    char path[PATH_MAX];
    if (lpPathName == NULL) {
        nw_last_error = 123;
        return FALSE;
    }
    nw_win_to_posix(lpPathName, path, sizeof(path));
    if (rmdir(path) != 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    return TRUE;
}

DWORD GetFileAttributesA(LPCSTR lpFileName)
{
    char path[PATH_MAX];
    struct stat st;
    DWORD attr = 0;

    if (lpFileName == NULL) {
        nw_last_error = 123;
        return INVALID_FILE_ATTRIBUTES;
    }
    nw_win_to_posix(lpFileName, path, sizeof(path));
    if (stat(path, &st) != 0) {
        nw_last_error = nw_errno_to_win(errno);
        return INVALID_FILE_ATTRIBUTES;
    }
    if (S_ISDIR(st.st_mode)) {
        attr |= FILE_ATTRIBUTE_DIRECTORY;
    } else {
        attr |= FILE_ATTRIBUTE_ARCHIVE;
    }
    if ((st.st_mode & 0222) == 0) {
        attr |= FILE_ATTRIBUTE_READONLY;
    }
    if (attr == 0) {
        attr = FILE_ATTRIBUTE_NORMAL;
    }
    return attr;
}

BOOL SetFileAttributesA(LPCSTR lpFileName, DWORD dwFileAttributes)
{
    char path[PATH_MAX];
    struct stat st;
    mode_t mode;

    if (lpFileName == NULL) {
        nw_last_error = 123;
        return FALSE;
    }
    nw_win_to_posix(lpFileName, path, sizeof(path));
    if (stat(path, &st) != 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    mode = st.st_mode;
    if (dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
        mode &= ~0222;
    } else {
        mode |= 0200;
    }
    if (chmod(path, mode) != 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    return TRUE;
}

DWORD GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength,
                       LPSTR lpBuffer, LPSTR *lpFilePart)
{
    char path[PATH_MAX];
    char full[PATH_MAX];
    char win[PATH_MAX + 8];
    char *slash;
    DWORD needed;

    if (lpFileName == NULL) {
        nw_last_error = 123;
        return 0;
    }
    nw_win_to_posix(lpFileName, path, sizeof(path));
    if (realpath(path, full) == NULL) {
        if (path[0] == '/') {
            snprintf(full, sizeof(full), "%s", path);
        } else {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                strcpy(cwd, ".");
            }
            snprintf(full, sizeof(full), "%s/%s", cwd, path);
        }
    }
    nw_posix_to_win(full, win, sizeof(win));
    needed = (DWORD)strlen(win) + 1;
    if (nBufferLength < needed) {
        return needed;
    }
    strcpy(lpBuffer, win);
    if (lpFilePart != NULL) {
        slash = strrchr(lpBuffer, '\\');
        *lpFilePart = slash != NULL ? slash + 1 : lpBuffer;
    }
    return needed - 1;
}

DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer)
{
    char cwd[PATH_MAX];
    char win[PATH_MAX + 8];
    DWORD needed;

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        nw_last_error = nw_errno_to_win(errno);
        return 0;
    }
    nw_posix_to_win(cwd, win, sizeof(win));
    needed = (DWORD)strlen(win) + 1;
    if (nBufferLength < needed) {
        return needed;
    }
    strcpy(lpBuffer, win);
    return needed - 1;
}

BOOL SetCurrentDirectoryA(LPCSTR lpPathName)
{
    char path[PATH_MAX];
    if (lpPathName == NULL) {
        nw_last_error = 123;
        return FALSE;
    }
    nw_win_to_posix(lpPathName, path, sizeof(path));
    if (chdir(path) != 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    return TRUE;
}

DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
    DWORD len;

    (void)hModule;
    nw_init_module_path();
    len = (DWORD)strlen(g_module_path_win);
    if (nSize == 0) {
        return 0;
    }
    if (nSize <= len) {
        if (nSize > 1) {
            memcpy(lpFilename, g_module_path_win, nSize - 1);
            lpFilename[nSize - 1] = '\0';
        }
        nw_last_error = 122; /* ERROR_INSUFFICIENT_BUFFER */
        return nSize;
    }
    memcpy(lpFilename, g_module_path_win, len + 1);
    return len;
}

HMODULE GetModuleHandleA(LPCSTR lpModuleName)
{
    (void)lpModuleName;
    nw_init_module_path();
    return (HMODULE)(uintptr_t)0x00400000;
}

DWORD GetTempPathA(DWORD nBufferLength, LPSTR lpBuffer)
{
    const char *tmp;
#if defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    char appTemp[PATH_MAX + 8];
#endif

#if defined(__PROSPERO__) && defined(NW_PS5_NATIVE_TITLE)
    /* A native title's /app0 is read-only.  Its writable data mount is
     * /download0 (param.json downloadDataSize); /tmp is not guaranteed to be
     * writable in that launch context.  The game creates its save variant in
     * this directory when the module-relative open fails. */
    tmp = "Z:\\download0\\";
#elif defined(__PROSPERO__) && defined(NW_PORTABLE_PS5)
    /* The payload's module directory is /app0, which is read-only.  /data is
     * the persistent writable mount (the boot log already lives there), so a
     * repaired save survives a reboot instead of landing in /tmp. */
    tmp = "Z:\\data\\";
#elif defined(__ANDROID__) || defined(NW_PORTABLE_IOS)
    /* Android and iOS have no writable /tmp shared with the app.  Use the
     * app directory installed by the platform entry point (internal storage
     * on Android, SDL_GetPrefPath on iOS). */
    nw_init_module_path();
    (void)snprintf(appTemp, sizeof(appTemp), "%s", g_module_path_win);
    {
        char *slash = strrchr(appTemp, '\\');
        if (slash != NULL)
            slash[1] = '\0';
        else
            appTemp[0] = '\0';
    }
    tmp = appTemp[0] != '\0' ? appTemp : "Z:\\";
#else
    tmp = "Z:\\tmp\\";
#endif
    DWORD needed = (DWORD)strlen(tmp) + 1;

    if (nBufferLength < needed) {
        return needed;
    }
    strcpy(lpBuffer, tmp);
    return needed - 1;
}

UINT GetTempFileNameA(LPCSTR lpPathName, LPCSTR lpPrefixString, UINT uUnique,
                      LPSTR lpTempFileName)
{
    char path[PATH_MAX];
    char prefix[16];
    char name[PATH_MAX];
    unsigned first = uUnique & 0xffffu;
    unsigned tries = (uUnique == 0u) ? 0x10000u : 1u;
    unsigned i;

    if (lpPathName == NULL) {
        return 0;
    }
    nw_win_to_posix(lpPathName, path, sizeof(path));
    snprintf(prefix, sizeof(prefix), "%s", lpPrefixString != NULL ? lpPrefixString : "NW");
    for (i = 0; i < tries; i++) {
        unsigned id = (first + i) & 0xffffu;
        int fd;

        snprintf(name, sizeof(name), "%s/%s%04x.tmp", path, prefix, id);
        fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0666);
        if (fd >= 0) {
            close(fd);
            nw_posix_to_win(name, lpTempFileName, PATH_MAX);
            return uUnique == 0u ? (UINT)(id + 1u) : uUnique;
        }
        if (errno != EEXIST) {
            break;
        }
    }
    nw_last_error = nw_errno_to_win(errno);
    return 0;
}

DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize)
{
    const char *value;
    DWORD len;

    if (lpName == NULL) {
        return 0;
    }
    value = getenv(lpName);
    if (value == NULL) {
        return 0;
    }
    len = (DWORD)strlen(value);
    if (nSize <= len) {
        return len + 1;
    }
    strcpy(lpBuffer, value);
    return len;
}

LPSTR GetCommandLineA(void)
{
    return g_command_line;
}

LPSTR GetEnvironmentStringsA(void)
{
    size_t total = 0;
    char **e;
    char *block;
    char *p;

    for (e = environ; e != NULL && *e != NULL; e++) {
        total += strlen(*e) + 1;
    }
    block = (char *)malloc(total + 1);
    if (block == NULL) {
        return NULL;
    }
    p = block;
    for (e = environ; e != NULL && *e != NULL; e++) {
        size_t n = strlen(*e) + 1;
        memcpy(p, *e, n);
        p += n;
    }
    *p = '\0';
    return block;
}

BOOL FreeEnvironmentStringsA(LPSTR block)
{
    free(block);
    return TRUE;
}

LPSTR WinGetEnvironmentStringsA(void)
{
    return GetEnvironmentStringsA();
}

UINT GetWindowsDirectoryA(LPSTR lpBuffer, UINT uSize)
{
    const char *dir = "Z:\\windows";
    UINT len = (UINT)strlen(dir);
    if (uSize <= len) {
        return len + 1;
    }
    strcpy(lpBuffer, dir);
    return len;
}

UINT GetSystemDirectoryA(LPSTR lpBuffer, UINT uSize)
{
    const char *dir = "Z:\\windows\\system32";
    UINT len = (UINT)strlen(dir);
    if (uSize <= len) {
        return len + 1;
    }
    strcpy(lpBuffer, dir);
    return len;
}

UINT GetDriveTypeA(LPCSTR lpRootPathName)
{
    (void)lpRootPathName;
    return DRIVE_FIXED;
}

DWORD GetLogicalDrives(void)
{
    return 1u; /* C: only */
}

DWORD GetLogicalDriveStringsA(DWORD nBufferLength, LPSTR lpBuffer)
{
    const char *drives = "C:\\\0";
    if (nBufferLength < 5) {
        return 5;
    }
    memcpy(lpBuffer, drives, 5);
    return 4;
}

/* ------------------------------------------------------------------ */
/* Directory enumeration                                              */
/* ------------------------------------------------------------------ */
static void nw_fill_find_data(const char *path, const char *name,
                              LPWIN32_FIND_DATAA data)
{
    char full[PATH_MAX];
    struct stat st;

    memset(data, 0, sizeof(*data));
    snprintf(full, sizeof(full), "%s/%s", path, name);
    if (stat(full, &st) == 0) {
        data->dwFileAttributes = S_ISDIR(st.st_mode)
                                     ? FILE_ATTRIBUTE_DIRECTORY
                                     : FILE_ATTRIBUTE_ARCHIVE;
        data->nFileSizeLow = (DWORD)((uint64_t)st.st_size & 0xffffffffu);
        data->nFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
    }
    snprintf(data->cFileName, sizeof(data->cFileName), "%s", name);
}

static BOOL nw_find_next(NwHandle *h, LPWIN32_FIND_DATAA data)
{
    struct dirent *de;
    const char *pattern = h->find_pattern;

    if (h->dir == NULL) {
        return FALSE;
    }
    if (strcmp(pattern, "*.*") == 0 || strcmp(pattern, "*") == 0) {
        pattern = "*";
    }
    while ((de = readdir(h->dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        if (fnmatch(pattern, de->d_name, 0) != 0) {
            continue;
        }
        nw_fill_find_data(h->find_path, de->d_name, data);
        return TRUE;
    }
    return FALSE;
}

HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
    char path[PATH_MAX];
    char *slash;
    const char *base;
    NwHandle *h;

    if (lpFileName == NULL || lpFindFileData == NULL) {
        nw_last_error = 123;
        return INVALID_HANDLE_VALUE;
    }
    nw_win_to_posix(lpFileName, path, sizeof(path));
    slash = strrchr(path, '/');
    if (slash == NULL) {
        strcpy(path, ".");
        base = path;
    } else {
        *slash = '\0';
        base = slash + 1;
    }
    h = nw_new_handle(NW_HANDLE_FIND);
    if (h == NULL) {
        nw_last_error = 8;
        return INVALID_HANDLE_VALUE;
    }
    snprintf(h->find_path, sizeof(h->find_path), "%s", path[0] ? path : ".");
    snprintf(h->find_pattern, sizeof(h->find_pattern), "%s", base);
    h->dir = opendir(h->find_path);
    if (h->dir == NULL) {
        nw_last_error = nw_errno_to_win(errno);
        free(h);
        return INVALID_HANDLE_VALUE;
    }
    if (!nw_find_next(h, lpFindFileData)) {
        closedir(h->dir);
        free(h);
        nw_last_error = 18; /* ERROR_NO_MORE_FILES */
        return INVALID_HANDLE_VALUE;
    }
    return (HANDLE)h;
}

BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
    NwHandle *h = (NwHandle *)hFindFile;
    if (h == NULL || h->kind != NW_HANDLE_FIND) {
        nw_last_error = 6;
        return FALSE;
    }
    if (!nw_find_next(h, lpFindFileData)) {
        nw_last_error = 18;
        return FALSE;
    }
    return TRUE;
}

BOOL FindClose(HANDLE hFindFile)
{
    NwHandle *h = (NwHandle *)hFindFile;
    if (h == NULL || h->kind != NW_HANDLE_FIND) {
        nw_last_error = 6;
        return FALSE;
    }
    if (h->dir != NULL) {
        closedir(h->dir);
    }
    free(h);
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Global memory, virtual memory, atomics                             */
/* ------------------------------------------------------------------ */
HGLOBAL GlobalAlloc(UINT uFlags, SIZE_T dwBytes)
{
    void *p = malloc(dwBytes);
    if (p != NULL && (uFlags & GMEM_ZEROINIT)) {
        memset(p, 0, dwBytes);
    }
    return (HGLOBAL)p;
}

HGLOBAL GlobalFree(HGLOBAL hMem)
{
    free(hMem);
    return NULL;
}

HGLOBAL GlobalReAlloc(HGLOBAL hMem, SIZE_T dwBytes, UINT uFlags)
{
    void *p = realloc(hMem, dwBytes);
    if (p != NULL && (uFlags & GMEM_ZEROINIT)) {
        /* The old size is unknown; Win32 zeroes the new tail. */
        memset((char *)p, 0, dwBytes);
    }
    return (HGLOBAL)p;
}

LPVOID GlobalLock(HGLOBAL hMem)
{
    return hMem;
}

BOOL GlobalUnlock(HGLOBAL hMem)
{
    (void)hMem;
    return TRUE;
}

SIZE_T GlobalSize(HGLOBAL hMem)
{
    (void)hMem;
    return 0;
}

HLOCAL LocalAlloc(UINT uFlags, SIZE_T dwBytes)
{
    return (HLOCAL)GlobalAlloc(uFlags, dwBytes);
}

HLOCAL LocalFree(HLOCAL hMem)
{
    free(hMem);
    return NULL;
}

#if defined(__x86_64__) || defined(__aarch64__)
#if defined(__PROSPERO__)
static void
nw_alloc_log(const char *tag, uintptr_t hint, void *p, size_t size)
{
    char line[192];
    int n = snprintf(line, sizeof(line),
                     "nw_mmap_low: %s hint=0x%lx p=%p size=%lu errno=%d\n",
                     tag, (unsigned long)hint, p, (unsigned long)size, errno);
    if (n > 0) {
        size_t len = (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1;
        (void)write(7, line, len);
        (void)write(2, line, len);
    }
}
#else
#define nw_alloc_log(tag, hint, p, size) ((void)0)
#endif

/*
 * The recovered state still stores guest pointers in 32-bit slots.  MAP_32BIT
 * is only a hint on some kernels (notably the PS5's), so verify the result and
 * fall back to explicit low-address scans.  The hint-only pass is safe and
 * works when the kernel honors a concrete low hint; the fixed pass then finds
 * a free hole when the low range is fragmented by the runtime heap.  A future
 * guest arena removes this dependency; it is not part of the i686 target.
 */
static void *nw_mmap_low(size_t size, int prot)
{
    void *p;
    int saved_errno;

#if defined(MAP_32BIT)
    p = mmap(NULL, size, prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (p != MAP_FAILED && (uintptr_t)p < UINT64_C(0x100000000)) {
        nw_alloc_log("32bit", 0, p, size);
        return p;
    }
    if (p != MAP_FAILED) {
        saved_errno = errno;
        (void)munmap(p, size);
        errno = saved_errno;
    }
#endif

    {
        uintptr_t hint;
        for (hint = UINT64_C(0x10000000);
             hint < UINT64_C(0x80000000);
             hint += UINT64_C(0x1000000)) {
            p = mmap((void *)hint, size, prot,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (p != MAP_FAILED && (uintptr_t)p < UINT64_C(0x100000000)) {
                nw_alloc_log("hint", hint, p, size);
                return p;
            }
            if (p != MAP_FAILED) {
                saved_errno = errno;
                (void)munmap(p, size);
                errno = saved_errno;
            }
        }
    }

    {
        uintptr_t hint;
        for (hint = UINT64_C(0x10000000);
             hint < UINT64_C(0x80000000);
             hint += UINT64_C(0x10000)) {
            p = mmap((void *)hint, size, prot,
                     MAP_PRIVATE | MAP_ANONYMOUS | NW_MAP_FIXED_NOREPLACE,
                     -1, 0);
            if (p != MAP_FAILED && (uintptr_t)p < UINT64_C(0x100000000)) {
                nw_alloc_log("fixed", hint, p, size);
                return p;
            }
            if (p != MAP_FAILED) {
                saved_errno = errno;
                (void)munmap(p, size);
                errno = saved_errno;
            }
        }
    }

    nw_alloc_log("fail", 0, MAP_FAILED, size);
    return MAP_FAILED;
}
#endif

LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType,
                    DWORD flProtect)
{
    int prot = PROT_READ | PROT_WRITE;
#if !defined(NW_PORTABLE_IOS)
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#endif
    void *p;

    (void)flAllocationType;
    if (flProtect == PAGE_EXECUTE_READ || flProtect == PAGE_EXECUTE_READWRITE) {
        prot |= PROT_EXEC;
    } else if (flProtect == PAGE_READONLY) {
        prot = PROT_READ;
    } else if (flProtect == PAGE_NOACCESS) {
        prot = PROT_NONE;
    }
#if defined(NW_PORTABLE_IOS)
    /* Two layouts share this entry point:
     *
     *   nw_guest_base == 0  every guest object is below 4 GiB (an explicitly
     *                       low game link) — use the Mach VM low scan.
     *   nw_guest_base != 0   the 64-bit guest window above 4 GiB — scan that
     *                       window so the static data window, heap, and
     *                       framebuffer share one 32-bit-addressable space.
     */
    if (lpAddress != NULL) {
        p = NwIosAllocLowAt(lpAddress, dwSize, prot);
    } else if (nw_guest_base != 0) {
        p = NwIosAllocWindow(nw_guest_base, dwSize, prot);
    } else {
        p = NwIosAllocLow(dwSize, prot);
    }
    if (p == NULL) {
        nw_last_error = nw_errno_to_win(errno);
        return NULL;
    }
    return p;
#else
    if (lpAddress != NULL) {
        flags |= NW_MAP_FIXED_NOREPLACE;
    }
#if defined(__x86_64__) || defined(__aarch64__)
    if (lpAddress == NULL) {
        p = nw_mmap_low(dwSize, prot);
    } else {
        p = mmap(lpAddress, dwSize, prot, flags, -1, 0);
    }
#else
    p = mmap(lpAddress, dwSize, prot, flags, -1, 0);
#endif
    if (p == MAP_FAILED) {
        nw_last_error = nw_errno_to_win(errno);
        return NULL;
    }
    return p;
#endif
}

BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType)
{
    (void)dwFreeType;
    if (lpAddress == NULL) {
        return FALSE;
    }
#if defined(NW_PORTABLE_IOS)
    /* dwSize is zero for MEM_RELEASE; the allocator tracks the rounded size. */
    NwIosFreeLow(lpAddress, dwSize);
    return TRUE;
#else
    if (munmap(lpAddress, dwSize) != 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
    return TRUE;
#endif
}

/* Large host-side buffers (the SDL framebuffer and the game's scratch
 * buffer) must remain addressable through the recovered 32-bit pointer
 * mirrors.  This uses the same low-address mmap path as the guest heap on
 * 64-bit hosts.  Native-only callers use it; the desktop allocator path is
 * unchanged. */
void *NwHostAllocLow(size_t size)
{
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void NwHostFreeLow(void *p, size_t size)
{
#if defined(NW_PORTABLE_IOS)
    NwIosFreeLow(p, size);
#else
    if (p != NULL)
        (void)munmap(p, size);
#endif
}

BOOL VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect,
                    PDWORD lpflOldProtect)
{
    int prot = PROT_READ | PROT_WRITE;

    if (lpflOldProtect != NULL) {
        *lpflOldProtect = PAGE_READWRITE;
    }
    if (flNewProtect == PAGE_EXECUTE_READ ||
        flNewProtect == PAGE_EXECUTE_READWRITE) {
        prot |= PROT_EXEC;
    } else if (flNewProtect == PAGE_READONLY) {
        prot = PROT_READ;
    } else if (flNewProtect == PAGE_NOACCESS) {
        prot = PROT_NONE;
    }
#if defined(NW_PORTABLE_IOS)
    if (NwIosProtectLow(lpAddress, dwSize, prot) != 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
#else
    if (mprotect(lpAddress, dwSize, prot) != 0) {
        nw_last_error = nw_errno_to_win(errno);
        return FALSE;
    }
#endif
    return TRUE;
}

SIZE_T VirtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer,
                    SIZE_T dwLength)
{
    if (lpBuffer == NULL || dwLength < sizeof(*lpBuffer)) {
        return 0;
    }
    memset(lpBuffer, 0, sizeof(*lpBuffer));
    lpBuffer->BaseAddress = (PVOID)lpAddress;
    lpBuffer->AllocationBase = (PVOID)lpAddress;
    lpBuffer->AllocationProtect = PAGE_READWRITE;
    lpBuffer->RegionSize = 0x1000;
    lpBuffer->State = MEM_COMMIT;
    lpBuffer->Protect = PAGE_READWRITE;
    lpBuffer->Type = MEM_PRIVATE;
    return sizeof(*lpBuffer);
}

LONG InterlockedIncrement(volatile LONG *p)
{
    return __sync_add_and_fetch(p, 1);
}

LONG InterlockedDecrement(volatile LONG *p)
{
    return __sync_sub_and_fetch(p, 1);
}

LONG InterlockedExchange(volatile LONG *p, LONG v)
{
    return __sync_lock_test_and_set(p, v);
}

LONG InterlockedCompareExchange(volatile LONG *p, LONG v, LONG cmp)
{
    return __sync_val_compare_and_swap(p, cmp, v);
}

/* ------------------------------------------------------------------ */
/* Clocks and calendars                                               */
/* ------------------------------------------------------------------ */
static uint64_t nw_monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

DWORD GetTickCount(void)
{
    return (DWORD)nw_monotonic_ms();
}

DWORD timeGetTime(void)
{
    return (DWORD)nw_monotonic_ms();
}

void Sleep(DWORD dwMilliseconds)
{
    struct timespec ts;
    ts.tv_sec = dwMilliseconds / 1000;
    ts.tv_nsec = (long)(dwMilliseconds % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
    }
}

void GetLocalTime(LPSYSTEMTIME st)
{
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    st->wYear = (WORD)(tmv.tm_year + 1900);
    st->wMonth = (WORD)(tmv.tm_mon + 1);
    st->wDayOfWeek = (WORD)tmv.tm_wday;
    st->wDay = (WORD)tmv.tm_mday;
    st->wHour = (WORD)tmv.tm_hour;
    st->wMinute = (WORD)tmv.tm_min;
    st->wSecond = (WORD)tmv.tm_sec;
    st->wMilliseconds = (WORD)(nw_monotonic_ms() % 1000u);
}

void GetSystemTime(LPSYSTEMTIME st)
{
    time_t t = time(NULL);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    st->wYear = (WORD)(tmv.tm_year + 1900);
    st->wMonth = (WORD)(tmv.tm_mon + 1);
    st->wDayOfWeek = (WORD)tmv.tm_wday;
    st->wDay = (WORD)tmv.tm_mday;
    st->wHour = (WORD)tmv.tm_hour;
    st->wMinute = (WORD)tmv.tm_min;
    st->wSecond = (WORD)tmv.tm_sec;
    st->wMilliseconds = (WORD)(nw_monotonic_ms() % 1000u);
}

DWORD GetTimeZoneInformation(LPTIME_ZONE_INFORMATION tz)
{
    time_t t = time(NULL);
    struct tm tmv;
    long gmtoff = 0;
    int i;

    memset(tz, 0, sizeof(*tz));
    localtime_r(&t, &tmv);
#if defined(__USE_MISC) || defined(__USE_GNU)
    gmtoff = tmv.tm_gmtoff;
#endif
    tz->Bias = (LONG)(-gmtoff / 60);
    for (i = 0; i < 31 && i < (int)(sizeof("UTC") - 1); i++) {
        tz->StandardName[i] = (WCHAR)"UTC"[i];
    }
    return 0; /* TIME_ZONE_ID_UNKNOWN */
}

static int64_t nw_days_from_civil(int y, unsigned m, unsigned d)
{
    int64_t era;
    unsigned yoe, doy, doe;

    y -= (m <= 2);
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153u * (m + (m > 2 ? -3 : 9)) + 2u) / 5u + d - 1u;
    doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

BOOL SystemTimeToFileTime(const SYSTEMTIME *st, LPFILETIME ft)
{
    int64_t days = nw_days_from_civil(st->wYear, st->wMonth, st->wDay);
    int64_t secs = days * 86400 + st->wHour * 3600 + st->wMinute * 60 +
                   st->wSecond;
    int64_t ticks = (secs + 11644473600LL) * 10000000LL +
                    (int64_t)st->wMilliseconds * 10000LL;

    ft->dwLowDateTime = (DWORD)((uint64_t)ticks & 0xffffffffu);
    ft->dwHighDateTime = (DWORD)((uint64_t)ticks >> 32);
    return TRUE;
}

BOOL FileTimeToSystemTime(const FILETIME *ft, LPSYSTEMTIME st)
{
    int64_t ticks = ((int64_t)(uint32_t)ft->dwHighDateTime << 32) |
                    (uint32_t)ft->dwLowDateTime;
    int64_t secs = ticks / 10000000LL - 11644473600LL;
    int64_t days = secs / 86400;
    int64_t rem = secs % 86400;
    /* Civil-from-days, Howard Hinnant's algorithm. */
    int64_t z = days + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = (int)(yoe + era * 400);
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp + (mp < 10 ? 3 : -9);

    st->wYear = (WORD)(y + (m <= 2));
    st->wMonth = (WORD)m;
    st->wDay = (WORD)d;
    st->wDayOfWeek = (WORD)((days + 4) % 7);
    st->wHour = (WORD)(rem / 3600);
    st->wMinute = (WORD)((rem % 3600) / 60);
    st->wSecond = (WORD)(rem % 60);
    st->wMilliseconds = (WORD)((ticks / 10000) % 1000);
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Multimedia timers                                                  */
/* ------------------------------------------------------------------ */
#define NW_MAX_TIMERS 32

typedef void (*NwTimerCallback)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

typedef struct NwTimer {
    int used;
    UINT id;
    UINT delay;
    UINT flags;
    DWORD_PTR user;
    NwTimerCallback cb;
    volatile int active;
    volatile int kill_pending;
    volatile int callback_running;
    pthread_t thread;
} NwTimer;

static NwTimer g_timers[NW_MAX_TIMERS];
static pthread_mutex_t g_timer_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_timer_cond = PTHREAD_COND_INITIALIZER;
static UINT g_next_timer_id = 1;

/* The MIDI sequencer is stopped by killing its timer and then resetting the
 * synth.  A timer callback already in flight must not emit a note-on after
 * that reset, or the note has no later note-off and rings until the next
 * level/music reload.  timeKillEvent therefore waits for the callback to
 * finish before returning.  __thread lets a callback that kills its own timer
 * avoid joining itself. */
static __thread NwTimer *g_tls_timer;

static void nw_timer_deadline(struct timespec *ts, UINT delay)
{
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += (time_t)(delay / 1000u);
    ts->tv_nsec += (long)(delay % 1000u) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

static void *nw_timer_thread(void *arg)
{
    NwTimer *t = (NwTimer *)arg;
    UINT my_id = t->id;
    struct timespec ts;

    g_tls_timer = t;
    for (;;) {
        int fire = 0;

        pthread_mutex_lock(&g_timer_lock);
        while (t->active && t->used && t->id == my_id) {
            nw_timer_deadline(&ts, t->delay);
            if (pthread_cond_timedwait(&g_timer_cond, &g_timer_lock, &ts) ==
                ETIMEDOUT) {
                fire = 1;
                break;
            }
        }
        if (!t->active || !t->used || t->id != my_id) {
            pthread_mutex_unlock(&g_timer_lock);
            break;
        }
        if (fire) {
            t->callback_running = 1;
            pthread_mutex_unlock(&g_timer_lock);

            t->cb(t->id, 0, t->user, 0, 0);

            pthread_mutex_lock(&g_timer_lock);
            t->callback_running = 0;
            if (t->flags == TIME_ONESHOT)
                t->active = 0;
        }
        if (!t->active) {
            pthread_mutex_unlock(&g_timer_lock);
            break;
        }
        pthread_mutex_unlock(&g_timer_lock);
    }

    pthread_mutex_lock(&g_timer_lock);
    t->callback_running = 0;
    /* An external killer owns the slot until it has joined this thread. */
    if (t->used && t->id == my_id && !t->kill_pending) {
        t->used = 0;
        t->active = 0;
    }
    pthread_cond_broadcast(&g_timer_cond);
    pthread_mutex_unlock(&g_timer_lock);
    g_tls_timer = 0;
    return NULL;
}

UINT_PTR timeSetEvent(UINT uDelay, UINT uResolution, void *lpTimeProc,
                      DWORD_PTR dwUser, UINT fuEvent)
{
    int i;
    NwTimer *t = NULL;

    (void)uResolution;
    if (uDelay == 0) {
        uDelay = 1;
    }
    pthread_mutex_lock(&g_timer_lock);
    for (i = 0; i < NW_MAX_TIMERS; i++) {
        if (!g_timers[i].used) {
            t = &g_timers[i];
            break;
        }
    }
    if (t == NULL) {
        pthread_mutex_unlock(&g_timer_lock);
        return 0;
    }
    t->used = 1;
    t->id = g_next_timer_id++;
    if (g_next_timer_id == 0u)
        g_next_timer_id = 1u;
    t->delay = uDelay;
    t->flags = fuEvent;
    t->user = dwUser;
    t->cb = (NwTimerCallback)lpTimeProc;
    t->kill_pending = 0;
    t->callback_running = 0;
    t->active = 1;
    if (pthread_create(&t->thread, NULL, nw_timer_thread, t) != 0) {
        t->used = 0;
        t->active = 0;
        pthread_mutex_unlock(&g_timer_lock);
        return 0;
    }
    /* Keep the thread joinable: timeKillEvent joins it before the slot can be
     * reused.  Detaching here would make the reset race unfixable. */
    pthread_mutex_unlock(&g_timer_lock);
    return t->id;
}

MMRESULT timeKillEvent(UINT uTimerID)
{
    int i;
    NwTimer *t = NULL;

    pthread_mutex_lock(&g_timer_lock);
    for (i = 0; i < NW_MAX_TIMERS; i++) {
        if (g_timers[i].used && g_timers[i].id == uTimerID) {
            t = &g_timers[i];
            break;
        }
    }
    if (t == NULL) {
        pthread_mutex_unlock(&g_timer_lock);
        return TIMERR_NOCANDO;
    }

    if (g_tls_timer == t) {
        /* Called from this timer's own callback.  It cannot join itself; the
         * callback returns to nw_timer_thread, which releases the slot. */
        t->active = 0;
        pthread_cond_broadcast(&g_timer_cond);
        pthread_mutex_unlock(&g_timer_lock);
        return TIMERR_NOERROR;
    }

    if (t->kill_pending) {
        /* Another thread is already joining this timer.  Wait for it rather
         * than issuing a second pthread_join. */
        while (t->used && t->id == uTimerID && t->kill_pending)
            pthread_cond_wait(&g_timer_cond, &g_timer_lock);
        pthread_mutex_unlock(&g_timer_lock);
        return TIMERR_NOERROR;
    }

    t->active = 0;
    t->kill_pending = 1;
    pthread_cond_broadcast(&g_timer_cond);
    pthread_mutex_unlock(&g_timer_lock);

    (void)pthread_join(t->thread, NULL);

    pthread_mutex_lock(&g_timer_lock);
    if (t->used && t->id == uTimerID) {
        t->used = 0;
        t->active = 0;
        t->kill_pending = 0;
    }
    pthread_cond_broadcast(&g_timer_cond);
    pthread_mutex_unlock(&g_timer_lock);
    return TIMERR_NOERROR;
}

MMRESULT timeGetDevCaps(LPTIMECAPS ptc, UINT cbtc)
{
    /* The recovered code passes 8, the Win9x TIMECAPS size (two UINTs).  The
     * compatibility header also has dwCapabilities, so compare against the
     * size the caller actually promised instead of sizeof(TIMECAPS). */
    if (ptc == NULL || cbtc < 2u * sizeof(UINT)) {
        return TIMERR_STRUCT;
    }
    ptc->wPeriodMin = 1;
    ptc->wPeriodMax = 1000;
    if (cbtc >= sizeof(*ptc))
        ptc->dwCapabilities = 0;
    return TIMERR_NOERROR;
}

MMRESULT timeBeginPeriod(UINT uPeriod)
{
    (void)uPeriod;
    return TIMERR_NOERROR;
}

MMRESULT timeEndPeriod(UINT uPeriod)
{
    (void)uPeriod;
    return TIMERR_NOERROR;
}

/* ------------------------------------------------------------------ */
/* MCI / waveOut / MIDI compatibility no-ops                          */
/* ------------------------------------------------------------------ */
MMRESULT mciSendCommandA(MCIDEVICEID mciId, UINT uMsg, DWORD_PTR dwParam1,
                         DWORD_PTR dwParam2)
{
    (void)mciId;
    (void)uMsg;
    (void)dwParam1;
    (void)dwParam2;
    return MCIERR_UNSUPPORTED_FUNCTION;
}

MMRESULT mciGetErrorStringA(MMRESULT mcierr, LPSTR pszText, UINT cchText)
{
    const char *msg = "MCI is not available in the native Linux build";
    if (pszText == NULL || cchText == 0) {
        return MMSYSERR_INVALPARAM;
    }
    snprintf(pszText, cchText, "%s", msg);
    return MMSYSERR_NOERROR;
}

MMRESULT midiOutOpen(HMIDIOUT *phmo, UINT uDeviceID, DWORD_PTR dwCallback,
                     DWORD_PTR dwInstance, DWORD dwFlags)
{
    (void)phmo;
    (void)uDeviceID;
    (void)dwCallback;
    (void)dwInstance;
    (void)dwFlags;
    return MMSYSERR_NODRIVER;
}

MMRESULT midiOutClose(HMIDIOUT hmo)
{
    (void)hmo;
    return MMSYSERR_NOERROR;
}

MMRESULT midiOutShortMsg(HMIDIOUT hmo, DWORD dwMsg)
{
    (void)hmo;
    (void)dwMsg;
    return MMSYSERR_NOERROR;
}

MMRESULT midiOutReset(HMIDIOUT hmo)
{
    (void)hmo;
    return MMSYSERR_NOERROR;
}

MMRESULT midiOutSetVolume(HMIDIOUT hmo, DWORD dwVolume)
{
    (void)hmo;
    (void)dwVolume;
    return MMSYSERR_NOERROR;
}

MMRESULT midiOutGetVolume(HMIDIOUT hmo, LPDWORD pdwVolume)
{
    (void)hmo;
    if (pdwVolume != NULL) {
        *pdwVolume = 0xffffffffu;
    }
    return MMSYSERR_NOERROR;
}

UINT midiOutGetNumDevs(void)
{
    return 1;
}

MMRESULT midiOutGetDevCapsA(UINT uDeviceID, LPVOID pCaps, UINT cbCaps)
{
    (void)uDeviceID;
    if (pCaps != NULL && cbCaps > 0) {
        memset(pCaps, 0, cbCaps);
    }
    return MMSYSERR_NOERROR;
}

MMRESULT waveOutOpen(HWAVEOUT *phwo, UINT uDeviceID, LPCWAVEFORMATEX pwfx,
                     DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen)
{
    (void)phwo;
    (void)uDeviceID;
    (void)pwfx;
    (void)dwCallback;
    (void)dwInstance;
    (void)fdwOpen;
    return MMSYSERR_NODRIVER;
}

MMRESULT waveOutClose(HWAVEOUT hwo)
{
    (void)hwo;
    return MMSYSERR_NOERROR;
}

MMRESULT waveOutPrepareHeader(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh)
{
    (void)hwo;
    (void)pwh;
    (void)cbwh;
    return MMSYSERR_NOERROR;
}

MMRESULT waveOutUnprepareHeader(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh)
{
    (void)hwo;
    (void)pwh;
    (void)cbwh;
    return MMSYSERR_NOERROR;
}

MMRESULT waveOutWrite(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh)
{
    (void)hwo;
    (void)pwh;
    (void)cbwh;
    return MMSYSERR_NOERROR;
}

MMRESULT waveOutReset(HWAVEOUT hwo)
{
    (void)hwo;
    return MMSYSERR_NOERROR;
}

MMRESULT waveOutSetVolume(HWAVEOUT hwo, DWORD dwVolume)
{
    (void)hwo;
    (void)dwVolume;
    return MMSYSERR_NOERROR;
}

MMRESULT waveOutGetVolume(HWAVEOUT hwo, LPDWORD pdwVolume)
{
    (void)hwo;
    if (pdwVolume != NULL) {
        *pdwVolume = 0xffffffffu;
    }
    return MMSYSERR_NOERROR;
}

MMRESULT waveOutGetPosition(HWAVEOUT hwo, LPMMTIME pmmt, UINT cbmmt)
{
    (void)hwo;
    if (pmmt != NULL && cbmmt >= sizeof(*pmmt)) {
        pmmt->wType = 1; /* TIME_MS */
        pmmt->u.ms = timeGetTime();
    }
    return MMSYSERR_NOERROR;
}

UINT waveOutGetNumDevs(void)
{
    return 1;
}

MMRESULT waveOutGetDevCapsA(UINT uDeviceID, LPVOID pCaps, UINT cbCaps)
{
    (void)uDeviceID;
    if (pCaps != NULL && cbCaps > 0) {
        memset(pCaps, 0, cbCaps);
    }
    return MMSYSERR_NOERROR;
}

/* ------------------------------------------------------------------ */
/* Version, modules, profiles, diagnostics                            */
/* ------------------------------------------------------------------ */
DWORD GetVersion(void)
{
    /* A modern Win32 version makes VerifyWin32sVersionOrReportStartupError
     * return success without trying to load the Win32s compatibility DLL. */
    return 0x04000000u;
}

DWORD WinGetVersion(void)
{
    return GetVersion();
}

HMODULE LoadLibraryA(LPCSTR lpLibFileName)
{
    (void)lpLibFileName;
    nw_last_error = 126; /* ERROR_MOD_NOT_FOUND */
    return NULL;
}

FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    (void)hModule;
    (void)lpProcName;
    nw_last_error = 127; /* ERROR_PROC_NOT_FOUND */
    return NULL;
}

BOOL FreeLibrary(HMODULE hModule)
{
    (void)hModule;
    return TRUE;
}

DWORD GetPrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName,
                               LPCSTR lpDefault, LPSTR lpReturnedString,
                               DWORD nSize, LPCSTR lpFileName)
{
    (void)lpAppName;
    (void)lpKeyName;
    (void)lpFileName;
    if (lpReturnedString == NULL || nSize == 0) {
        return 0;
    }
    if (lpDefault != NULL) {
        snprintf(lpReturnedString, nSize, "%s", lpDefault);
    } else {
        lpReturnedString[0] = '\0';
    }
    return (DWORD)strlen(lpReturnedString);
}

int MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    (void)hWnd;
    (void)uType;
    {
        char buf[2048];
        int n = 0;
        if (lpCaption != NULL) {
            n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s: ",
                          lpCaption);
        }
        if (lpText != NULL) {
            n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s", lpText);
        }
        if (n < (int)sizeof(buf) - 1) {
            buf[n++] = '\n';
        }
        (void)write(2, buf, (size_t)n);
    }
    return IDOK;
}

int MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
    (void)hWnd;
    (void)lpText;
    (void)lpCaption;
    (void)uType;
    return IDOK;
}

BOOL MessageBeep(UINT uType)
{
    (void)uType;
    return TRUE;
}

BOOL SetUnhandledExceptionFilter(void *p)
{
    (void)p;
    return TRUE;
}

LONG UnhandledExceptionFilter(EXCEPTION_POINTERS *pExceptionInfo)
{
    (void)pExceptionInfo;
    return 0;                   /* EXCEPTION_CONTINUE_SEARCH */
}

PVOID AddVectoredExceptionHandler(ULONG first, PVOID handler)
{
    (void)first;
    (void)handler;
    return (PVOID)1;
}

ULONG RemoveVectoredExceptionHandler(PVOID handle)
{
    (void)handle;
    return 1;
}

int WinHelpA(HWND hWnd, LPCSTR lpszHelp, UINT uCommand, ULONG_PTR dwData)
{
    (void)hWnd;
    (void)lpszHelp;
    (void)uCommand;
    (void)dwData;
    return 1;
}

BOOL ShellExecuteA(HWND hwnd, LPCSTR op, LPCSTR file, LPCSTR params,
                   LPCSTR dir, int show)
{
    (void)hwnd;
    (void)op;
    (void)file;
    (void)params;
    (void)dir;
    (void)show;
    return FALSE;
}

BOOL CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                    LPSECURITY_ATTRIBUTES lpProcessAttributes,
                    LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
                    DWORD dwCreationFlags, LPVOID lpEnvironment,
                    LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo,
                    LPPROCESS_INFORMATION lpProcessInformation)
{
    (void)lpApplicationName;
    (void)lpCommandLine;
    (void)lpProcessAttributes;
    (void)lpThreadAttributes;
    (void)bInheritHandles;
    (void)dwCreationFlags;
    (void)lpEnvironment;
    (void)lpCurrentDirectory;
    (void)lpStartupInfo;
    (void)lpProcessInformation;
    nw_last_error = 2;
    return FALSE;
}

BOOL ReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer,
                       SIZE_T nSize, SIZE_T *lpNumberOfBytesRead)
{
    (void)hProcess;
    (void)lpBaseAddress;
    (void)lpBuffer;
    (void)nSize;
    if (lpNumberOfBytesRead != NULL) {
        *lpNumberOfBytesRead = 0;
    }
    return FALSE;
}

/* ------------------------------------------------------------------ */
/* String helpers                                                     */
/* ------------------------------------------------------------------ */
int wsprintfA(LPSTR lpOut, LPCSTR lpFmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, lpFmt);
    n = vsnprintf(lpOut, 1024, lpFmt, ap);
    va_end(ap);
    return n;
}

int wvsprintfA(LPSTR lpOut, LPCSTR lpFmt, va_list ap)
{
    return vsnprintf(lpOut, 1024, lpFmt, ap);
}

int lstrlenA(LPCSTR p)
{
    return p != NULL ? (int)strlen(p) : 0;
}

LPSTR lstrcpyA(LPSTR d, LPCSTR s)
{
    return strcpy(d, s);
}

LPSTR lstrcpynA(LPSTR d, LPCSTR s, int n)
{
    if (n <= 0) {
        return d;
    }
    strncpy(d, s, (size_t)n - 1);
    d[n - 1] = '\0';
    return d;
}

LPSTR lstrcatA(LPSTR d, LPCSTR s)
{
    return strcat(d, s);
}

int lstrcmpA(LPCSTR a, LPCSTR b)
{
    return strcmp(a, b);
}

int lstrcmpiA(LPCSTR a, LPCSTR b)
{
    return strcasecmp(a, b);
}

int MulDiv(int a, int b, int c)
{
    return c != 0 ? (int)(((int64_t)a * b) / c) : -1;
}

/* ------------------------------------------------------------------ */
/* GDI and window entry points used by linked but inactive units      */
/* ------------------------------------------------------------------ */
static int g_dummy_dc;

HDC GetDC(HWND hWnd)
{
    (void)hWnd;
    return (HDC)&g_dummy_dc;
}

/* The SDL window path never dispatches Win32 WM_PAINT, but the recovered
 * window procedure is still linked into the shared library.  Provide harmless
 * equivalents so every exported symbol can resolve on Android. */
HDC BeginPaint(HWND hWnd, LPPAINTSTRUCT ps)
{
    if (ps != NULL)
        memset(ps, 0, sizeof(*ps));
    return GetDC(hWnd);
}

BOOL EndPaint(HWND hWnd, const PAINTSTRUCT *ps)
{
    (void)hWnd;
    (void)ps;
    return TRUE;
}

BOOL GetUpdateRect(HWND hWnd, LPRECT rect, BOOL erase)
{
    (void)hWnd;
    (void)erase;
    if (rect != NULL)
        memset(rect, 0, sizeof(*rect));
    return FALSE;
}

int ReleaseDC(HWND hWnd, HDC hDC)
{
    (void)hWnd;
    (void)hDC;
    return 1;
}

BOOL GdiFlush(void)
{
    return TRUE;
}

int GetDeviceCaps(HDC hdc, int index)
{
    (void)hdc;
    switch (index) {
    case 12: return 8;      /* BITSPIXEL */
    case 14: return 1;      /* PLANES */
    case 8:  return 640;    /* HORZRES */
    case 10: return 480;    /* VERTRES */
    default: return 0;
    }
}

DWORD GetSystemMetrics(int index)
{
    switch (index) {
    case 0:  return 640;   /* SM_CXSCREEN */
    case 1:  return 480;   /* SM_CYSCREEN */
    case 11: return 16;    /* SM_CXICON */
    case 12: return 16;    /* SM_CYICON */
    case 13: return 32;    /* SM_CXCURSOR */
    case 14: return 32;    /* SM_CYCURSOR */
    case 16: return 640;   /* SM_CXFULLSCREEN */
    case 17: return 480;   /* SM_CYFULLSCREEN */
    case 19: return 1;     /* SM_MOUSEPRESENT */
    default: return 0;
    }
}

BOOL GetClientRect(HWND hWnd, LPRECT r)
{
    (void)hWnd;
    if (r != NULL) {
        r->left = 0;
        r->top = 0;
        r->right = 640;
        r->bottom = 480;
    }
    return TRUE;
}

BOOL GetWindowRect(HWND hWnd, LPRECT r)
{
    return GetClientRect(hWnd, r);
}

BOOL GetCursorPos(LPPOINT p)
{
    if (p != NULL) {
        p->x = 0;
        p->y = 0;
    }
    return TRUE;
}

BOOL SetCursorPos(int x, int y)
{
    (void)x;
    (void)y;
    return TRUE;
}

SHORT GetAsyncKeyState(int vKey)
{
    (void)vKey;
    return 0;
}

BOOL ClientToScreen(HWND hWnd, LPPOINT p)
{
    (void)hWnd;
    (void)p;
    return TRUE;
}

BOOL ScreenToClient(HWND hWnd, LPPOINT p)
{
    (void)hWnd;
    (void)p;
    return TRUE;
}

int MapWindowPoints(HWND from, HWND to, LPPOINT p, UINT n)
{
    (void)from;
    (void)to;
    (void)p;
    (void)n;
    return 0;
}

BOOL InvalidateRect(HWND hWnd, const RECT *r, BOOL erase)
{
    (void)hWnd;
    (void)r;
    (void)erase;
    return TRUE;
}

BOOL ValidateRect(HWND hWnd, const RECT *r)
{
    (void)hWnd;
    (void)r;
    return TRUE;
}

BOOL UpdateWindow(HWND hWnd)
{
    (void)hWnd;
    return TRUE;
}

BOOL ShowWindow(HWND hWnd, int cmd)
{
    (void)hWnd;
    (void)cmd;
    return TRUE;
}

BOOL DestroyWindow(HWND hWnd)
{
    (void)hWnd;
    return TRUE;
}

BOOL SetWindowPos(HWND hWnd, HWND after, int x, int y, int cx, int cy,
                  UINT flags)
{
    (void)hWnd;
    (void)after;
    (void)x;
    (void)y;
    (void)cx;
    (void)cy;
    (void)flags;
    return TRUE;
}

BOOL MoveWindow(HWND hWnd, int x, int y, int w, int h, BOOL repaint)
{
    (void)hWnd;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)repaint;
    return TRUE;
}

int ScrollWindow(HWND hWnd, int dx, int dy, const RECT *scroll,
                 const RECT *clip)
{
    (void)hWnd;
    (void)dx;
    (void)dy;
    (void)scroll;
    (void)clip;
    return 0;
}

BOOL IsWindowVisible(HWND hWnd)
{
    (void)hWnd;
    return TRUE;
}

BOOL IsIconic(HWND hWnd)
{
    (void)hWnd;
    return FALSE;
}

HWND WindowFromDC(HDC hdc)
{
    (void)hdc;
    return NULL;
}

HWND WindowFromPoint(POINT p)
{
    (void)p;
    return NULL;
}

BOOL GetDIBColorTable(HDC hdc, UINT start, UINT count, RGBQUAD *table)
{
    (void)hdc;
    (void)start;
    if (table != NULL) {
        memset(table, 0, (size_t)count * sizeof(*table));
    }
    return count;
}

DWORD GetPixel(HDC hdc, int x, int y)
{
    (void)hdc;
    (void)x;
    (void)y;
    return 0;
}

BOOL PeekMessageA(LPMSG msg, HWND hWnd, UINT min, UINT max, UINT remove)
{
    (void)msg;
    (void)hWnd;
    (void)min;
    (void)max;
    (void)remove;
    return FALSE;
}

BOOL PostMessageA(HWND hWnd, UINT msg, WPARAM w, LPARAM l)
{
    (void)hWnd;
    (void)msg;
    (void)w;
    (void)l;
    return TRUE;
}

BOOL GetMessageA(LPMSG msg, HWND hWnd, UINT min, UINT max)
{
    (void)msg;
    (void)hWnd;
    (void)min;
    (void)max;
    return FALSE;
}

BOOL TranslateMessage(const MSG *msg)
{
    (void)msg;
    return TRUE;
}

LRESULT DispatchMessageA(const MSG *msg)
{
    (void)msg;
    return 0;
}

void PostQuitMessage(int code)
{
    (void)code;
}

BOOL SetForegroundWindow(HWND hWnd)
{
    (void)hWnd;
    return TRUE;
}

HWND SetFocus(HWND hWnd)
{
    (void)hWnd;
    return NULL;
}

HWND GetFocus(void)
{
    return NULL;
}

BOOL SetTimer(HWND hWnd, UINT_PTR id, UINT elapse, void *fn)
{
    (void)hWnd;
    (void)id;
    (void)elapse;
    (void)fn;
    return TRUE;
}

BOOL KillTimer(HWND hWnd, UINT_PTR id)
{
    (void)hWnd;
    (void)id;
    return TRUE;
}

HCURSOR LoadCursorA(HINSTANCE inst, LPCSTR name)
{
    (void)inst;
    (void)name;
    return (HCURSOR)(uintptr_t)1;
}

HCURSOR SetCursor(HCURSOR c)
{
    return c;
}

int ShowCursor(BOOL show)
{
    (void)show;
    return 1;
}

BOOL SetWindowTextA(HWND hWnd, LPCSTR text)
{
    (void)hWnd;
    (void)text;
    return TRUE;
}

BOOL SetCursorPos(int x, int y);

HWND CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR windowName,
                     DWORD style, int x, int y, int w, int h, HWND parent,
                     HMENU menu, HINSTANCE inst, LPVOID param)
{
    (void)exStyle;
    (void)className;
    (void)windowName;
    (void)style;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)parent;
    (void)menu;
    (void)inst;
    (void)param;
    return NULL;
}

ATOM RegisterClassA(const WNDCLASSA *wc)
{
    (void)wc;
    return 1;
}

BOOL UnregisterClassA(LPCSTR name, HINSTANCE inst)
{
    (void)name;
    (void)inst;
    return TRUE;
}

LRESULT DefWindowProcA(HWND hWnd, UINT msg, WPARAM w, LPARAM l)
{
    (void)hWnd;
    (void)msg;
    (void)w;
    (void)l;
    return 0;
}

LRESULT CallWindowProcA(WNDPROC proc, HWND hWnd, UINT msg, WPARAM w, LPARAM l)
{
    if (proc != NULL) {
        return proc(hWnd, msg, w, l);
    }
    return 0;
}

LRESULT SendMessageA(HWND hWnd, UINT msg, WPARAM w, LPARAM l)
{
    (void)hWnd;
    (void)msg;
    (void)w;
    (void)l;
    return 0;
}

HBRUSH CreateSolidBrush(COLORREF color)
{
    (void)color;
    return (HBRUSH)(uintptr_t)1;
}

BOOL DeleteObject(HGDIOBJ obj)
{
    (void)obj;
    return TRUE;
}

HPALETTE CreatePalette(const LOGPALETTE *lp)
{
    (void)lp;
    return (HPALETTE)(uintptr_t)1;
}

UINT SetPaletteEntries(HPALETTE pal, UINT start, UINT count,
                       const PALETTEENTRY *entries)
{
    (void)pal;
    (void)start;
    (void)entries;
    return count;
}

HPALETTE SelectPalette(HDC hdc, HPALETTE pal, BOOL bg)
{
    (void)hdc;
    (void)bg;
    return pal;
}

UINT RealizePalette(HDC hdc)
{
    (void)hdc;
    return 0;
}

int GetObjectA(HANDLE h, int size, LPVOID p)
{
    (void)h;
    (void)size;
    (void)p;
    return 0;
}

HBITMAP CreateDIBSection(HDC hdc, const BITMAPINFO *bi, UINT usage,
                         void **bits, HANDLE section, DWORD offset)
{
    (void)hdc;
    (void)bi;
    (void)usage;
    (void)section;
    (void)offset;
    if (bits != NULL) {
        *bits = NULL;
    }
    return NULL;
}

HDC CreateCompatibleDC(HDC hdc)
{
    (void)hdc;
    return (HDC)&g_dummy_dc;
}

BOOL DeleteDC(HDC hdc)
{
    (void)hdc;
    return TRUE;
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ obj)
{
    (void)hdc;
    return obj;
}

BOOL BitBlt(HDC dst, int x, int y, int w, int h, HDC src, int sx, int sy,
            DWORD rop)
{
    (void)dst;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)src;
    (void)sx;
    (void)sy;
    (void)rop;
    return TRUE;
}

BOOL PatBlt(HDC hdc, int x, int y, int w, int h, DWORD rop)
{
    (void)hdc;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)rop;
    return TRUE;
}

BOOL SetBkColor(HDC hdc, COLORREF c)
{
    (void)hdc;
    (void)c;
    return TRUE;
}

BOOL SetTextColor(HDC hdc, COLORREF c)
{
    (void)hdc;
    (void)c;
    return TRUE;
}

int SetBkMode(HDC hdc, int mode)
{
    (void)hdc;
    (void)mode;
    return 1;
}

int DrawTextA(HDC hdc, LPCSTR text, int n, LPRECT r, UINT fmt)
{
    (void)hdc;
    (void)text;
    (void)n;
    (void)r;
    (void)fmt;
    return 0;
}

BOOL TextOutA(HDC hdc, int x, int y, LPCSTR text, int n)
{
    (void)hdc;
    (void)x;
    (void)y;
    (void)text;
    (void)n;
    return TRUE;
}

BOOL GetTextMetricsA(HDC hdc, LPVOID tm)
{
    (void)hdc;
    if (tm != NULL) {
        memset(tm, 0, 64);
    }
    return TRUE;
}

HFONT CreateFontA(int h, int w, int esc, int orient, int weight, DWORD italic,
                  DWORD underline, DWORD strike, DWORD charset, DWORD outprec,
                  DWORD clipprec, DWORD quality, DWORD pitch, LPCSTR face)
{
    (void)h;
    (void)w;
    (void)esc;
    (void)orient;
    (void)weight;
    (void)italic;
    (void)underline;
    (void)strike;
    (void)charset;
    (void)outprec;
    (void)clipprec;
    (void)quality;
    (void)pitch;
    (void)face;
    return (HFONT)(uintptr_t)1;
}

BOOL FillRect(HDC hdc, const RECT *r, HBRUSH b)
{
    (void)hdc;
    (void)r;
    (void)b;
    return TRUE;
}

BOOL FrameRect(HDC hdc, const RECT *r, HBRUSH b)
{
    (void)hdc;
    (void)r;
    (void)b;
    return TRUE;
}

BOOL Rectangle(HDC hdc, int l, int t, int r, int b)
{
    (void)hdc;
    (void)l;
    (void)t;
    (void)r;
    (void)b;
    return TRUE;
}

BOOL MoveToEx(HDC hdc, int x, int y, LPPOINT old)
{
    (void)hdc;
    (void)x;
    (void)y;
    (void)old;
    return TRUE;
}

BOOL LineTo(HDC hdc, int x, int y)
{
    (void)hdc;
    (void)x;
    (void)y;
    return TRUE;
}

int SaveDC(HDC hdc)
{
    (void)hdc;
    return 1;
}

BOOL RestoreDC(HDC hdc, int saved)
{
    (void)hdc;
    (void)saved;
    return TRUE;
}

BOOL SetMapMode(HDC hdc, int mode)
{
    (void)hdc;
    (void)mode;
    return TRUE;
}

BOOL SetViewportOrgEx(HDC hdc, int x, int y, LPPOINT old)
{
    (void)hdc;
    (void)x;
    (void)y;
    (void)old;
    return TRUE;
}

BOOL SetWindowOrgEx(HDC hdc, int x, int y, LPPOINT old)
{
    (void)hdc;
    (void)x;
    (void)y;
    (void)old;
    return TRUE;
}

BOOL SetViewportExtEx(HDC hdc, int x, int y, LPSIZE old)
{
    (void)hdc;
    (void)x;
    (void)y;
    (void)old;
    return TRUE;
}

BOOL SetWindowExtEx(HDC hdc, int x, int y, LPSIZE old)
{
    (void)hdc;
    (void)x;
    (void)y;
    (void)old;
    return TRUE;
}

BOOL OffsetViewportOrgEx(HDC hdc, int x, int y, LPPOINT old)
{
    (void)hdc;
    (void)x;
    (void)y;
    (void)old;
    return TRUE;
}

BOOL OffsetWindowOrgEx(HDC hdc, int x, int y, LPPOINT old)
{
    (void)hdc;
    (void)x;
    (void)y;
    (void)old;
    return TRUE;
}

BOOL ScaleViewportExtEx(HDC hdc, int xn, int xd, int yn, int yd, LPSIZE old)
{
    (void)hdc;
    (void)xn;
    (void)xd;
    (void)yn;
    (void)yd;
    (void)old;
    return TRUE;
}

BOOL ScaleWindowExtEx(HDC hdc, int xn, int xd, int yn, int yd, LPSIZE old)
{
    (void)hdc;
    (void)xn;
    (void)xd;
    (void)yn;
    (void)yd;
    (void)old;
    return TRUE;
}

int GetMapMode(HDC hdc)
{
    (void)hdc;
    return 1;
}

BOOL PtVisible(HDC hdc, int x, int y)
{
    (void)hdc;
    (void)x;
    (void)y;
    return TRUE;
}

BOOL RectVisible(HDC hdc, const RECT *r)
{
    (void)hdc;
    (void)r;
    return TRUE;
}

BOOL GetWindowOrgEx(HDC hdc, LPPOINT p)
{
    (void)hdc;
    if (p != NULL) {
        p->x = p->y = 0;
    }
    return TRUE;
}

BOOL GetViewportOrgEx(HDC hdc, LPPOINT p)
{
    return GetWindowOrgEx(hdc, p);
}

BOOL GetViewportExtEx(HDC hdc, LPSIZE p)
{
    (void)hdc;
    if (p != NULL) {
        p->cx = 640;
        p->cy = 480;
    }
    return TRUE;
}

BOOL GetWindowExtEx(HDC hdc, LPSIZE p)
{
    return GetViewportExtEx(hdc, p);
}

BOOL LPtoDP(HDC hdc, LPPOINT p, int n)
{
    (void)hdc;
    (void)p;
    (void)n;
    return TRUE;
}

BOOL DPtoLP(HDC hdc, LPPOINT p, int n)
{
    (void)hdc;
    (void)p;
    (void)n;
    return TRUE;
}

int GetClipBox(HDC hdc, LPRECT r)
{
    (void)hdc;
    if (r != NULL) {
        r->left = 0;
        r->top = 0;
        r->right = 640;
        r->bottom = 480;
    }
    return 1;
}

BOOL IntersectRect(LPRECT d, const RECT *a, const RECT *b)
{
    LONG l = a->left > b->left ? a->left : b->left;
    LONG t = a->top > b->top ? a->top : b->top;
    LONG r = a->right < b->right ? a->right : b->right;
    LONG bo = a->bottom < b->bottom ? a->bottom : b->bottom;
    if (l >= r || t >= bo) {
        SetRectEmpty(d);
        return FALSE;
    }
    d->left = l;
    d->top = t;
    d->right = r;
    d->bottom = bo;
    return TRUE;
}

BOOL UnionRect(LPRECT d, const RECT *a, const RECT *b)
{
    d->left = a->left < b->left ? a->left : b->left;
    d->top = a->top < b->top ? a->top : b->top;
    d->right = a->right > b->right ? a->right : b->right;
    d->bottom = a->bottom > b->bottom ? a->bottom : b->bottom;
    return TRUE;
}

BOOL IsRectEmpty(const RECT *r)
{
    return r->left >= r->right || r->top >= r->bottom;
}

BOOL SetRect(LPRECT r, int l, int t, int right, int bottom)
{
    r->left = l;
    r->top = t;
    r->right = right;
    r->bottom = bottom;
    return TRUE;
}

BOOL SetRectEmpty(LPRECT r)
{
    return SetRect(r, 0, 0, 0, 0);
}

BOOL CopyRect(LPRECT d, const RECT *s)
{
    *d = *s;
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Globals referenced by converted units but not owned by a unit      */
/* ------------------------------------------------------------------ */
volatile unsigned int g_nwCsrIn;
volatile unsigned int g_nwCsrOut;
volatile unsigned int g_nwCsrIter;
DWORD (__cdecl *g_pfStandardStreamRedirect)(DWORD, const void *, DWORD);

/* The UT notification handlers below are only published by the legacy
 * InitializeUtDriverAndHandlerTable path.  The SDL audio subsystem publishes
 * the real handlers from AudioPipeline.c, so these are inert placeholders. */
void UtNotifyCode0x202(void) {}
void UtNotifyCode0x203(void) {}
void UtNotifyCode0x204(void) {}
void UtNotifyCode0x205(void) {}
void UtNotifyCode0x206(void) {}
void UtNotifyCode0x207(void) {}
void UtNotifyCode0x209(void) {}
void UtNotifyCode0x20a(void) {}
void UtNotifySendArgCode0x208(void) {}
void UtNotifyCopyArgToBuffer0x208(void) {}
void UtNotifySendArgPlus100(void) {}

/* Import-pointer cells named by the original import table. */
void *__imp__PeekMessageA;
void *__imp__IsIconic;
