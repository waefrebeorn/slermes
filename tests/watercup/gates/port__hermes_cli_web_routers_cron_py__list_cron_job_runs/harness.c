/* gate: port__hermes_cli_web_routers_cron_py__list_cron_job_runs — traced copy of ws_list_cron_job_runs (json_ret) */
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


extern json_t * ws_list_cron_job_runs(const char *job_id, const char *profile, int limit);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_list_cron_job_runs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ws_list_cron_job_runs");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    json_t * result = ws_list_cron_job_runs(argv[i], argv[2 + (i - 2 + 1) % 5], (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char *_js = result ? json_serialize(result) : NULL;
    WCUP_VALSTR(_js ? _js : "null");
    if (result) json_free(result);
    if (_js) free(_js);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ws_list_cron_job_runs");
    }
    return 0;
}
