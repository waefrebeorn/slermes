/*
 * browser_vault.c — the `browser_vault` tool: what is in the vault, and filling a login into the page
 * that is open right now.
 *
 * This is the one place where the layers meet, and every layer underneath is separately proven:
 *
 *   fernet            the token format, against the spec's own vectors and cryptography.Fernet
 *   vault_file        the encrypted document, read back by Python's VaultStore
 *   vault_local_store the local backend answering list/get/resolve over those files
 *   browser_vault_list the listing shape the model sees
 *   vault_origin_guard is this page an origin the item was saved for
 *   classifier        which control is the password, and what the identifier is
 *   fill plan         which fill goes to which stamped control, and the JS that applies it
 *
 * What was missing was the join: a caller could not ask "fill my login for this site". That is all this
 * file adds, plus the one thing none of the layers can do alone -- reaching the page.
 *
 * Actions: list, fill. Anything else answers with an error naming the action, because a tool that
 * accepts a verb it cannot perform teaches the model to trust a lie.
 */
#include "browser_vault.h"

#include "hermes_core_types.h"   /* hermes_get_home(char *, size_t) */

#include "browser_vault_list.h"
#include "browser_vault_fill_plan.h"
#include "hermes_json.h"
#include "browser_tool_cdp.h"
#include "port_vault_login_classifier.h"
#include "vault_local_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The classifier verbs keep their PoP names; the header exposes the ergonomic wrappers, not these. */
extern json_t *w16_vaultlogin_58f247_classify_login(const json_t *control);
extern json_t *w16_vaultlogin_58f247_select_password(const json_t *classified, const char *password);

static char *vault_base(void) {
    /* hermes_get_home fills a caller buffer: it is `void hermes_get_home(char *, size_t)`, not a
     * getter returning a pointer. Declaring it the other way round links happily and crashes the first
     * time it is called. Same resolution as every other module: SLERMES_HOME, else ~/.slermes/~/.hermes. */
    /* An explicit vault directory wins: it lets a test point the tool at a fixture instead of the real
     * vault, and lets a user keep their vault somewhere other than the hermes home. */
    const char *override = getenv("HERMES_VAULT_DIR");
    if (override && *override) return strdup(override);

    char home[512];
    hermes_get_home(home, sizeof home);
    if (!home[0]) return NULL;
    size_t n = strlen(home) + 8;
    char *base = (char *)malloc(n);
    if (base) snprintf(base, n, "%s%cvault", home, '/');
    return base;
}

/* ── one page question, one page action ─────────────────────────────────────────────────────────── */

/* Evaluate an expression in the attached page and return its value as a string (caller frees), or NULL.
 * The CDP response nests the value three deep: result -> result -> value. */
static char *page_eval(const char *expr, double timeout, char **err) {
    (void)timeout;   /* the client owns its own socket timeout */
    char *out = browser_cdp_evaluate(expr);
    if (!out && err && !*err) *err = strdup("the browser did not answer (no page attached, or the "
                                            "script threw)");
    return out;
}

static char *page_origin(char **err) {
    return page_eval("window.location.origin", 10, err);
}

/* ── fill ──────────────────────────────────────────────────────────────────────────────────────── */

/* Picks the item to fill: the handle when one is given, otherwise the single login item saved for
 * this origin. Two candidates is an error, not a guess -- filling the wrong account is worse than
 * asking. */
