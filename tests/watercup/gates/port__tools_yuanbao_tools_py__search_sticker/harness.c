/* gate: port__tools_yuanbao_tools_py__search_sticker — traced copy of cli_tools_yuanbao_tools_search_sticker (json_ret) */
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


extern json_node_t* cli_tools_yuanbao_tools_search_sticker(const char *query, int limit);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_yuanbao_tools_search_sticker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_yuanbao_tools_search_sticker");
    fflush(stdout);
    int _sv 