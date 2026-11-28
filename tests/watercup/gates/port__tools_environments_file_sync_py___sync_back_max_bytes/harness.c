/* gate: _sync_back_max_bytes() with config terminal.sync_back_max_bytes = fixture. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
long long cli_tools_environments_file_sync_sync_back_max_bytes_resolve(const json_t *raw);
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_sync_back_max_bytes");
        json_t *raw = json_parse(argv[i], NULL);
        long long v = cli_tools_environments_file_sync_sync_back_max_bytes_resolve(raw);
        char num[32]; snprintf(num, sizeof num, "%lld", v);
        WCUP_VALSTR(num);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_sync_back_max_bytes");
        if (raw) json_free(raw);
    }
    return 0;
}
