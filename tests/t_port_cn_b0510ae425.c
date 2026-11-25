#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include "noc_cn_b0510ae425.h"
int main(void) {
    char line[8192];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *out = cn_b0510ae425_constants();
        char *s = json_dumps(out, 0);
        printf("{\"func\":\"constants\",\"out\":%s}\n", s ? s : "null");
        free(s); json_free(out); json_free(in);
    }
    return 0;
}
