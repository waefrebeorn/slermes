/*
 * browser_vault_fill_plan.c — the origin guard and the identifier pick, ported.
 *
 * Both functions exist to keep a stored secret from going to the wrong page or the wrong field, and
 * both are pure. The strings are upstream's verbatim: they are what the model reads to decide whether
 * to retry, navigate, or stop and ask the user.
 */
#include "browser_vault_fill_plan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *refusal(const char *error_type, const char *error)
{
    json_t *o = json_object();
    json_set(o, "success", json_bool(0));
    if (error_type) json_set(o, "error_type", json_string(error_type));
    json_set(o, "error", json_string(error));
    return o;
}

static const char *str_at(const json_t *arr, size_t i)
{
    return json_string_value_safe(json_get(arr, i));
}

json_t *vault_origin_guard(const json_t *saved_origins, const char *page_origin,
                           const char *item_id, const char *kind)
{
    size_t n = (saved_origins && saved_origins->type == JSON_ARRAY) ? json_len(saved_origins) : 0;

    if (n == 0) {
        /* Upstream: f"Vault item {meta.id!r} has no bound origin; {meta.kind} items are filled only on
         * the site they were saved for." -- the id is %r, so it carries single quotes. */
        size_t need = strlen(item_id ? item_id : "") + strlen(kind ? kind : "") + 140;
        char *msg = malloc(need);
        if (!msg) return refusal("no_origin", "Vault item has no bound origin.");
        snprintf(msg, need,
                 "Vault item '%s' has no bound origin; %s items are filled only on the site they were "
                 "saved for.", item_id ? item_id : "", kind ? kind : "");
        json_t *o = refusal("no_origin", msg);
        free(msg);
        return o;
    }

    if (!page_origin || !page_origin[0]) {
        return refusal(NULL,
                       "Could not determine the current page origin. Navigate to the login page first.");
    }

    for (size_t i = 0; i < n; i++) {
        const char *allowed = str_at(saved_origins, i);
        if (allowed && strcmp(allowed, page_origin) == 0)
            return json_string(page_origin);          /* exact match; no wildcard inference */
    }

    /* The refusal names both sides, because "origin_mismatch" without them gives the model nothing to
     * act on. The list is joined exactly the way upstream joins it. */
    size_t need = 128;
    for (size_t i = 0; i < n; i++) {
        const char *a = str_at(saved_origins, i);
        need += (a ? strlen(a) : 0) + 2;
    }
    char *joined = malloc(need);
    if (!joined) return refusal("origin_mismatch", "Refused: origin mismatch.");
    joined[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        const char *a = str_at(saved_origins, i);
        if (i) strcat(joined, ", ");
        strcat(joined, a ? a : "");
    }
    size_t mlen = strlen(page_origin) + strlen(joined) + 320;
    char *msg = malloc(mlen);
    if (!msg) { free(joined); return refusal("origin_mismatch", "Refused: origin mismatch."); }
    snprintf(msg, mlen,
             "Refused: current page origin (%s) does not match the vault item's bound origin(s) (%s). "
             "Vault fills only run on the exact origin(s) the credential was saved for.",
             page_origin, joined);
    json_t *o = refusal("origin_mismatch", msg);
    free(msg);
    free(joined);
    return o;
}

/* min(fields, key=lambda c: (c.control.form_index != form, -c.score, c.control.index)) */
json_t *vault_identifier_fill(const json_t *classified, const json_t *password_fill,
                              const char *identifier)
{
    json_t *out = json_array();
    if (!identifier || !identifier[0]) return out;
    if (!classified || classified->type != JSON_ARRAY) return out;

    /* The anchor is the password fill's index; the form it lives in wins ties. */
    int have_anchor = 0;
    double anchor = 0;
    double form = 0;
    if (password_fill && password_fill->type == JSON_ARRAY && json_len(password_fill) > 0) {
        const json_t *first = json_get(password_fill, 0);
        const json_t *idx = json_obj_get(first, "index");
        if (idx && idx->type == JSON_NUMBER) { anchor = json_number_value(idx); have_anchor = 1; }
    }
    if (have_anchor) {
        for (size_t i = 0; i < json_len(classified); i++) {
            const json_t *c = json_get(classified, i);
            const json_t *ci = json_obj_get(c, "index");
            if (ci && ci->type == JSON_NUMBER && json_number_value(ci) == anchor) {
                const json_t *fi = json_obj_get(c, "form_index");
                if (fi && fi->type == JSON_NUMBER) form = json_number_value(fi);
                break;
            }
        }
    }

    const json_t *best = NULL;
    double best_form = 0, best_score = 0, best_index = 0;
    for (size_t i = 0; i < json_len(classified); i++) {
        const json_t *c = json_get(classified, i);
        const char *token = json_get_str(c, "token", "");
        if (!token) continue;
        if (strcmp(token, "username") && strcmp(token, "email") && strcmp(token, "tel")) continue;

        const json_t *fi = json_obj_get(c, "form_index");
        double cform = (fi && fi->type == JSON_NUMBER) ? json_number_value(fi) : 0;
        const json_t *sc = json_obj_get(c, "score");
        double cscore = (sc && sc->type == JSON_NUMBER) ? json_number_value(sc) : 0;
        const json_t *ci = json_obj_get(c, "index");
        double cindex = (ci && ci->type == JSON_NUMBER) ? json_number_value(ci) : 0;

        if (!best) { best = c; best_form = cform; best_score = cscore; best_index = cindex; continue; }

        /* Tuple comparison in upstream's order: wrong form first, then higher score, then lower index. */
        int bform_wrong = have_anchor && best_form != form;
        int cform_wrong = have_anchor && cform != form;
        int better = 0;
        if (cform_wrong != bform_wrong) better = bform_wrong;
        else if (cscore != best_score) better = cscore > best_score;
        else better = cindex < best_index;
        if (better) { best = c; best_form = cform; best_score = cscore; best_index = cindex; }
    }

    if (best) {
        json_t *fill = json_object();
        json_set(fill, "index", json_copy(json_obj_get(best, "index")));
        json_set(fill, "token", json_string(json_get_str(best, "token", "")));
        json_set(fill, "value", json_string(identifier));
        json_append(out, fill);
    }
    return out;
}
