/*
 * Minimal Win32 compatibility surface for the native i686 Linux build.
 *
 * The recovered NetWalk sources are written against the Win32 SDK.  Most of
 * the code only uses scalar typedefs and a small set of file, thread, timer
 * and multimedia entry points.  This header supplies those declarations and
 * the Linux compatibility layer implements the subset the game reaches.
 *
 * It deliberately does not try to be a general Win32 emulator: GDI, window
 * messages and MCI are represented by types and no-op entry points, because
 * the Linux build selects the SDL3 backends for those services.
 */
#ifndef NW_LINUX_WINDOWS_H
#define NW_LINUX_WINDOWS_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <pthread.h>
#include <semaphore.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Scalar types                                                        */
/* ------------------------------------------------------------------ */
typedef int              BOOL;
typedef unsigned char    BYTE;
typedef unsigned char    UCHAR;
typedef unsigned char    UINT8;
typedef unsigned short   UINT16;
typedef unsigned int     UINT32;
typedef unsigned char    byte;
typedef unsigned short   WORD;
typedef unsigned short   USHORT;
typedef unsigned short   ATOM;
typedef uint32_t         DWORD;
typedef uint32_t         ULONG;
typedef int32_t          LONG;
typedef int16_t          SHORT;
typedef int16_t          INT16;
typedef int64_t          INT64;
typedef uint64_t         UINT64;
typedef int              INT;
typedef unsigned int     UINT;
typedef char             CHAR;
typedef char             TCHAR;
typedef wchar_t          WCHAR;
typedef intptr_t         INT_PTR;
typedef uintptr_t        UINT_PTR;
typedef intptr_t         LONG_PTR;
typedef uintptr_t        ULONG_PTR;
typedef uintptr_t        DWORD_PTR;
typedef size_t           SIZE_T;
typedef size_t           ULONG_PTR_SIZE;
typedef unsigned long    FLAGS;
typedef unsigned char    BOOLEAN;
typedef void            *PVOID;
typedef const void      *LPCVOID;
typedef void            *LPVOID;
typedef void            *HANDLE;
typedef void            *HWND;
typedef void            *HDC;
typedef void            *HINSTANCE;
typedef void            *HMODULE;
typedef void            *HGDIOBJ;
typedef void            *HBRUSH;
typedef void            *HPEN;
typedef void            *HFONT;
typedef void            *HBITMAP;
typedef void            *HICON;
typedef void            *HCURSOR;
typedef void            *HMENU;
typedef void            *HPALETTE;
typedef void            *HKEY;
typedef void            *HGLOBAL;
typedef void            *HLOCAL;
typedef void            *HMIDIOUT;
typedef void            *HWAVEOUT;
typedef void            *HMMIO;
typedef uint32_t         MCIDEVICEID;
typedef uint32_t         MMRESULT;
typedef uint32_t         COLORREF;
typedef char            *LPSTR;
typedef const char      *LPCSTR;
typedef char            *LPCH;
typedef const char      *LPCCH;
typedef WCHAR           *LPWSTR;
typedef const WCHAR     *LPCWSTR;
typedef BYTE            *PBYTE;
typedef BYTE            *LPBYTE;
typedef WORD            *PWORD;
typedef WORD            *LPWORD;
typedef DWORD           *PDWORD;
typedef DWORD           *LPDWORD;
typedef LONG            *PLONG;
typedef LONG            *LPLONG;
typedef UINT            *PUINT;
typedef INT             *PINT;
typedef BOOL            *PBOOL;
typedef BOOL            *LPBOOL;
typedef void            *PHANDLE;
typedef void            *LPHANDLE;
typedef UINT_PTR         WPARAM;
typedef LONG_PTR         LPARAM;
typedef LONG_PTR         LRESULT;
typedef void           (*FARPROC)(void);
typedef DWORD          (*LPTHREAD_START_ROUTINE)(LPVOID);

/* ------------------------------------------------------------------ */
/* Calling conventions / attributes                                    */
/* ------------------------------------------------------------------ */
#define WINAPI
#define APIENTRY
#define CALLBACK
#define WINAPIV
#define __stdcall
#define __cdecl
#define __fastcall
#define __thiscall
#define DECLSPEC_IMPORT
#define WINBASEAPI
#define WINUSERAPI
#define WINGDIAPI

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define MAX_PATH          260
#define MAX_PATH_LEN      260
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INVALID_FILE_SIZE ((DWORD)0xffffffffu)
#define INVALID_SET_FILE_POINTER ((DWORD)0xffffffffu)
#define INVALID_FILE_ATTRIBUTES ((DWORD)0xffffffffu)
#define INVALID_DRIVE      0xffffffffu

#define FILE_ATTRIBUTE_READONLY   0x00000001u
#define FILE_ATTRIBUTE_HIDDEN     0x00000002u
#define FILE_ATTRIBUTE_SYSTEM     0x00000004u
#define FILE_ATTRIBUTE_DIRECTORY  0x00000010u
#define FILE_ATTRIBUTE_ARCHIVE    0x00000020u
#define FILE_ATTRIBUTE_NORMAL     0x00000080u
#define FILE_ATTRIBUTE_TEMPORARY  0x00000100u

