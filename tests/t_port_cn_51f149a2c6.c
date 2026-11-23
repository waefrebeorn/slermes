#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include "noc_cn_51f149a2c6.h"
int main(void) {
    char line[8192];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *out = cn_51f149a2c6_constants();
        char *s = json_dumps(out, 0);
        printf("{\"func\":\"constants\",\"out\":%s}\n", s ? s : "null");
        free(s); json_free(out); json_free(in);
    }
    return 0;
}