static json_t *pick_item(const char *base, const char *handle, const char *origin, char **err) {
    if (handle && *handle) {
        /* The store answers with a wrapper: {"value": <meta>} on a hit, {"error": "..."} on a miss.
         * Reading the wrapper as the item is how a refusal ends up naming an empty item
         * ("Vault item '' has no bound origin"). */
        json_t *res = vault_local_get_meta(base, handle);
        const json_t *e = res ? json_obj_get(res, "error") : NULL;
        const json_t *val = res ? json_obj_get(res, "value") : NULL;
        const json_t *ok = res ? json_obj_get(res, "ok") : NULL;
        if (!res || !val || (ok && ok->type == JSON_BOOL && !json_bool_value(ok))) {
            *err = strdup(e && e->type == JSON_STRING ? json_string_value(e)
                                                      : "no vault item with that handle");
            json_free(res);
            return NULL;
        }
        json_t *meta = json_copy((json_t *)val);
        json_free(res);
        /* An empty metadata object means the backend did not find the handle. Without this the caller
         * proceeds to the origin guard and reports "Vault item '' has no bound origin", which names
         * nothing and sends the reader looking in the wrong place. */
        const json_t *h = json_obj_get(meta, "handle");
        const json_t *i = json_obj_get(meta, "id");
        int named = (h && h->type == JSON_STRING && json_string_value(h)[0]) ||
                    (i && i->type == JSON_STRING && json_string_value(i)[0]);
        if (!named) {
            char msg[160];
            snprintf(msg, sizeof(msg), "no vault item with id '%s'", handle);
            *err = strdup(msg);
            json_free(meta);
            return NULL;
        }
        return meta;
    }

    json_t *res = vault_local_list(base);
    const json_t *value = res ? json_obj_get(res, "value") : NULL;
    if (!value || value->type != JSON_ARRAY) {
        const json_t *e = res ? json_obj_get(res, "error") : NULL;
        *err = strdup(e && e->type == JSON_STRING ? json_string_value(e)
                                                  : "the vault could not be read");
        json_free(res);
        return NULL;
    }
    const json_t *listing = value;

    json_t *found = NULL;
    for (size_t i = 0; i < json_len((json_t *)listing); i++) {
        const json_t *item = json_get((json_t *)listing, i);
        const json_t *kind = item ? json_obj_get(item, "kind") : NULL;
        const json_t *o = item ? json_obj_get(item, "origin") : NULL;
        if (!kind || kind->type != JSON_STRING) continue;
        if (strcmp(json_string_value(kind), "login") != 0) continue;
        if (!o || o->type != JSON_STRING || strcmp(json_string_value(o), origin) != 0) continue;
        if (found) {
            json_free(found);
            json_free(res);
            *err = strdup("more than one login is saved for this origin -- pass handle");
            return NULL;
        }
        found = json_copy((json_t *)item);
    }
    json_free(res);
    if (!found) *err = strdup("no login is saved for this origin");
    return found;
}

/* saved origins for the guard: the item's own origin plus any extra allowed origins */
static json_t *saved_origins_of(const json_t *meta) {
    json_t *out = json_array();
    const json_t *origin = json_obj_get(meta, "origin");
    if (origin && origin->type == JSON_STRING) json_append(out, json_copy((json_t *)origin));
    const json_t *extra = json_obj_get(meta, "allowed_origins");
    if (extra && extra->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len((json_t *)extra); i++)
            json_append(out, json_copy(json_get((json_t *)extra, i)));
    }
    return out;
}

/* Build the classified-control array from what the inspection returned. Upstream classifies the raw
 * controls and carries index/score/token forward; the inspection already stamped each control with the
 * nonce + its index, which is how the fill finds it again. */
static json_t *classify_all(const json_t *controls) {
    json_t *classified = json_array();
    if (!controls || controls->type != JSON_ARRAY) return classified;
    for (size_t i = 0; i < json_len((json_t *)controls); i++) {
        const json_t *c = json_get((json_t *)controls, i);
        json_t *r = w16_vaultlogin_58f247_classify_login(c);
        if (!r) continue;
        const json_t *ok = json_obj_get(r, "ok");
        if (ok && ok->type == JSON_BOOL && json_bool_value(ok)) {
            json_t *e = json_object();
            json_set(e, "index", json_copy(json_obj_get(c, "index")));
            json_set(e, "token", json_copy(json_obj_get(r, "token")));
            json_set(e, "score", json_copy(json_obj_get(r, "score")));
            json_set(e, "form_index", json_copy(json_obj_get(c, "formIndex")));
            json_append(classified, e);
        }
        json_free(r);
    }
    return classified;
}