#define GENERIC_READ              0x80000000u
#define GENERIC_WRITE             0x40000000u
#define GENERIC_EXECUTE           0x20000000u
#define GENERIC_ALL               0x10000000u
#define FILE_READ_DATA            0x0001u
#define FILE_WRITE_DATA           0x0002u
#define FILE_APPEND_DATA          0x0004u
#define FILE_READ_EA              0x0008u
#define FILE_WRITE_EA             0x0010u
#define FILE_READ_ATTRIBUTES      0x0080u
#define FILE_WRITE_ATTRIBUTES     0x0100u

#define FILE_SHARE_READ           0x00000001u
#define FILE_SHARE_WRITE          0x00000002u
#define FILE_SHARE_DELETE         0x00000004u

#define CREATE_NEW                1u
#define CREATE_ALWAYS             2u
#define OPEN_EXISTING             3u
#define OPEN_ALWAYS               4u
#define TRUNCATE_EXISTING         5u

#define FILE_BEGIN                0u
#define FILE_CURRENT              1u
#define FILE_END                  2u

#define FILE_TYPE_UNKNOWN         0x0000u
#define FILE_TYPE_DISK            0x0001u
#define FILE_TYPE_CHAR            0x0002u
#define FILE_TYPE_PIPE            0x0003u
#define FILE_TYPE_REMOTE          0x8000u

#define STD_INPUT_HANDLE          ((DWORD)-10)
#define STD_OUTPUT_HANDLE         ((DWORD)-11)
#define STD_ERROR_HANDLE          ((DWORD)-12)

#define INFINITE                  0xffffffffu
#define WAIT_OBJECT_0             0x00000000u
#define WAIT_ABANDONED            0x00000080u
#define WAIT_TIMEOUT              0x00000102u
#define WAIT_FAILED               0xffffffffu
#define STILL_ACTIVE              259u

#define GMEM_FIXED                0x0000u
#define GMEM_MOVEABLE             0x0002u
#define GMEM_ZEROINIT             0x0040u
#define GPTR                      0x0040u
#define GHND                      0x0042u

#define PAGE_NOACCESS             0x01u
#define PAGE_READONLY             0x02u
#define PAGE_READWRITE            0x04u
#define PAGE_WRITECOPY            0x08u
#define PAGE_EXECUTE              0x10u
#define PAGE_EXECUTE_READ         0x20u
#define PAGE_EXECUTE_READWRITE    0x40u
#define PAGE_EXECUTE_WRITECOPY    0x80u
#define MEM_COMMIT                0x1000u
#define MEM_RESERVE               0x2000u
#define MEM_DECOMMIT              0x4000u
#define MEM_RELEASE               0x8000u
#define MEM_FREE                  0x10000u
#define MEM_IMAGE                 0x1000000u
#define MEM_MAPPED                0x40000u
#define MEM_PRIVATE               0x20000u

#define CP_ACP                    0u
#define CP_OEMCP                  1u
#define CP_UTF8                   65001u

#define MB_OK                     0x00000000u
#define MB_OKCANCEL               0x00000001u
#define MB_ABORTRETRYIGNORE       0x00000002u
#define MB_YESNOCANCEL            0x00000003u
#define MB_YESNO                  0x00000004u
#define MB_RETRYCANCEL            0x00000005u
#define MB_ICONHAND               0x00000010u
#define MB_ICONERROR              0x00000010u
#define MB_ICONQUESTION           0x00000020u
#define MB_ICONEXCLAMATION        0x00000030u
#define MB_ICONWARNING            0x00000030u
#define MB_ICONASTERISK           0x00000040u
#define MB_ICONINFORMATION        0x00000040u
#define IDOK                      1
#define IDCANCEL                  2
#define IDABORT                   3
#define IDRETRY                   4
#define IDIGNORE                  5
#define IDYES                     6
#define IDNO                      7

#define SW_HIDE                   0
#define SW_SHOWNORMAL             1
#define SW_SHOW                   5
#define SW_SHOWDEFAULT            10

#define HTCLIENT                   1
#define HTCAPTION                  2
#define IDC_ARROW                  ((LPCSTR)(uintptr_t)32512)
#define IDC_IBEAM                  ((LPCSTR)(uintptr_t)32513)
#define IDC_WAIT                   ((LPCSTR)(uintptr_t)32514)
#define IDC_CROSS                  ((LPCSTR)(uintptr_t)32515)
#define IDC_UPARROW                ((LPCSTR)(uintptr_t)32516)
#define IDC_SIZENWSE               ((LPCSTR)(uintptr_t)32642)
#define IDC_SIZENESW               ((LPCSTR)(uintptr_t)32643)
#define IDC_SIZEWE                 ((LPCSTR)(uintptr_t)32644)
#define IDC_SIZENS                 ((LPCSTR)(uintptr_t)32645)
#define IDC_SIZEALL                ((LPCSTR)(uintptr_t)32646)
#define IDC_NO                     ((LPCSTR)(uintptr_t)32648)
#define IDC_HAND                   ((LPCSTR)(uintptr_t)32649)
#define IDC_APPSTARTING            ((LPCSTR)(uintptr_t)32650)
#define IDC_HELP                   ((LPCSTR)(uintptr_t)32651)

