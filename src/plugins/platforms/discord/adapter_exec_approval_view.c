
/*
 * adapter_exec_approval_view.c - real C11 port of the Discord exec-approval card's
 * pure decision surface: the __init__ button set, the _check_auth admission gate,
 * retire()'s footer key and _resolve()'s branch outcome. Everything else in the
 * module is discord.py plumbing (decorators, embeds, awaits).
 * PoP: w19_adapterexe_bc6b62_init_buttons @ plugins/platforms/discord/adapter_exec_approval_view.py:build_exec_approval_view.ExecApprovalView.__init__
 * PoP: w19_adapterexe_bc6b62_check_auth @ plugins/platforms/discord/adapter_exec_approval_view.py:build_exec_approval_view.ExecApprovalView._check_auth
 * PoP: w19_adapterexe_bc6b62_footer_key @ plugins/platforms/discord/adapter_exec_approval_view.py:build_exec_approval_view.ExecApprovalView.retire
 * PoP: w19_adapterexe_bc6b62_resolve_outcome @ plugins/platforms/discord/adapter_exec_approval_view.py:build_exec_approval_view.ExecApprovalView._resolve
 */
#include "hermes_json.h"
#include "noc_w19_adapterexe_bc6b62.h"
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* if smart_denied or not allow_session: drop session+always; elif not allow_permanent: drop always;
 * if not allow_once: drop once. */
json_t *w19_adapterexe_bc6b62_init_buttons(int allow_once, int allow_session,
                                          int allow_permanent, int smart_denied) {
    json_t *out = json_array();
    if (smart_denied || !allow_session) {
        json_append(out, json_string("allow_session"));
        json_append(out, json_string("allow_always"));
    } else if (!allow_permanent) {
        json_append(out, json_string("allow_always"));
    }
    if (!allow_once) json_append(out, json_string("allow_once"));
    return out;
}

/* str(a).strip() for every configured admin id, dropping the empty ones. */
static int admin_matches(const json_t *admin_ids, const char *uid, size_t *count_out) {
    size_t n = 0;
    int hit = 0;
    if (admin_ids && admin_ids->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(admin_ids); i++) {
            json_t *a = json_get(admin_ids, i);
            if (!a || a->type != JSON_STRING || !a->str_val) continue;
            const char *s = a->str_val;
            size_t len = strlen(s);
            while (len && (s[0] == ' ' || s[0] == '\t' || s[0] == '\n' || s[0] == '\r')) { s++; len--; }
            while (len && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\n' || s[len - 1] == '\r')) len--;
            if (!len) continue;
            n++;
            if (uid && strlen(uid) == len && !strncmp(uid, s, len)) hit = 1;
        }
    }
    if (count_out) *count_out = n;
    return hit;
}

json_t *w19_adapterexe_bc6b62_check_auth(int base_ok, int require_admin,
                                        json_t *admin_ids, const char *uid) {
    json_t *out = json_object();
    size_t admins = 0;
    int warned = 0;
    if (!base_ok) {
        json_set(out, "ok", json_bool(false));
        json_set(out, "warned", json_bool(false));
        return out;
    }
    if (!require_admin) {
        json_set(out, "ok", json_bool(true));
        json_set(out, "warned", json_bool(false));
        return out;
    }
    if (admin_matches(admin_ids, uid && uid[0] ? uid : NULL, &admins)) {
        json_set(out, "ok", json_bool(true));
        json_set(out, "warned", json_bool(false));
        return out;
    }
    warned = admins == 0 ? 1 : 0;   /* fails closed, logged once, only with no admins */
    json_set(out, "ok", json_bool(false));
    json_set(out, "warned", json_bool(warned ? true : false));
    return out;
}

const char *w19_adapterexe_bc6b62_footer_key(const char *reason) {
    const char *r = reason ? reason : "";
    if (!strcmp(r, "resolved")) return "platform.discord.approval.already_resolved";
    if (!strcmp(r, "timeout")) return "platform.discord.prompt.expired_footer";
    if (!strcmp(r, "gone")) return "platform.discord.approval.no_longer_pending";
    return "platform.discord.slash_confirm.resolved_cancel";
}

/* Branch order: gate, refusal, resolve failure, no-count (request id, else expired), finalize. */
json_t *w19_adapterexe_bc6b62_resolve_outcome(int gate_ok, int refusal, int resolve_raises,
                                             int count, int request_id_present,
                                             const char *color_name) {
    json_t *out = json_object();
    json_t *actions = json_array();
    const char *final_color = NULL;
    const char *final_text = NULL;
    int resolved = 0, settled = 0;

    if (!gate_ok) {
        json_append(actions, json_string("return"));
    } else if (refusal) {
        json_append(actions, json_string("refuse"));
    } else if (resolve_raises) {
        json_append(actions, json_string("click_failed"));
    } else {
        resolved = 1;
        settled = 1;
        if (!count) {
            settled = 0;
            if (request_id_present) {
                json_append(actions, json_string("no_longer_pending"));
            } else {
                json_append(actions, json_string("finalize"));
                final_color = "dark_grey";
                final_text = "platform.discord.approval.expired";
            }
        } else {
            json_append(actions, json_string("finalize"));
            final_color = color_name ? color_name : "";
            final_text = "platform.discord.approval.by_user";
        }
    }
    json_set(out, "actions", actions);
    json_set(out, "finalize_color", final_color ? json_string(final_color) : json_null());
    json_set(out, "finalize_text", final_text ? json_string(final_text) : json_null());
    json_set(out, "resolved", json_bool(resolved ? true : false));
    json_set(out, "backend_settled", json_bool(settled ? true : false));
    (void)py_truthy;
    return out;
}
