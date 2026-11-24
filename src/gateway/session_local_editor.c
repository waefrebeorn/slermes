/*
 * session_local_editor.c - C home for upstream Hermes gateway/session_local_editor.py.
 * PoP: w16_sessionloc_eaa05a_validate_editor @ gateway/session_local_editor.py:validate_editor
 *
 * Faithful port of the create-path totality check. Each refusal branch reports
 * the same (reason, field, detail) the live AuthorityRefusal carries. The
 * downstream mcp-server validation is the caller's concern (a no-op stub in the
 * oracle), matching validate_servers' success path.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_sessionloc_eaa05a.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static const char *type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER:
            return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}

static json_t *ok_result(void) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(1));
    return o;
}

static json_t *refuse(const char *field, const char *detail) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(0));
    json_set(o, "reason", json_string("invalid_params"));
    json_set(o, "field", json_string(field));
    json_set(o, "detail", json_string(detail));
    return o;
}

static int cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);
}

struct json_t *w16_sessionloc_eaa05a_validate_editor(const struct json_t *source,
                                                     const struct json_t *editor) {
    char detail[512];
    if (!editor || editor->type == JSON_NULL) return ok_result();

    const char *src = (source && source->type == JSON_STRING && source->str_val) ? source->str_val
                    : (source && source->type == JSON_NULL) ? "None" : "";
    if (strcmp(src, "acp") != 0) {
        snprintf(detail, sizeof(detail),
                 "an editor policy is only accepted from an ACP launch, not %s", src);
        return refuse("editor", detail);
    }
    if (editor->type != JSON_OBJECT) {
        snprintf(detail, sizeof(detail), "must be a mapping, got %s", type_name(editor));
        return refuse("editor", detail);
    }

    /* unknown = sorted(set(editor) - {'mcp_servers', 'edit_approval_policy'}) */
    size_t n = json_object_size(editor);
    char **unknown = (char **)calloc(n ? n : 1, sizeof(char *));
    size_t un = 0;
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(editor, i);
        if (!k) continue;
        if (!strcmp(k, "mcp_servers") || !strcmp(k, "edit_approval_policy")) continue;
        unknown[un++] = dupstr(k);
    }
    if (un > 0) {
        qsort(unknown, un, sizeof(char *), cmp_str);
        snprintf(detail, sizeof(detail), "editor.%s", unknown[0]);
        char *field = dupstr(detail);
        for (size_t i = 0; i < un; i++) free(unknown[i]);
        free(unknown);
        json_t *r = refuse(field, "unknown editor key");
        free(field);
        return r;
    }
    free(unknown);

    const json_t *eap = json_obj_get(editor, "edit_approval_policy");
    if (eap && !(eap->type == JSON_STRING && eap->str_val && !strcmp(eap->str_val, "ask")))
        return refuse("editor.edit_approval_policy",
                      "must be 'ask': every edit is approved one at a time");

    const json_t *mcp = json_obj_get(editor, "mcp_servers");
    if (mcp && mcp->type != JSON_ARRAY) {
        snprintf(detail, sizeof(detail), "must be a list, got %s", type_name(mcp));
        return refuse("editor.mcp_servers", detail);
    }
    return ok_result();
}
