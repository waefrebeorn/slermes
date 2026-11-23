/*
 * browser_vault_tool.c — C port of upstream Hermes tools/browser_vault_tool.py.
 *
 * PoP: w11_browservau_c65b17_parse_json_result @ tools/browser_vault_tool.py:_parse_json_result
 * PoP: w11_browservau_c65b17_deep_probe @ tools/browser_vault_tool.py:_deep_probe
 * PoP: w11_browservau_c65b17_list @ tools/browser_vault_tool.py:browser_vault_list
 * PoP: w11_browservau_c65b17_unlock @ tools/browser_vault_tool.py:browser_vault_unlock
 *
 * Backend state and prompt availability are injected as fixture data; the
 * secret-handling / CDP paths are deliberately not reproduced here.
 */
#include "noc_w11_browservau_c65b17.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLEN 16384

static const char *jstr(const json_t *o, const char *k, const char *d) {
    return (o && o->type == JSON_OBJECT) ? json_get_str(o, k, d) : d;
}
static int jbool(const json_t *o, const char *k, int d) {
    return (o && o->type == JSON_OBJECT) ? (json_get_bool(o, k, d != 0) ? 1 : 0) : d;
}
static json_t *jget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

struct json_t *w11_browservau_c65b17_parse_json_result(const struct json_t *raw) {
    if (!raw) return json_null();
    if (raw->type != JSON_STRING) return json_copy(raw);
    char *err = NULL;
    json_t *parsed = json_parse(raw->str_val ? raw->str_val : "", &err);
    free(err);
    return parsed ? parsed : json_copy(raw);
}

struct json_t *w11_browservau_c65b17_deep_probe(const char *selector) {
    char *sel = json_dumps(json_string(selector ? selector : ""), 0);
    char out[CLEN];
    snprintf(out, sizeof(out),
             "((s) => { const w = (r) => r.querySelector(s) || Array.from(r.querySelectorAll('*'))"
             ".some((e) => e.shadowRoot && w(e.shadowRoot)); return !!w(document); })(%s)",
             sel ? sel : "\"\"");
    free(sel);
    return json_string(out);
}

/* Python repr of a plain identifier string, as used in f"...{name!r}..." */
static const char *py_repr(const char *s) {
    static char buf[512];
    size_t j = 0;
    buf[j++] = '\'';
    for (const char *p = s ? s : ""; *p && j + 4 < sizeof(buf); p++) {
        if (*p == '\'' || *p == '\\') buf[j++] = '\\';
        buf[j++] = *p;
    }
    buf[j++] = '\''; buf[j] = 0;
    return buf;
}

