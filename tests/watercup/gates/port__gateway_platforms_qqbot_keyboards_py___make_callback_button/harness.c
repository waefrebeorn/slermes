/* gate: port__gateway_platforms_qqbot_keyboards_py___make_callback_button — traced copy of cli_gateway_platforms_qqbot_keyboards__make_callback_button (json_ret) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"
#include "json.h"
extern void json_free(json_t *);
extern char *json_serialize(const json_t *);
extern json_t *json_parse(const char *, char **);


extern json_node_t* cli_gateway_platforms_qqbot_keyboards__make_callback_button(const char *btn_id, const char *label,
                                                                          const char *visited_label, const char *data,
                                                                          int style, const char *group_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_platforms_qqbot_keyboards__make_callback_button <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_gateway_platforms_qqbot_keyboards__make_callback_button");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    json_node_t* result = cli_gateway_platforms_qqbot_keyboards__make_callback_button(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), argv[2 + (i - 2 + 5) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char *_js = result ? json_serialize(result) : NULL;
    WCUP_VALSTR(_js ? _js : "null");
    if (result) json_free(result);
    if (_js) free(_js);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_gateway_platforms_qqbot_keyboards__make_callback_button");
    }
    return 0;
}
