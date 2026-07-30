/*
 * t_port_video_gen_cache_dir.c — Oracle harness for
 * agent/video_gen_provider.py:_videos_cache_dir
 * (ported to src/tools/video_gen.c as video_gen_cache_dir).
 *
 * Calls video_gen_cache_dir() (which respects HERMES_HOME via hermes_cache_dir)
 * and emits the returned path + whether it exists. The Python oracle runs in
 * the SAME HERMES_HOME and compares the path string and existence.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

extern char *video_gen_cache_dir(void);

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    /* C's home resolver reads SLERMES_HOME (fallback $HOME/.slermes); the test
     * runner exports HERMES_HOME. Align them so C and Python resolve to the
     * same temp root. */
    const char *h = getenv("HERMES_HOME");
    if (h) setenv("SLERMES_HOME", h, 1);
    char *dir = video_gen_cache_dir();
    if (!dir) { printf("{\"path\":null,\"exists\":false}\n"); return 1; }
    struct stat st;
    int exists = (stat(dir, &st) == 0 && S_ISDIR(st.st_mode));
    char buf[8192];
    int n = snprintf(buf, sizeof(buf),
                     "{\"path\":\"%s\",\"exists\":%s}",
                     dir, exists ? "true" : "false");
    printf("%s\n", n > 0 ? buf : "");
    free(dir);
    return 0;
}