#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_CONTINUE_EXECUTION (-1)

#define DRIVE_UNKNOWN             0
#define DRIVE_NO_ROOT_DIR         1
#define DRIVE_REMOVABLE           2
#define DRIVE_FIXED               3
#define DRIVE_REMOTE              4
#define DRIVE_CDROM               5
#define DRIVE_RAMDISK             6

#define DIB_RGB_COLORS            0
#define PS_SOLID                  0
#define PS_DASH                   1
#define PC_NOCOLLAPSE             0x0004u
#define DIB_PAL_COLORS            1
#define SRCCOPY                   0x00cc0020u
#define PATCOPY                   0x00f00021u
#define BI_RGB                    0
#define CBM_INIT                  4u

#define PM_NOREMOVE               0x0000u
#define PM_REMOVE                 0x0001u
#define PM_NOYIELD                0x0002u

#define WM_NULL                   0x0000u
#define WM_CREATE                 0x0001u
#define WM_DESTROY                0x0002u
#define WM_MOVE                   0x0003u
#define WM_SIZE                   0x0005u
#define WM_ACTIVATE               0x0006u
#define WM_SETFOCUS               0x0007u
#define WM_KILLFOCUS              0x0008u
#define WM_PAINT                  0x000Fu
#define WM_CLOSE                  0x0010u
#define WM_QUIT                   0x0012u
#define WM_ERASEBKGND             0x0014u
#define WM_SHOWWINDOW             0x0018u
#define WM_SETCURSOR              0x0020u
#define WM_MOUSEACTIVATE          0x0021u
#define WM_GETMINMAXINFO          0x0024u
#define WM_WINDOWPOSCHANGING      0x0046u
#define WM_WINDOWPOSCHANGED       0x0047u
#define WM_KEYDOWN                0x0100u
#define WM_KEYUP                  0x0101u
#define WM_CHAR                   0x0102u
#define WM_SYSKEYDOWN             0x0104u
#define WM_SYSKEYUP               0x0105u
#define WM_SYSCHAR                0x0106u
#define WM_COMMAND                0x0111u
#define WM_TIMER                  0x0113u
#define WM_MOUSEMOVE              0x0200u
#define WM_LBUTTONDOWN            0x0201u
#define WM_LBUTTONUP              0x0202u
#define WM_RBUTTONDOWN            0x0204u
#define WM_RBUTTONUP              0x0205u
#define WM_MOUSEWHEEL             0x020Au
#define WM_SETFONT                0x0030u
#define WM_USER                   0x0400u
#define WM_APP                    0x8000u

#define GWL_STYLE                 (-16)
#define GWL_EXSTYLE               (-20)
#define GWL_WNDPROC               (-4)
#define GWL_HINSTANCE             (-6)
#define GWL_HWNDPARENT            (-8)
#define GWL_ID                    (-12)
#define GWL_USERDATA              (-21)
#define WS_OVERLAPPED             0x00000000u
#define WS_POPUP                  0x80000000u
#define WS_CHILD                  0x40000000u
#define WS_MINIMIZE               0x20000000u
#define WS_VISIBLE                0x10000000u
#define WS_DISABLED               0x08000000u
#define WS_CLIPSIBLINGS           0x04000000u
#define WS_CLIPCHILDREN           0x02000000u
#define WS_MAXIMIZE               0x01000000u
#define WS_CAPTION                0x00C00000u
#define WS_BORDER                 0x00800000u
#define WS_DLGFRAME               0x00400000u
#define WS_VSCROLL                0x00200000u
#define WS_HSCROLL                0x00100000u
#define WS_SYSMENU                0x00080000u
#define WS_THICKFRAME             0x00040000u
#define WS_MINIMIZEBOX            0x00020000u
#define WS_MAXIMIZEBOX            0x00010000u
#define WS_OVERLAPPEDWINDOW       (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define SWP_NOSIZE                0x0001u
#define SWP_NOMOVE                0x0002u
#define SWP_NOZORDER              0x0004u
#define SWP_NOACTIVATE            0x0010u
#define SWP_SHOWWINDOW            0x0040u

