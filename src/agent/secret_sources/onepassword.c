#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Standalone C home: onepassword CLI error classification. */

/* PoP: agent_secret_sources_onepassword_u_classify_op_error
 * @ agent/secret_sources/onepassword.py:_classify_op_error
 * classify_cli_error(message, _OP_ERROR_RULES). Note this table has "resolve host"
 * (not bare "resolve") and no "download" row, so those answer "internal". */
typedef struct { const char *kind; const char *tok[8]; int nt; } _op_rule;
static const _op_rule OP_RULES[] = {
  {"timeout", {"timed out"}, 1},
  {"binary_missing", {"not found on path","not an executable","failed to invoke"}, 3},
  {"auth_failed", {"unauthorized","not signed in","session expired","authentication","401","403"}, 6},
  {"empty_value", {"empty value"}, 1},
  {"network", {"network","connection","resolve host","dns"}, 4},
};
char *agent_secret_sources_onepassword_u_classify_op_error(const char *arg) {
    const char *m = arg ? arg : "";
    char *low = malloc(strlen(m) + 1); size_t w = 0;
    for (const char *p = m; *p; p++) { char c = *p; low[w++] = (c>='A'&&c<='Z') ? (char)(c+32) : c; }
    low[w] = 0;
    for (int i = 0; i < (int)(sizeof(OP_RULES)/sizeof(OP_RULES[0])); i++)
        for (int k = 0; k < OP_RULES[i].nt; k++)
            if (strstr(low, OP_RULES[i].tok[k])) { free(low); return strdup(OP_RULES[i].kind); }
    free(low);
    return strdup("internal");
}
