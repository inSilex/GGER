#include <windows.h>

extern void NwWin64Init(void);
extern int InitializeAndRunWinMain(void);

#ifdef _MSC_VER
/* Let the MSVC CRT initialize itself (heap, stdio, constructors) before the
 * recovered WinMain body runs. */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    NwWin64Init();
    return InitializeAndRunWinMain();
}
#else
void NwWin64Entry(void)
{
    NwWin64Init();
    ExitProcess((UINT)InitializeAndRunWinMain());
}
#endif