#define VK_BACK                   0x08
#define VK_TAB                    0x09
#define VK_RETURN                 0x0D
#define VK_SHIFT                  0x10
#define VK_CONTROL                0x11
#define VK_MENU                   0x12
#define VK_PAUSE                  0x13
#define VK_CAPITAL                0x14
#define VK_ESCAPE                 0x1B
#define VK_SPACE                  0x20
#define VK_PRIOR                  0x21
#define VK_NEXT                   0x22
#define VK_END                    0x23
#define VK_HOME                   0x24
#define VK_LEFT                   0x25
#define VK_UP                     0x26
#define VK_RIGHT                  0x27
#define VK_DOWN                   0x28
#define VK_INSERT                 0x2D
#define VK_DELETE                 0x2E
#define VK_LSHIFT                 0xA0
#define VK_RSHIFT                 0xA1
#define VK_LCONTROL               0xA2
#define VK_RCONTROL               0xA3
#define VK_LMENU                  0xA4
#define VK_RMENU                  0xA5
#define VK_F1                     0x70
#define VK_NUMLOCK                0x90
#define VK_SCROLL                 0x91
#define VK_SNAPSHOT               0x2C
#define VK_NUMPAD0                0x60
#define VK_NUMPAD1                0x61
#define VK_NUMPAD2                0x62
#define VK_NUMPAD3                0x63
#define VK_NUMPAD4                0x64
#define VK_NUMPAD5                0x65
#define VK_NUMPAD6                0x66
#define VK_NUMPAD7                0x67
#define VK_NUMPAD8                0x68
#define VK_NUMPAD9                0x69
#define VK_MULTIPLY               0x6A
#define VK_ADD                    0x6B
#define VK_SUBTRACT               0x6D
#define VK_DECIMAL                0x6E
#define VK_DIVIDE                 0x6F
#define VK_OEM_COMMA              0xBC
#define VK_OEM_PERIOD             0xBE
#define VK_OEM_MINUS              0xBD
#define VK_OEM_PLUS               0xBB
#define VK_OEM_1                  0xBA
#define VK_OEM_2                  0xBF
#define VK_OEM_3                  0xC0
#define VK_OEM_4                  0xDB
#define VK_OEM_5                  0xDC
#define VK_OEM_6                  0xDD
#define VK_OEM_7                  0xDE

#define LOWORD(x)  ((WORD)((uintptr_t)(x) & 0xffffu))
#ifndef MAKEINTRESOURCEA
#define MAKEINTRESOURCEA(i) ((LPSTR)(uintptr_t)(WORD)(i))
#endif
#ifndef MAKEINTRESOURCEW
#define MAKEINTRESOURCEW(i) ((LPWSTR)(uintptr_t)(WORD)(i))
#endif
#define HIWORD(x)  ((WORD)(((uintptr_t)(x) >> 16) & 0xffffu))
#define LOBYTE(x)  ((BYTE)((uintptr_t)(x) & 0xffu))
#define HIBYTE(x)  ((BYTE)(((uintptr_t)(x) >> 8) & 0xffu))
#define MAKELONG(a,b) ((LONG)(((WORD)(a)) | (((DWORD)((WORD)(b))) << 16)))
#define MAKELPARAM(a,b) ((LPARAM)(uintptr_t)(((WORD)(a)) | (((DWORD)((WORD)(b))) << 16)))
#define MAKEWPARAM(a,b) ((WPARAM)(((WORD)(a)) | (((DWORD)((WORD)(b))) << 16)))
#define MAKELRESULT(a,b) ((LRESULT)(uintptr_t)(((WORD)(a)) | (((DWORD)((WORD)(b))) << 16)))
#define RGB(r,g,b) ((DWORD)(((BYTE)(r)) | (((BYTE)(g)) << 8) | (((BYTE)(b)) << 16)))
#define GetRValue(rgb) ((BYTE)((rgb) & 0xffu))
#define GetGValue(rgb) ((BYTE)(((rgb) >> 8) & 0xffu))
#define GetBValue(rgb) ((BYTE)(((rgb) >> 16) & 0xffu))
#define ZeroMemory(p,n) memset((p),0,(n))
#define CopyMemory(d,s,n) memcpy((d),(s),(n))
#define MoveMemory(d,s,n) memmove((d),(s),(n))
#define FillMemory(p,n,v) memset((p),(v),(n))
#define _countof(a) (sizeof(a)/sizeof((a)[0]))
#define UNREFERENCED_PARAMETER(x) ((void)(x))

/* ------------------------------------------------------------------ */
/* Structures                                                          */
/* ------------------------------------------------------------------ */
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);

#pragma pack(push, 1)
typedef struct _SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay;
    WORD wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _TIME_ZONE_INFORMATION {
    LONG Bias;
    WCHAR StandardName[32];
    SYSTEMTIME StandardDate;
    LONG StandardBias;
    WCHAR DaylightName[32];
    SYSTEMTIME DaylightDate;
    LONG DaylightBias;
} TIME_ZONE_INFORMATION, *LPTIME_ZONE_INFORMATION;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct { DWORD Offset; DWORD OffsetHigh; };
        PVOID Pointer;
    };
    HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; };
    struct { DWORD LowPart; LONG HighPart; } u;
    int64_t QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _WIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    CHAR cFileName[MAX_PATH];
    CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;

typedef struct _STARTUPINFOA {
    DWORD cb;
    LPSTR lpReserved;
    LPSTR lpDesktop;
    LPSTR lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars;
    DWORD dwFillAttribute, dwFlags;
    WORD wShowWindow, cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput, hStdOutput, hStdError;
} STARTUPINFOA, *LPSTARTUPINFOA;

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess, hThread;
    DWORD dwProcessId, dwThreadId;
} PROCESS_INFORMATION, *LPPROCESS_INFORMATION;

