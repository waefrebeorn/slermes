/* t_port_owner_matches.c — harness for skills_hub_gaps.c owner_from_payload
 * and owner_matches. Reads {"expected":<str|null>,"data":<obj|null>}, emits
 * {"in":{...},"owner":<str|null>,"out":0|1}. Oracle ast-extracts both LIVE defs.
 * Gate: run_one_oracle.sh owner_matches oracle_owner_matches.py ""
 *       "src/tools/skills_hub_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *owner_from_payload(const json_t *data);
int owner_matches(const char *expected_owner, const json_t *data);

static void jstr(const char *s) {
    putchar('"');
    for (const unsigned char *p=(const unsigned char*)(s?s:""); *p; p++) {
        if (*p=='"') fputs("\\\"",stdout);
        else if (*p=='\\') fputs("\\\\",stdout);
        else if (*p<0x20) printf("\\u%04x",*p);
        else putchar(*p);
    }
    putchar('"');
}

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *exp = json_get_str(in, "expected", NULL);
        json_t *data = json_obj_get(in, "data");  /* borrowed */
        char *owner = owner_from_payload(data);
        int out = owner_matches(exp, data);
        printf("{\"expected\":"); if (exp) jstr(exp); else fputs("null",stdout);
        printf(",\"data\":");
        if (data) { char *s=json_dumps(data,0); if(s){fputs(s,stdout);free(s);} else fputs("null",stdout); }
        else fputs("null",stdout);
        printf(",\"owner\":"); if (owner) { jstr(owner); free(owner); } else fputs("null",stdout);
        printf(",\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
