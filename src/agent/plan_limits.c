/*
 * plan_limits.c — C port of upstream Hermes agent/plan_limits.py.
 *
 * PoP: planlimits_model_label @ agent/plan_limits.py:model_label
 * PoP: planlimits_credits_action @ agent/plan_limits.py:credits_action
 * PoP: planlimits_portal_wording @ agent/plan_limits.py:portal_wording
 * PoP: planlimits_next_steps @ agent/plan_limits.py:limit_next_steps_from_payload
 */
#include "noc_planlimits.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str.strip() for the ASCII whitespace used by the fixture copy. */
static char *trim_dup(const char *s) {
    if (!s) return strdup("");
    const char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - p);
    char *out = (char *)malloc(n + 1);
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

char *planlimits_model_label(const char *model, const json_t *pricing) {
    const char *m = model ? model : "";
    if (json_is_object(pricing)) {
        json_t *entry = json_obj_get(pricing, m);
        if (json_is_object(entry)) {
            json_t *name = json_obj_get(entry, "name");
            if (name && name->type == JSON_STRING) {
                char *t = trim_dup(name->str_val);
                if (*t) return t;
                free(t);
            }
        }
    }
    return strdup(m);
}

const char *planlimits_credits_action(int keep_going_false, const char *purchased_micros_decimal) {
    long long micros = purchased_micros_decimal ? strtoll(purchased_micros_decimal, NULL, 10) : 0;
    int has_balance = micros > 0;
    if (keep_going_false)
        return has_balance ? "use_credits" : "add_credits_and_keep_going";
    return "add_credits";
}

static char *replace_all(const char *s, const char *a, const char *b) {
    if (!s) s = "";
    size_t la = strlen(a), lb = strlen(b);
    if (!la) return strdup(s);
    size_t cap = strlen(s) * 2 + 64, len = 0;
    char *out = (char *)malloc(cap);
    const char *p = s;
    while (1) {
        const char *q = strstr(p, a);
        size_t seg = q ? (size_t)(q - p) : strlen(p);
        while (len + seg + (q ? lb : 0) + 1 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
        memcpy(out + len, p, seg); len += seg;
        if (!q) { out[len] = '\0'; break; }
        memcpy(out + len, b, lb); len += lb;
        p = q + la;
    }
    return out;
}

char *planlimits_portal_wording(const char *text) {
    char *s = strdup(text ? text : "");
    char *t1 = replace_all(s, "turn on Keep going in /topup", "turn on Keep going in the Portal");
    free(s);
    char *t2 = replace_all(t1, "(turn on in /topup)", "(turn on in the Portal)");
    free(t1);
    char *t3 = replace_all(t2, "/topup to add credits", "add credits in the Portal");
    free(t2);
    char *t4 = replace_all(t3, "add credits in /topup", "add credits in the Portal");
    free(t3);
    return t4;
}

/* --- next_steps helpers --- */
static int truthy(const json_t *j) {
    if (!j) return 0;
    switch (j->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return j->bool_val ? 1 : 0;
        case JSON_NUMBER: return j->num_val != 0.0;
        case JSON_STRING: return j->str_val && j->str_val[0];
        case JSON_ARRAY: case JSON_OBJECT: return j->c.count > 0;
    }
    return 0;
}

static char *pystr(const json_t *j) {
    if (!j) return strdup("");
    switch (j->type) {
        case JSON_STRING: return strdup(j->str_val ? j->str_val : "");
        case JSON_BOOL: return strdup(j->bool_val ? "True" : "False");
        case JSON_NULL: return strdup("None");
        case JSON_NUMBER: {
            char b[64];
            double v = j->num_val;
            if (v == (double)(long long)v) snprintf(b, sizeof b, "%lld", (long long)v);
            else snprintf(b, sizeof b, "%g", v);
            return strdup(b);
        }
        default: return strdup("");
    }
}

static void app(char *buf, size_t cap, size_t *len, const char *s) {
    size_t n = strlen(s);
    if (*len + n + 1 > cap) return; /* fixtures stay well under cap */
    memcpy(buf + *len, s, n);
    *len += n;
    buf[*len] = '\0';
}

char *planlimits_next_steps(const json_t *limit, const char *model_command,
                            const char *topup_command, int topup_is_null, int portal, int markup) {
    const char *mc = (model_command && *model_command) ? model_command : "/model";
    const char *tc = topup_is_null ? NULL : (topup_command ? topup_command : "/topup");

    char *mcw = NULL, *tcw = NULL;
    if (markup) {
        size_t mlen = strlen(mc) + 11 + 1;
        mcw = (char *)malloc(mlen);
        snprintf(mcw, mlen, "[bold]%s[/]", mc);
        mc = mcw;
        if (tc) {
            size_t tlen = strlen(tc) + 11 + 1;
            tcw = (char *)malloc(tlen);
            snprintf(tcw, tlen, "[bold]%s[/]", tc);
            tc = tcw;
        }
    }

    int has_sug = 0;
    char *label = NULL, *note = NULL;
    const char *action = NULL;
    if (json_is_object(limit)) {
        json_t *sugs = json_obj_get(limit, "suggestions");
        if (sugs && sugs->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(sugs); i++) {
                json_t *s = json_get(sugs, i);
                if (!json_is_object(s)) continue;
                json_t *mj = json_obj_get(s, "model");
                if (!truthy(mj)) continue;
                json_t *lj = json_obj_get(s, "label");
                if (!truthy(lj)) lj = mj;
                label = pystr(lj);
                json_t *nj = json_obj_get(s, "note");
                note = truthy(nj) ? pystr(nj) : strdup("included");
                has_sug = 1;
                break;
            }
        }
        json_t *credits = json_obj_get(limit, "credits");
        if (json_is_object(credits)) {
            json_t *aj = json_obj_get(credits, "action");
            if (truthy(aj)) action = pystr(aj);
        }
    }

    char *where = NULL;
    if (action) {
        where = (portal || !tc || !*tc) ? strdup("the Portal") : strdup(tc);
    }

    char *buf = (char *)malloc(8192);
    size_t len = 0, cap = 8192;
    buf[0] = '\0';
    if (has_sug) {
        app(buf, cap, &len, "Switch to ");
        app(buf, cap, &len, label ? label : "");
        app(buf, cap, &len, " (");
        app(buf, cap, &len, note ? note : "included");
        app(buf, cap, &len, ") with ");
        app(buf, cap, &len, mc);
    } else {
        app(buf, cap, &len, "Pick another model with ");
        app(buf, cap, &len, mc);
    }
    if (action) {
        app(buf, cap, &len, ", or ");
        if (!strcmp(action, "use_credits")) {
            app(buf, cap, &len, "turn on Keep going in ");
            app(buf, cap, &len, where);
            app(buf, cap, &len, " to use credits");
        } else if (!strcmp(action, "add_credits_and_keep_going")) {
            app(buf, cap, &len, "add credits and turn on Keep going in ");
            app(buf, cap, &len, where);
        } else {
            app(buf, cap, &len, "add credits ");
            app(buf, cap, &len, !strcmp(where, "the Portal") ? "in " : "with ");
            app(buf, cap, &len, where);
        }
    }
    app(buf, cap, &len, ".");

    free(label); free(note); free((void *)action); free(where);
    free(mcw); free(tcw);
    return buf;
}