typedef struct _POINT { LONG x, y; } POINT, *PPOINT, *LPPOINT;
typedef struct _MINMAXINFO {
    POINT ptReserved;
    POINT ptMaxSize;
    POINT ptMaxPosition;
    POINT ptMinTrackSize;
    POINT ptMaxTrackSize;
} MINMAXINFO, *PMINMAXINFO, *LPMINMAXINFO;
typedef struct _SIZE { LONG cx, cy; } SIZE, *PSIZE, *LPSIZE;
typedef struct _RECT { LONG left, top, right, bottom; } RECT, *PRECT, *LPRECT;

typedef struct tagMSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
} MSG, *PMSG, *LPMSG;

typedef struct tagPAINTSTRUCT {
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT, *PPAINTSTRUCT, *LPPAINTSTRUCT;

typedef struct tagWNDCLASSA {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCSTR lpszMenuName;
    LPCSTR lpszClassName;
} WNDCLASSA, *LPWNDCLASSA;

typedef struct tagRGBQUAD { BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved; } RGBQUAD, *LPRGBQUAD;
typedef struct tagRGBTRIPLE { BYTE rgbtBlue, rgbtGreen, rgbtRed; } RGBTRIPLE;
typedef struct tagPALETTEENTRY { BYTE peRed, peGreen, peBlue, peFlags; } PALETTEENTRY, *PPALETTEENTRY, *LPPALETTEENTRY;
typedef struct tagLOGPALETTE {
    WORD palVersion;
    WORD palNumEntries;
    PALETTEENTRY palPalEntry[1];
} LOGPALETTE, *PLOGPALETTE, *LPLOGPALETTE;

typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[1];
} BITMAPINFO, *PBITMAPINFO, *LPBITMAPINFO;
typedef struct tagBITMAP {
    LONG bmType, bmWidth, bmHeight, bmWidthBytes;
    WORD bmPlanes, bmBitsPixel;
    LPVOID bmBits;
} BITMAP, *PBITMAP, *LPBITMAP;

typedef struct _MEMORY_BASIC_INFORMATION {
    PVOID BaseAddress;
    PVOID AllocationBase;
    DWORD AllocationProtect;
    SIZE_T RegionSize;
    DWORD State;
    DWORD Protect;
    DWORD Type;
} MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;

typedef struct _EXCEPTION_RECORD {
    DWORD ExceptionCode;
    DWORD ExceptionFlags;
    struct _EXCEPTION_RECORD *ExceptionRecord;
    PVOID ExceptionAddress;
    DWORD NumberParameters;
    ULONG_PTR ExceptionInformation[15];
} EXCEPTION_RECORD, *PEXCEPTION_RECORD;
typedef struct _CONTEXT {
    DWORD ContextFlags;
    DWORD Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    /* i386 CONTEXT layout: the fields the crash filter reads. */
    DWORD SegGs, SegFs, SegEs, SegDs;
    DWORD Edi, Esi, Ebx, Edx, Ecx, Eax;
    DWORD Ebp, Eip, SegCs, EFlags, Esp, SegSs;
    BYTE ExtendedRegisters[512];
} CONTEXT, *PCONTEXT;
typedef struct _EXCEPTION_POINTERS {
    PEXCEPTION_RECORD ExceptionRecord;
    PCONTEXT ContextRecord;
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS;

typedef struct _TIMECAPS {
    UINT wPeriodMin;
    UINT wPeriodMax;
    DWORD dwCapabilities;
} TIMECAPS, *LPTIMECAPS;
typedef struct _MMTIME {
    UINT wType;
    union {
        DWORD ms;
        struct { BYTE hour, min, sec; } smf;
        struct { DWORD songptrpos; } midi;
        struct { WORD wSeconds, wMinutes, wHours, wDays; } smpte;
    } u;
} MMTIME, *PMMTIME, *LPMMTIME;

typedef struct _WAVEFORMATEX {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *LPWAVEFORMATEX;
typedef const WAVEFORMATEX *LPCWAVEFORMATEX;
typedef struct _WAVEHDR {
    LPSTR lpData;
    DWORD dwBufferLength;
    DWORD dwBytesRecorded;
    DWORD_PTR dwUser;
    DWORD dwFlags;
    DWORD dwLoops;
    struct _WAVEHDR *lpNext;
    DWORD_PTR reserved;
} WAVEHDR, *PWAVEHDR, *LPWAVEHDR;

typedef struct _MIDIHDR {
    LPSTR lpData;
    DWORD dwBufferLength;
    DWORD dwBytesRecorded;
    DWORD_PTR dwUser;
    DWORD dwFlags;
    struct _MIDIHDR *lpNext;
    DWORD_PTR reserved;
} MIDIHDR, *PMIDIHDR, *LPMIDIHDR;

typedef struct _MCI_OPEN_PARMSA {
    DWORD_PTR dwCallback;
    MCIDEVICEID wDeviceID;
    LPCSTR lpstrDeviceType;
    LPCSTR lpstrElementName;
    LPCSTR lpstrAlias;
} MCI_OPEN_PARMSA, *LPMCI_OPEN_PARMSA;
typedef struct _MCI_PLAY_PARMS {
    DWORD_PTR dwCallback;
    DWORD dwFrom;
    DWORD dwTo;
} MCI_PLAY_PARMS, *LPMCI_PLAY_PARMS;
typedef struct _MCI_STATUS_PARMS {
    DWORD_PTR dwCallback;
    DWORD_PTR dwReturn;
    DWORD dwItem;
    DWORD dwTrack;
} MCI_STATUS_PARMS, *LPMCI_STATUS_PARMS;
typedef struct _MCI_GENERIC_PARMS {
    DWORD_PTR dwCallback;
} MCI_GENERIC_PARMS, *LPMCI_GENERIC_PARMS;
typedef struct _MCI_SET_PARMS {
    DWORD_PTR dwCallback;
    DWORD dwTimeFormat;
    DWORD dwAudio;
} MCI_SET_PARMS, *LPMCI_SET_PARMS;
#pragma pack(pop)

/* ------------------------------------------------------------------ */
/* Shared-memory-style helpers                                        */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* Function declarations (implemented by platforms/linux/linux_compat.c) */
/* ------------------------------------------------------------------ */
DWORD GetLastError(void);
void  SetLastError(DWORD);
DWORD GetCurrentThreadId(void);
DWORD GetVersion(void);
DWORD GetTickCount(void);
DWORD timeGetTime(void);
void  Sleep(DWORD);
DWORD GetSystemMetrics(int);
HDC   GetDC(HWND);
int   ReleaseDC(HWND, HDC);
BOOL  GdiFlush(void);
SHORT GetAsyncKeyState(int);
BOOL  PostMessageA(HWND, UINT, WPARAM, LPARAM);
BOOL  PeekMessageA(LPMSG, HWND, UINT, UINT, UINT);
BOOL  IsIconic(HWND);

HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES, LONG, LONG, LPCSTR);
BOOL  ReleaseSemaphore(HANDLE, LONG, LONG *);
DWORD WaitForSingleObject(HANDLE, DWORD);
BOOL  CloseHandle(HANDLE);
HANDLE CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, DWORD *);
BOOL  SetThreadPriority(HANDLE, int);
BOOL  GetExitCodeThread(HANDLE, DWORD *);
HANDLE GetCurrentProcess(void);
HANDLE GetCurrentThread(void);
BOOL  TerminateProcess(HANDLE, UINT);
void  ExitProcess(UINT) __attribute__((noreturn));

