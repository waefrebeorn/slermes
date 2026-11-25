/*
 * browser_vault_list.c — `browser_vault action=list`, ported from tools/browser_vault_tool.py.
 *
 * The shape is upstream's, key for key, because the entry is what the model reads to decide which
 * handle to fill: "available" gates whether a fill is even attempted (a payment item without an origin
 * cannot be routed to a site), and "two_factor" tells it whether a code can be minted without asking
 * the user.
 *
 * Two fields are deliberately absent here and present in upstream only under conditions C cannot
 * reach yet: allowed_origins (manager backends populate it, VaultStore._meta never does) and the
 * browser-profile backend. Reporting either would claim a fill scope the Python side never claimed.
 */
#include "browser_vault_list.h"
#include "vault_local_store.h"

#include <stdlib.h>
#include <string.h>

/* One entry per item, in upstream's key order. */
static json_t *entry_for(const json_t *meta, const char *backend_name)
{
    json_t *e = json_object();
    const char *id = json_get_str(meta, "id", "");
    const char *label = json_get_str(meta, "label", "");
    const char *kind = json_get_str(meta, "kind", "");
    const json_t *origin = json_obj_get(meta, "origin");
    const json_t *has_otp = json_obj_get(meta, "has_otp");
    const json_t *identifier = json_obj_get(meta, "identifier");

    json_set(e, "handle", json_string(id));
    json_set(e, "backend", json_string(backend_name));
    json_set(e, "label", json_string(label));
    json_set(e, "kind", json_string(kind));
    json_set(e, "origin", json_copy(origin));

    /* available: kind == "login" or bool(origin) -- a login can be filled by handle alone, anything
     * else needs an origin to know where it belongs. */
    int origin_truthy = origin && origin->type != JSON_NULL &&
                        !(json_string_value_safe(origin) && json_string_value_safe(origin)[0] == '\0');
    int available = (strcmp(kind, "login") == 0) || origin_truthy;
    json_set(e, "available", json_bool(available ? 1 : 0));

    /* two_factor: upstream emits it when the item has an OTP seed OR the backend needs an unlock (an
     * external manager may hold a seed it has not told us about). The local backend never needs an
     * unlock, so only the first condition can fire here. */
    if (has_otp && json_bool_value(has_otp))
        json_set(e, "two_factor", json_string("automatic"));

    const char *ident = json_string_value_safe(identifier);
    if (ident && ident[0]) {
        json_set(e, "identifier", json_string(ident));
        const json_t *itype = json_obj_get(meta, "identifier_type");
        json_set(e, "identifier_type",
                 json_string(json_string_value_safe(itype) ? json_string_value_safe(itype) : ""));
    }
    return e;
}

json_t *browser_vault_list_json(const char *base, int can_prompt)
{
    json_t *out = json_object();
    json_t *items = json_array();

    json_t *local = vault_local_list(base);
    const json_t *value = json_obj_get(local, "value");
    const json_t *error = json_obj_get(local, "error");
    int list_ok = value && value->type == JSON_ARRAY;

    if (list_ok) {
        for (size_t i = 0; i < json_len(value); i++) {
            const json_t *meta = json_get(value, i);
            if (!meta || meta->type != JSON_OBJECT) continue;
            json_append(items, entry_for(meta, "local"));
        }
    }

    json_set(out, "success", json_bool(1));
    json_set(out, "items", items);

    /* Upstream's hint is aimed at the model: it exists so an empty vault does not get "solved" by
     * typing a password the model saw on the page. */
    if (json_len(items) == 0) {
        json_set(out, "hint",
                 json_string("No saved logins. On a login page, call browser_vault action=save_login to "
                             "ask the user to save one. Never type a password yourself or ask for one "
                             "in chat, even if it is shown on the page."));
    }

    if (!list_ok) {
        /* The vault could not be opened: report it the way upstream reports a backend that threw,
         * instead of an empty list that would read as "you have no logins". */
        json_t *errs = json_array();
        json_t *one = json_object();
        json_set(one, "backend", json_string("local"));
        json_set(one, "error",
                 json_string(error && json_string_value_safe(error) ? json_string_value_safe(error)
                                                                    : "the vault could not be opened"));
        json_append(errs, one);
        json_set(out, "errors", errs);
    }

    (void)can_prompt;   /* only the "locked" branch uses it, and no backend here is lockable yet */
    json_free(local);
    return out;
}
