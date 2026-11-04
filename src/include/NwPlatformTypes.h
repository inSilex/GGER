/*
 * NwPlatformTypes -- neutral, platform-independent vocabulary shared by the
 * window, presenter, and input backends.
 *
 * This is NOT an abstraction layer.  Backends are selected at build time in
 * platform build and implement the same function symbols
 * in their own source files.  These typedefs exist only so shared headers do
 * not leak HWND/WPARAM/LPARAM; they are plain integers and pointers.
 *
 * At SDL-only retirement this header can disappear: the remaining call sites
 * can use the SDL types directly.
 */
#ifndef NW_PLATFORM_TYPES_H
#define NW_PLATFORM_TYPES_H

#include <stdint.h>

typedef void     *NwWindowHandle;   /* HWND        / SDL_Window *          */
typedef unsigned  NwMessageCode;    /* WM_*        / internal event kind   */
typedef uintptr_t NwWordParam;      /* WPARAM                              */
typedef intptr_t  NwLongParam;      /* LPARAM                              */

#endif /* NW_PLATFORM_TYPES_H */