json_t *browser_vault_fill(const char *base, const char *handle, const char *password_override,
                           const char *origin_override) {
    char *err = NULL;

    if (!cdp_get_url()) {
        json_t *o = json_object();
        json_set(o, "success", json_bool(0));
        json_set(o, "error", json_string("no browser is attached (set CHROME_WS_URL or browser.cdp_url)"));
        return o;
    }

    char *origin = origin_override && *origin_override ? strdup(origin_override) : page_origin(&err);
    if (!origin) {
        json_t *o = json_object();
        json_set(o, "success", json_bool(0));
        json_set(o, "error", json_string(err ? err : "the page origin could not be determined"));
        free(err);
        return o;
    }

    json_t *meta = pick_item(base, handle, origin, &err);
    if (!meta) {
        free(origin);
        json_t *o = json_object();
        json_set(o, "success", json_bool(0));
        json_set(o, "error", json_string(err ? err : "no item to fill"));
        free(err);
        return o;
    }

    /* The item is addressed by `handle` everywhere else in the tool surface (that is the key the
     * listing prints), so take it from there and fall back to `id` only if a backend names it that. */
    const char *item_id = json_string_value_safe(json_obj_get(meta, "handle"));
    if (!item_id || !*item_id) item_id = json_string_value_safe(json_obj_get(meta, "id"));
    const char *kind = json_string_value_safe(json_obj_get(meta, "kind"));

    /* The guard first: is this page one the item was saved for? A fill is a secret leaving the vault,
     * so the origin question is answered before a single byte is resolved. It returns the origin
     * string when the fill may proceed and a refusal object otherwise. */
    json_t *origins = saved_origins_of(meta);
    json_t *guard = vault_origin_guard(origins, origin, item_id, kind);
    json_free(origins);
    if (!guard || guard->type != JSON_STRING) {
        json_t *o = json_object();
        json_set(o, "success", json_bool(0));
        json_set(o, "refused", guard ? json_copy(guard) : json_null());
        json_free(guard);
        json_free(meta); free(origin);
        return o;
    }
    json_free(guard);

    /* Resolve the password. An explicit password wins so the caller can drive a one-off fill without
     * the secret ever being stored. */
    char *password = NULL;
    if (password_override && *password_override) {
        password = strdup(password_override);
    } else {
        /* The backend answers {"ok":bool,"value":...,"error":...} -- the same wrapper the listing uses. */
        json_t *pw = vault_local_resolve_password(base, item_id);
        const json_t *ok = pw ? json_obj_get(pw, "ok") : NULL;
        const json_t *val = pw ? json_obj_get(pw, "value") : NULL;
        if (pw && (!ok || (ok->type == JSON_BOOL && json_bool_value(ok))) &&
            val && val->type == JSON_STRING) {
            password = strdup(json_string_value(val));
        } else if (pw) {
            const json_t *e = json_obj_get(pw, "error");
            if (e && e->type == JSON_STRING) err = strdup(json_string_value(e));
        }
        json_free(pw);
    }
    if (!password) {
        json_free(meta); free(origin);
        json_t *o = json_object();
        json_set(o, "success", json_bool(0));
        json_set(o, "error", json_string(err ? err : "the item has no password"));
        free(err);
        return o;
    }

    /* Stamp the page, classify what came back, choose the password fill, then apply it. The nonce
     * keeps the stamp phase and the fill phase in step and makes the stamps impossible to guess. */
    char nonce[32];
    snprintf(nonce, sizeof(nonce), "%08x", (unsigned)(time(NULL) ^ (unsigned)(size_t)origin));
    char *insp_js = vl_build_inspection_js(nonce);
    char *controls_json = insp_js ? page_eval(insp_js, 15, &err) : NULL;
    free(insp_js);

    json_t *out = json_object();
    if (!controls_json) {
        json_set(out, "success", json_bool(0));
        json_set(out, "error", json_string(err ? err : "the page could not be inspected"));
        free(err);
    } else {
        json_t *controls = json_parse(controls_json, NULL);
        json_t *classified = classify_all(controls);
        json_free(controls);
        free(controls_json);

        json_t *pw_fills = w16_vaultlogin_58f247_select_password(classified, password);

        /* The identifier is metadata, not a secret, and upstream fills it too -- a password-only fill
         * leaves the user typing their own username after we already knew it. */
        const char *identifier = json_string_value_safe(json_obj_get(meta, "identifier"));
        json_t *id_fills = vault_identifier_fill(classified, pw_fills, identifier);

        json_t *fills = json_array();
        if (id_fills && id_fills->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(id_fills); i++)
                json_append(fills, json_copy(json_get(id_fills, i)));
        if (pw_fills && pw_fills->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(pw_fills); i++)
                json_append(fills, json_copy(json_get(pw_fills, i)));
        json_free(id_fills);

        char *fills_json = json_serialize(fills);
        json_free(classified);

        char *fill_js = fills_json ? vl_build_fill_js(fills_json, origin, nonce) : NULL;
        free(fills_json);

        char *result_json = fill_js ? page_eval(fill_js, 15, &err) : NULL;
        free(fill_js);

        json_t *result = result_json ? json_parse(result_json, NULL) : NULL;
        free(result_json);
        const json_t *filled = result ? json_obj_get(result, "filled") : NULL;
        double filled_n = (filled && filled->type == JSON_NUMBER) ? json_number_value(filled) : 0;
        if (result && result->type == JSON_OBJECT && filled_n > 0) {
            json_set(out, "success", json_bool(1));
            json_set(out, "item_id", json_string(item_id ? item_id : ""));
            json_set(out, "origin", json_string(origin));
            json_set(out, "filled", json_copy((json_t *)filled));
        } else if (result && result->type == JSON_OBJECT) {
            /* A fill that wrote nothing is not a success. Reporting success:true with filled:0 reads as
             * "the login is in the page" to every caller, including a model deciding what to do next. */
            json_set(out, "success", json_bool(0));
            json_set(out, "filled", json_number(0));
            json_set(out, "error", json_string(json_len(fills) == 0
                ? "no password field was found on the page"
                : "the page refused the fill (the control may be disabled or read-only)"));
        } else {
            json_set(out, "success", json_bool(0));
            json_set(out, "error", json_string(err ? err : (fills && json_len(fills) == 0
                                                               ? "no password field was found on the page"
                                                               : "the fill script did not report a result")));
        }
        json_free(result);
        json_free(pw_fills);
        json_free(fills);
    }
    free(err);
    json_free(meta);
    free(password);
    free(origin);
    return out;
}