HANDLE GetStdHandle(DWORD);
BOOL  SetStdHandle(DWORD, HANDLE);
DWORD GetFileType(HANDLE);
HANDLE CreateFileA(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
BOOL  ReadFile(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
BOOL  WriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
DWORD SetFilePointer(HANDLE, LONG, PLONG, DWORD);
BOOL  SetEndOfFile(HANDLE);
DWORD GetFileSize(HANDLE, LPDWORD);
BOOL  GetFileSizeEx(HANDLE, PLARGE_INTEGER);
BOOL  DeleteFileA(LPCSTR);
BOOL  MoveFileA(LPCSTR, LPCSTR);
BOOL  CopyFileA(LPCSTR, LPCSTR, BOOL);
BOOL  CreateDirectoryA(LPCSTR, LPSECURITY_ATTRIBUTES);
BOOL  RemoveDirectoryA(LPCSTR);
DWORD GetFileAttributesA(LPCSTR);
BOOL  SetFileAttributesA(LPCSTR, DWORD);
DWORD GetFullPathNameA(LPCSTR, DWORD, LPSTR, LPSTR *);
DWORD GetCurrentDirectoryA(DWORD, LPSTR);
BOOL  SetCurrentDirectoryA(LPCSTR);
DWORD GetTempPathA(DWORD, LPSTR);
UINT  GetTempFileNameA(LPCSTR, LPCSTR, UINT, LPSTR);
DWORD GetModuleFileNameA(HMODULE, LPSTR, DWORD);
HMODULE GetModuleHandleA(LPCSTR);
DWORD GetEnvironmentVariableA(LPCSTR, LPSTR, DWORD);
LPSTR GetCommandLineA(void);
LPSTR GetEnvironmentStringsA(void);
BOOL  FreeEnvironmentStringsA(LPSTR);
UINT  GetWindowsDirectoryA(LPSTR, UINT);
UINT  GetSystemDirectoryA(LPSTR, UINT);
UINT  GetDriveTypeA(LPCSTR);
DWORD GetLogicalDrives(void);
DWORD GetLogicalDriveStringsA(DWORD, LPSTR);
HANDLE FindFirstFileA(LPCSTR, LPWIN32_FIND_DATAA);
BOOL  FindNextFileA(HANDLE, LPWIN32_FIND_DATAA);
BOOL  FindClose(HANDLE);
HANDLE FindFirstFileA(LPCSTR, LPWIN32_FIND_DATAA);

HGLOBAL GlobalAlloc(UINT, SIZE_T);
HGLOBAL GlobalFree(HGLOBAL);
HGLOBAL GlobalReAlloc(HGLOBAL, SIZE_T, UINT);
LPVOID GlobalLock(HGLOBAL);
BOOL   GlobalUnlock(HGLOBAL);
SIZE_T GlobalSize(HGLOBAL);
HLOCAL LocalAlloc(UINT, SIZE_T);
HLOCAL LocalFree(HLOCAL);
LPVOID VirtualAlloc(LPVOID, SIZE_T, DWORD, DWORD);
BOOL   VirtualFree(LPVOID, SIZE_T, DWORD);
BOOL   VirtualProtect(LPVOID, SIZE_T, DWORD, PDWORD);
SIZE_T VirtualQuery(LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T);

LONG  InterlockedIncrement(volatile LONG *);
LONG  InterlockedDecrement(volatile LONG *);
LONG  InterlockedExchange(volatile LONG *, LONG);
LONG  InterlockedCompareExchange(volatile LONG *, LONG, LONG);

void  GetLocalTime(LPSYSTEMTIME);
void  GetSystemTime(LPSYSTEMTIME);
BOOL  SystemTimeToFileTime(const SYSTEMTIME *, LPFILETIME);
BOOL  FileTimeToSystemTime(const FILETIME *, LPSYSTEMTIME);
DWORD GetTimeZoneInformation(LPTIME_ZONE_INFORMATION);
BOOL  SetUnhandledExceptionFilter(void *);
LONG  UnhandledExceptionFilter(EXCEPTION_POINTERS *);
PVOID AddVectoredExceptionHandler(ULONG, PVOID);
ULONG RemoveVectoredExceptionHandler(PVOID);

int   MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
int   MessageBoxW(HWND, LPCWSTR, LPCWSTR, UINT);
BOOL  MessageBeep(UINT);
DWORD GetLastError(void);

int   wsprintfA(LPSTR, LPCSTR, ...);
int   wvsprintfA(LPSTR, LPCSTR, va_list);
int   lstrlenA(LPCSTR);
LPSTR lstrcpyA(LPSTR, LPCSTR);
LPSTR lstrcpynA(LPSTR, LPCSTR, int);
LPSTR lstrcatA(LPSTR, LPCSTR);
int   lstrcmpA(LPCSTR, LPCSTR);
int   lstrcmpiA(LPCSTR, LPCSTR);
int   MulDiv(int, int, int);

/* Multimedia ------------------------------------------------------- */
MMRESULT timeGetDevCaps(LPTIMECAPS, UINT);
MMRESULT timeBeginPeriod(UINT);
MMRESULT timeEndPeriod(UINT);
UINT_PTR timeSetEvent(UINT, UINT, void *, DWORD_PTR, UINT);
MMRESULT timeKillEvent(UINT);
MMRESULT mciSendCommandA(MCIDEVICEID, UINT, DWORD_PTR, DWORD_PTR);
MMRESULT mciGetErrorStringA(MMRESULT, LPSTR, UINT);
MMRESULT midiOutOpen(HMIDIOUT *, UINT, DWORD_PTR, DWORD_PTR, DWORD);
MMRESULT midiOutClose(HMIDIOUT);
MMRESULT midiOutShortMsg(HMIDIOUT, DWORD);
MMRESULT midiOutReset(HMIDIOUT);
MMRESULT midiOutSetVolume(HMIDIOUT, DWORD);
MMRESULT midiOutGetVolume(HMIDIOUT, LPDWORD);
MMRESULT waveOutOpen(HWAVEOUT *, UINT, LPCWAVEFORMATEX, DWORD_PTR, DWORD_PTR, DWORD);
MMRESULT waveOutClose(HWAVEOUT);
MMRESULT waveOutPrepareHeader(HWAVEOUT, LPWAVEHDR, UINT);
MMRESULT waveOutUnprepareHeader(HWAVEOUT, LPWAVEHDR, UINT);
MMRESULT waveOutWrite(HWAVEOUT, LPWAVEHDR, UINT);
MMRESULT waveOutReset(HWAVEOUT);
MMRESULT waveOutSetVolume(HWAVEOUT, DWORD);
MMRESULT waveOutGetVolume(HWAVEOUT, LPDWORD);
MMRESULT waveOutGetPosition(HWAVEOUT, LPMMTIME, UINT);

/* GDI/window-entry stubs used by code that stays linked but is inactive. */
int   GetDeviceCaps(HDC, int);
BOOL  SetWindowPos(HWND, HWND, int, int, int, int, UINT);
BOOL  ShowWindow(HWND, int);
BOOL  UpdateWindow(HWND);
BOOL  DestroyWindow(HWND);
HWND  CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
BOOL  GetClientRect(HWND, LPRECT);
BOOL  GetWindowRect(HWND, LPRECT);
HDC   BeginPaint(HWND, LPPAINTSTRUCT);
BOOL  EndPaint(HWND, const PAINTSTRUCT *);
BOOL  GetUpdateRect(HWND, LPRECT, BOOL);
BOOL  InvalidateRect(HWND, const RECT *, BOOL);
BOOL  ValidateRect(HWND, const RECT *);
HCURSOR LoadCursorA(HINSTANCE, LPCSTR);
HCURSOR SetCursor(HCURSOR);
int   ShowCursor(BOOL);
BOOL  SetWindowTextA(HWND, LPCSTR);
BOOL  SetCursorPos(int, int);
BOOL  GetCursorPos(LPPOINT);
LRESULT SendMessageA(HWND, UINT, WPARAM, LPARAM);
#define BLACK_BRUSH 4
#define WHITE_BRUSH 0
#define NULL_BRUSH 5
LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
LRESULT CallWindowProcA(WNDPROC, HWND, UINT, WPARAM, LPARAM);
ATOM  RegisterClassA(const WNDCLASSA *);
BOOL  UnregisterClassA(LPCSTR, HINSTANCE);
BOOL  GetMessageA(LPMSG, HWND, UINT, UINT);
BOOL  TranslateMessage(const MSG *);
LRESULT DispatchMessageA(const MSG *);
void  PostQuitMessage(int);
BOOL  SetForegroundWindow(HWND);
HWND  SetFocus(HWND);
HWND  GetFocus(void);
BOOL  SetTimer(HWND, UINT_PTR, UINT, void *);
BOOL  KillTimer(HWND, UINT_PTR);
BOOL  ClientToScreen(HWND, LPPOINT);
BOOL  ScreenToClient(HWND, LPPOINT);
int   MapWindowPoints(HWND, HWND, LPPOINT, UINT);
HBRUSH CreateSolidBrush(COLORREF);
BOOL  DeleteObject(HGDIOBJ);
HPALETTE CreatePalette(const LOGPALETTE *);
UINT  SetPaletteEntries(HPALETTE, UINT, UINT, const PALETTEENTRY *);
HPALETTE SelectPalette(HDC, HPALETTE, BOOL);
UINT  RealizePalette(HDC);
int   GetObjectA(HANDLE, int, LPVOID);
HBITMAP CreateDIBSection(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD);
HDC   CreateCompatibleDC(HDC);
BOOL  DeleteDC(HDC);
HGDIOBJ SelectObject(HDC, HGDIOBJ);
BOOL  BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD);
BOOL  PatBlt(HDC, int, int, int, int, DWORD);
BOOL  SetBkColor(HDC, COLORREF);
BOOL  SetTextColor(HDC, COLORREF);
int   SetBkMode(HDC, int);
int   DrawTextA(HDC, LPCSTR, int, LPRECT, UINT);
BOOL  TextOutA(HDC, int, int, LPCSTR, int);
BOOL  GetTextMetricsA(HDC, LPVOID);
HFONT CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR);
BOOL  FillRect(HDC, const RECT *, HBRUSH);
BOOL  FrameRect(HDC, const RECT *, HBRUSH);
BOOL  Rectangle(HDC, int, int, int, int);
BOOL  MoveToEx(HDC, int, int, LPPOINT);
BOOL  LineTo(HDC, int, int);
int   SaveDC(HDC);
BOOL  RestoreDC(HDC, int);
BOOL  SetMapMode(HDC, int);
BOOL  SetViewportOrgEx(HDC, int, int, LPPOINT);
BOOL  SetWindowOrgEx(HDC, int, int, LPPOINT);
BOOL  SetViewportExtEx(HDC, int, int, LPSIZE);
BOOL  SetWindowExtEx(HDC, int, int, LPSIZE);
BOOL  OffsetViewportOrgEx(HDC, int, int, LPPOINT);
BOOL  OffsetWindowOrgEx(HDC, int, int, LPPOINT);
BOOL  ScaleViewportExtEx(HDC, int, int, int, int, LPSIZE);
BOOL  ScaleWindowExtEx(HDC, int, int, int, int, LPSIZE);
int   GetMapMode(HDC);
BOOL  PtVisible(HDC, int, int);
BOOL  RectVisible(HDC, const RECT *);
BOOL  GetWindowOrgEx(HDC, LPPOINT);
BOOL  GetViewportOrgEx(HDC, LPPOINT);
BOOL  GetViewportExtEx(HDC, LPSIZE);
BOOL  GetWindowExtEx(HDC, LPSIZE);
BOOL  LPtoDP(HDC, LPPOINT, int);
BOOL  DPtoLP(HDC, LPPOINT, int);
int   GetClipBox(HDC, LPRECT);
BOOL  IntersectRect(LPRECT, const RECT *, const RECT *);
BOOL  UnionRect(LPRECT, const RECT *, const RECT *);
BOOL  IsRectEmpty(const RECT *);
BOOL  SetRect(LPRECT, int, int, int, int);
BOOL  SetRectEmpty(LPRECT);
BOOL  CopyRect(LPRECT, const RECT *);
int   WinHelpA(HWND, LPCSTR, UINT, ULONG_PTR);
BOOL  ShellExecuteA(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, int);

/* ------------------------------------------------------------------ */
/* Standard C names the MinGW headers map                             */
/* ------------------------------------------------------------------ */
#ifndef _snprintf
#define _snprintf snprintf
#endif
#ifndef _vsnprintf
#define _vsnprintf vsnprintf
#endif
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif

#ifdef __cplusplus
}
#endif

#endif /* NW_LINUX_WINDOWS_H */
