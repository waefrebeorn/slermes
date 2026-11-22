/* t_port_validate_page.c — harness for hosted_room_replicas_gaps.c validate_page.
 * Reads {"page":<json>}, emits {"page":...,"ok":0|1,"err":"Class: msg"}.
 * Gate: run_one_oracle.sh validate_page oracle_validate_page.py ""
 *       "src/gateway/hosted_room_replicas_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int validate_page(const json_t *page, char *err, size_t errcap);

int main(void) {
    char line[16384];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *page = json_obj_get(in, "page");
        char err[512] = "";
        int ok = validate_page(page, err, sizeof(err));
        printf("{\"page\":");
        char *s = json_dumps(page ? page : json_null(), 0);
        if (s) { fputs(s, stdout); free(s); } else fputs("null", stdout);
        printf(",\"ok\":%d,\"err\":", ok);
        json_t *ej = json_string(err);
        char *e = json_dumps(ej, 0);
        if (e) { fputs(e, stdout); free(e); } else fputs("\"\"", stdout);
        json_free(ej);
        printf("}\n");
        json_free(in);
    }
    return 0;
}
