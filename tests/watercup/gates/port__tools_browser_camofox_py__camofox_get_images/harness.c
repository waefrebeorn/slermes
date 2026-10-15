/* gate: port__tools_browser_camofox_py__camofox_get_images — traced copy of camofox_get_images (json_ret) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"
#include "json.h"
extern void json_free(json_t *);
extern char *json_serialize(const json_t *);


extern json_t * camofox_get_images(const char *camofox_url, const char *session_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s camofox_get_images <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("camofox_get_images");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    json_t * result = camofox_get_images(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char *_js = result ? json_serialize(result) : NULL;
    WCUP_VALSTR(_js ? _js : "null");
    if (result) json_free(result);
    if (_js) free(_js);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("camofox_get_images");
    }
    return 0;
}
