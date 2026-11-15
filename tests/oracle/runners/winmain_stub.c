/* SDL2main references WinMain, so linking the real slermes object closure
 * requires a WinMain symbol. The oracle harness supplies main() itself;
 * src/main.o also supplies main(), and linking both lets src/main.o win --
 * the harness then prints the agent banner instead of its JSON.
 *
 * This stub satisfies WinMain without pulling in src/main.o, so the harness's
 * main() is the one that runs. __argc/__argv are provided by the mingw CRT.
 */
#include <stdlib.h>   /* mingw's __argc / __argv */

int main(int argc, char **argv);

int WinMain(void *hInstance, void *hPrevInstance, char *lpCmdLine,
            int nShowCmd)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;
    return main(__argc, __argv);
}