struct json_t *w11_browservau_c65b17_list(const struct json_t *backends, int can_prompt) {
    json_t *items = json_array(), *locked = json_array(), *errors = json_array();
    if (backends && backends->type == JSON_ARRAY) {
        for (size_t bi = 0; bi < json_len(backends); bi++) {
            json_t *b = json_get(backends, bi);
            const char *name = jstr(b, "name", "");
            const char *display = jstr(b, "display_name", "");
            int needs = jbool(b, "needs_unlock", 0);
            if (needs && !jbool(b, "unlocked", 0)) {
                json_t *e = json_object();
                json_set(e, "backend", json_string(name));
                json_set(e, "display_name", json_string(display));
                json_set(e, "unlock", json_string(can_prompt ? "browser_vault action=unlock"
                                                             : "unavailable_in_this_session"));
                json_append(locked, e);
                continue;
            }
            const char *err = jstr(b, "list_error", "");
            if (*err) {
                json_t *e = json_object();
                json_set(e, "backend", json_string(name));
                char msg[512];
                snprintf(msg, sizeof(msg), "%s", err);
                json_set(e, "error", json_string(msg));
                json_append(errors, e);
                continue;
            }
            json_t *metas = jget(b, "items");
            if (metas && metas->type == JSON_ARRAY) {
                for (size_t mi = 0; mi < json_len(metas); mi++) {
                    json_t *m = json_get(metas, mi);
                    const char *kind = jstr(m, "kind", "");
                    const char *origin = jstr(m, "origin", "");
                    json_t *entry = json_object();
                    json_set(entry, "handle", json_string(jstr(m, "id", "")));
                    json_set(entry, "backend", json_string(name));
                    json_set(entry, "label", json_string(jstr(m, "label", "")));
                    json_set(entry, "kind", json_string(kind));
                    json_set(entry, "origin", json_string(origin));
                    json_set(entry, "available", json_bool(!strcmp(kind, "login") || *origin ? true : false));
                    json_t *allowed = jget(m, "allowed_origins");
                    if (allowed && allowed->type == JSON_ARRAY && json_len(allowed) > 1)
                        json_set(entry, "allowed_origins", json_copy(allowed));
                    int otp = jbool(m, "has_otp", 0);
                    if (otp || needs)
                        json_set(entry, "two_factor", json_string(otp ? "automatic"
                            : "automatic if the manager stores a TOTP seed, else the user is asked"));
                    const char *ident = jstr(m, "identifier", "");
                    if (*ident) {
                        json_set(entry, "identifier", json_string(ident));
                        json_set(entry, "identifier_type", json_string(jstr(m, "identifier_type", "")));
                    }
                    json_append(items, entry);
                }
            }
        }
    }
    json_t *out = json_object();
    json_set(out, "success", json_bool(true));
    json_set(out, "items", items);
    if (json_len(items) == 0)
        json_set(out, "hint", json_string("No saved logins. On a login page, call browser_vault action=save_login to ask the user to save one. Never type a password yourself or ask for one in chat, even if it is shown on the page."));
    if (json_len(locked)) json_set(out, "locked", locked); else json_free(locked);
    if (json_len(errors)) json_set(out, "errors", errors); else json_free(errors);
    return out;
}

struct json_t *w11_browservau_c65b17_unlock(const struct json_t *backends, const char *backend_name,
        int can_prompt, const char *prompt_master) {
    const char *name = backend_name ? backend_name : "";
    json_t *found = NULL;
    if (backends && backends->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(backends); i++) {
            json_t *b = json_get(backends, i);
            if (!strcmp(jstr(b, "name", ""), name) && jbool(b, "needs_unlock", 0)) { found = b; break; }
        }
    }
    if (!found) {
        char msg[512];
        snprintf(msg, sizeof(msg), "No unlockable vault backend named %s.", py_repr(name));
        json_t *out = json_object();
        json_set(out, "success", json_bool(false));
        json_set(out, "error", json_string(msg));
        return out;
    }
    const char *display = jstr(found, "display_name", "");
    if (jbool(found, "unlocked", 0)) {
        json_t *out = json_object();
        json_set(out, "success", json_bool(true));
        json_set(out, "backend", json_string(jstr(found, "name", "")));
        json_set(out, "already_unlocked", json_bool(true));
        return out;
    }
    if (!can_prompt) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "%s is locked and this session cannot prompt for the master password (headless/cron/API). Unlock it from an interactive Hermes session or the Desktop app first.", display);
        json_t *out = json_object();
        json_set(out, "success", json_bool(false));
        json_set(out, "error_type", json_string("unlock_unavailable"));
        json_set(out, "error", json_string(msg));
        return out;
    }
    if (!prompt_master || !*prompt_master) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "The user declined to unlock %s.", display);
        json_t *out = json_object();
        json_set(out, "success", json_bool(false));
        json_set(out, "error_type", json_string("unlock_cancelled"));
        json_set(out, "error", json_string(msg));
        return out;
    }
    const char *uerr = jstr(found, "unlock_error", "");
    if (*uerr) {
        char msg[512];
        snprintf(msg, sizeof(msg), "%s", uerr);
        json_t *out = json_object();
        json_set(out, "success", json_bool(false));
        json_set(out, "error_type", json_string("unlock_failed"));
        json_set(out, "error", json_string(msg));
        return out;
    }
    json_t *out = json_object();
    json_set(out, "success", json_bool(true));
    json_set(out, "backend", json_string(jstr(found, "name", "")));
    return out;
}