/* ── tool entry point ──────────────────────────────────────────────────────────────────────────── */

char *browser_vault_tool(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *out = NULL;
    char *err = NULL;

    json_t *args = args_json && *args_json ? json_parse(args_json, NULL) : json_object();
    const char *action = json_string_value_safe(json_obj_get(args, "action"));
    if (!action || !*action) action = "list";

    char *base = vault_base();
    if (!base) {
        out = json_object();
        json_set(out, "success", json_bool(0));
        json_set(out, "error", json_string("could not resolve the hermes home directory"));
    } else if (strcmp(action, "list") == 0) {
        out = browser_vault_list_json(base, 0);
    } else if (strcmp(action, "fill") == 0) {
        out = browser_vault_fill(base,
                                 json_string_value_safe(json_obj_get(args, "handle")),
                                 json_string_value_safe(json_obj_get(args, "password")),
                                 json_string_value_safe(json_obj_get(args, "page_origin")));
    } else {
        out = json_object();
        json_set(out, "success", json_bool(0));
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "action must be one of list, fill (got %s); save_login, enter_code and unlock are not "
                 "implemented", action);
        json_set(out, "error", json_string(msg));
    }
    free(base);
    free(err);
    json_free(args);

    char *s = out ? json_serialize(out) : NULL;
    json_free(out);
    return s ? s : strdup("{\"success\":false,\"error\":\"out of memory\"}");
}
