/*
 * t_port_partial_compress_keep.c — Oracle harness for
 * hermes_cli/partial_compress.py:_coerce_keep
 * (ported to src/cli/commands.c as cmd_compress_coerce_keep).
 *
 * Calls the port with a set of keep-count tokens and emits case->result.
 * The Python oracle replays against LIVE Python and compares the ints.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_compress_coerce_keep_value(const char *value);

struct Case { const char *name; const char *token; };
static const struct Case CASES[] = {
    {"valid", "5"},
    {"zero", "0"},
    {"over_max", "200"},
    {"nonint", "abc"},
    {"whitespace", "  3  "},
    {"empty", ""},
    {"null", NULL},
    {"neg", "-4"},
    {"boundary_low", "1"},
    {"boundary_high", "100"},
    {"over_by_one", "101"},
};
static const int NCASES = sizeof(CASES) / sizeof(CASES[0]);

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    for (int i = 0; i < NCASES; i++) {
        int r = cmd_compress_coerce_keep_value(CASES[i].token);
        char tok_json[64];
        if (CASES[i].token)
            snprintf(tok_json, sizeof(tok_json), "\"%s\"", CASES[i].token);
        else
            snprintf(tok_json, sizeof(tok_json), "null");
        printf("{\"case\":\"%s\",\"token\":%s,\"ret\":%d}\n",
               CASES[i].name, tok_json, r);
    }
    return 0;
}
