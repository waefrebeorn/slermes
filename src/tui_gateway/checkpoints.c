/*
 * checkpoints.c — faithful C11 port of tui_gateway/checkpoints.py.
 * Gate w10_checkpoints_fc7424.
 *
 * PoP: w10_checkpoints_fc7424_load_enabled @ tui_gateway/checkpoints.py:_load_checkpoints_enabled
 * PoP: w10_checkpoints_fc7424_resolve_enabled_ex @ tui_gateway/checkpoints.py:resolve_checkpoints_enabled
 * PoP: w10_checkpoints_fc7424_undo_hint_fields @ tui_gateway/checkpoints.py:undo_hint_fields
 */
#include "hermes_json.h"
#include "noc_w10_checkpoints_fc7424.h"
#include <string.h>

/* Python truthiness for JSON values. */
static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

static int str_in(const char *s, const char *const *set, size_t n) {
    for (size_t i = 0; i < n; i++) if (!strcmp(s, set[i])) return 1;
    return 0;
}

/* utils.is_truthy_value(value, default=False) */
static int is_truthy_value(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f') s++;
        size_t n = strlen(s);
        while (n && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]=='\v'||s[n-1]=='\f')) n--;
        char buf[32];
        size_t k = n < sizeof(buf) - 1 ? n : sizeof(buf) - 1;
        for (size_t i = 0; i < k; i++) {
            char c = s[i];
            buf[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
        }
        buf[k] = '\0';
        static const char *const truthy[] = {"1", "true", "yes", "on"};
        return str_in(buf, truthy, 4);
    }
    return py_truthy(v);
}

int w10_checkpoints_fc7424_load_enabled(const json_t *cfg) {
    const json_t *cp = NULL;
    if (cfg && py_truthy(cfg) && cfg->type == JSON_OBJECT)
        cp = json_obj_get(cfg, "checkpoints");
    int enabled = 0; /* DEFAULT_CONFIG["checkpoints"]["enabled"] == False */
    if (cp) {
        if (cp->type == JSON_BOOL) {
            enabled = cp->bool_val ? 1 : 0;
        } else if (cp->type == JSON_OBJECT) {
            const json_t *e = json_obj_get(cp, "enabled");
            enabled = py_truthy(e);
        }
    }
    return enabled;
}

int w10_checkpoints_fc7424_resolve_enabled_ex(const json_t *cfg, const json_t *env_value) {
    if (env_value && env_value->type != JSON_NULL) return is_truthy_value(env_value); /* explicit opt-in wins */
    return w10_checkpoints_fc7424_load_enabled(cfg);
}

json_t *w10_checkpoints_fc7424_undo_hint_fields(const json_t *session,
        const json_t *rewound, const json_t *params, const struct json_t *hint_seam) {
    json_t *empty = json_object();
    /* if not rewound or not rewound[2] or params.get("intent") == "retry": return {} */
    if (!py_truthy(rewound) || !rewound || rewound->type != JSON_ARRAY || json_len(rewound) < 3)
        return empty;
    if (!py_truthy(json_get(rewound, 2))) return empty;
    const char *intent = json_get_str(params, "intent", "");
    if (intent && !strcmp(intent, "retry")) return empty;
    /* undo_rollback_hint_for: mgr = getattr(session.get("agent"), "_checkpoint_mgr", None);
     * if mgr is None or not mgr.enabled: return None */
    const json_t *agent = session ? json_obj_get(session, "agent") : NULL;
    if (!agent || agent->type != JSON_OBJECT) return empty;
    const json_t *mgr = json_obj_get(agent, "_checkpoint_mgr");
    if (!mgr || mgr->type != JSON_OBJECT || !py_truthy(json_obj_get(mgr, "enabled"))) return empty;
    if (py_truthy(hint_seam)) {
        json_t *out = json_object();
        json_set(out, "undo_hint", json_copy(hint_seam));
        return out;
    }
    return empty;
}
