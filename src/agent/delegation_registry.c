/*
 * delegation_registry.c - C home for upstream Hermes agent/delegation_registry.py.
 * PoP: w11_delegation_c4a4e9_configured_backend @ agent/delegation_registry.py:configured_backend
 * PoP: w11_delegation_c4a4e9_lower_key @ agent/provider_registry.py:lower_key
 * PoP: w11_delegation_c4a4e9_bundled_name_matches @ agent/delegation_registry.py:_BUNDLED_NAME_RE
 *
 * Module-level literal constants mirrored from the Python source (provenance):
 *   DEFAULT_BACKEND = "default"
 *   _BUNDLED_NAME_RE = re.compile(r"^[a-z0-9][a-z0-9_-]{0,63}$")
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_delegation_c4a4e9.h"

static const char *const W11DR_DEFAULT_BACKEND = "default";
static int w11dr_ws(unsigned char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}
static char *w11dr_strip(const char *s) {
    if (!s) return (char *)calloc(1,1);
    size_t n=strlen(s), a=0, b=n;
    while (a<b && w11dr_ws((unsigned char)s[a])) a++;
    while (b>a && w11dr_ws((unsigned char)s[b-1])) b--;
    char *o=(char *)malloc(b-a+1); memcpy(o,s+a,b-a); o[b-a]=0; return o;
}
static void w11dr_lower(char *s) { for (; s && *s; s++) if (*s>='A' && *s<='Z') *s=(char)(*s+32); }

/* lower_key(name) = name.strip().lower() -- mirrors agent/provider_registry.py. */
char *w11_delegation_c4a4e9_lower_key(const char *name) {
    char *t=w11dr_strip(name); w11dr_lower(t); return t;
}
/* configured_backend: lower_key(raw) when raw is a non-blank str else "default". */
char *w11_delegation_c4a4e9_configured_backend(json_t *config) {
    if (!config || config->type!=JSON_OBJECT) return strdup(W11DR_DEFAULT_BACKEND);
    json_t *raw=json_obj_get(config,"backend");
    if (raw && raw->type==JSON_STRING) {
        char *t=w11dr_strip(raw->str_val);
        if (*t) { w11dr_lower(t); return t; }
        free(t);
    }
    return strdup(W11DR_DEFAULT_BACKEND);
}
/* bool(_BUNDLED_NAME_RE.match(name)) -- Python '$' also matches before one trailing '\n'. */
int w11_delegation_c4a4e9_bundled_name_matches(const char *name) {
    if (!name) return 0;
    size_t n=strlen(name);
    if (n>0 && name[n-1]=='\n') n--;
    if (n<1 || n>64) return 0;
    char c=name[0];
    if (!((c>='a'&&c<='z')||(c>='0'&&c<='9'))) return 0;
    for (size_t i=1;i<n;i++) {
        c=name[i];
        if (!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return 0;
    }
    return 1;
}
