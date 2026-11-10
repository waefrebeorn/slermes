/*
 * port_vault_login_classifier.c — Port of Python: agent/vault_login_classifier.py
 *
 * Login / checkout autofill control classifier. Scoring (unchanged from the
 * Python):
 *   autocomplete token exact match ...... 100
 *   type=password (not new/confirm/...) ..  90
 *   type=email / type=tel ...............  85
 *   label/name heuristic ................  70-75
 * Hard exclusions: autocomplete new-password / one-time-code, and
 * name/label matching "(new|confirm|create|repeat)\s*password".
 */

#include "port_vault_login_classifier.h"
#include "vault_login_tokens.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VL_MAX_TOKENS 64

/* ── patterns ────────────────────────────────────────────────────── */

static int is_excluded_password(char **t, int n)
{
    static const char *const seeds[] = { "new", "confirm", "create", "repeat" };
    for (int i = 0; i + 1 < n; i++)
        if (any_tok(t + i, 1, seeds, 4) && strcmp(t[i + 1], "password") == 0)
            return 1;
    static const char *const fused[] = { "newpassword", "confirmpassword",
                                         "createpassword", "repeatpassword" };
    return any_tok(t, n, fused, 4);
}

static int is_email_text(char **t, int n)
{
    return has_tok(t, n, "email") || has_pair(t, n, "e", "mail") ||
           has_pair(t, n, "email", "address");
}

static int is_tel_text(char **t, int n)
{
    static const char *const set[] = { "phone", "telephone", "mobile" };
    return any_tok(t, n, set, 3);
}

static int is_username_text(char **t, int n)
{
    static const char *const set[] = { "username", "login", "account", "member",
                                       "membership", "mileageplus" };
    return any_tok(t, n, set, 6) || has_pair(t, n, "user", "name");
}

static int is_otp_text(char **t, int n)
{
    static const char *const prefix[] = {
        "verification", "security", "auth", "authentication", "authenticator",
        "2fa", "mfa", "totp", "otp", "passcode", "sms",
    };
    static const char *const code_words[] = { "code", "pin", "token" };
    for (int i = 0; i < n; i++) {
        int is_prefix = any_tok(t + i, 1, prefix, 11) ||
                        pair_at(t, n, i, "one", "time") ||
                        pair_at(t, n, i, "two", "factor");
        if (!is_prefix) continue;
        for (int j = i + 1; j < n; j++)
            if (any_tok(t + j, 1, code_words, 3)) return 1;
    }
    static const char *const direct[] = { "otp", "totp", "2fa", "mfa", "passcode" };
    return any_tok(t, n, direct, 5) || has_pair(t, n, "verification", "code");
}

static const char *checkout_token(char **t, int n)
{
    if (has_pair(t, n, "card", "number") || has_tok(t, n, "cardnumber") ||
        has_tok(t, n, "ccnumber") || has_pair(t, n, "cc", "num") ||
        has_tok(t, n, "pan"))
        return "cc-number";
    if (has_triple(t, n, "name", "on", "card") || has_tok(t, n, "cardholder") ||
        has_pair(t, n, "cc", "name") || has_tok(t, n, "ccname"))
        return "cc-name";
    if (has_tok(t, n, "cvc") || has_tok(t, n, "cvv") || has_tok(t, n, "csc") ||
        has_pair(t, n, "security", "code") || has_pair(t, n, "card", "code"))
        return "cc-csc";
    if (has_pair(t, n, "exp", "month") || has_pair(t, n, "expiry", "month") ||
        has_pair(t, n, "expiration", "month") || has_pair(t, n, "exp", "mm") ||
        has_tok(t, n, "ccmonth"))
        return "cc-exp-month";
    if (has_pair(t, n, "exp", "year") || has_pair(t, n, "expiry", "year") ||
        has_pair(t, n, "expiration", "year") || has_pair(t, n, "exp", "yy") ||
        has_pair(t, n, "exp", "yyyy") || has_tok(t, n, "ccyear"))
        return "cc-exp-year";
    if (has_tok(t, n, "exp") || has_tok(t, n, "expiry") ||
        has_tok(t, n, "expiration") || has_pair(t, n, "mm", "yy") ||
        has_pair(t, n, "valid", "thru"))
        return "cc-exp";
    if (has_tok(t, n, "address2") || has_pair(t, n, "address", "2") ||
        has_triple(t, n, "address", "line", "2") || has_tok(t, n, "apt") ||
        has_tok(t, n, "suite") || has_tok(t, n, "unit"))
        return "address-line2";
    if (has_tok(t, n, "address") || has_pair(t, n, "address", "1") ||
        has_triple(t, n, "address", "line", "1") || has_tok(t, n, "street"))
        return "address-line1";
    if (has_tok(t, n, "city") || has_tok(t, n, "town") || has_tok(t, n, "locality"))
        return "address-level2";
    if (has_tok(t, n, "state") || has_tok(t, n, "province") ||
        has_tok(t, n, "region") || has_tok(t, n, "county"))
        return "address-level1";
    if (has_tok(t, n, "zip") || has_tok(t, n, "postal") || has_tok(t, n, "postcode"))
        return "postal-code";
    if (has_tok(t, n, "country")) return "country-name";
    return NULL;
}

/* ── control / classified lifecycle ─────────────────────────────── */

void vl_control_free(vl_control_t *control)
{
    if (!control) return;
    free(control->autocomplete);
    free(control->label);
    free(control->name);
    free(control->type);
    memset(control, 0, sizeof(*control));
}

void vl_classified_free(vl_classified_t *classified)
{
    if (!classified) return;
    vl_control_free(&classified->control);
    free(classified->token);
    memset(classified, 0, sizeof(*classified));
}

void vl_classified_list_free(vl_classified_t *list, int n)
{
    if (!list) return;
    for (int i = 0; i < n; i++) vl_classified_free(&list[i]);
    free(list);
}

static char *node_str(const json_t *node)
{
    if (!node) return dup_str("");
    char buf[64];
    switch (node->type) {
    case JSON_STRING: return dup_str(node->str_val ? node->str_val : "");
    case JSON_NUMBER: snprintf(buf, sizeof(buf), "%.17g", node->num_val); return dup_str(buf);
    case JSON_BOOL:   return dup_str(node->bool_val ? "True" : "False");
    case JSON_NULL:   return dup_str("");
    default:          return json_serialize(node);
    }
}

/* PoP: vl_control_from_json @ agent/vault_login_classifier.py:from_dict */
int vl_control_from_json(const char *json, vl_control_t *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    json_t *doc = json ? json_parse(json, NULL) : NULL;
    if (!doc || doc->type != JSON_OBJECT) {
        if (doc) json_free(doc);
        return -1;
    }

    out->autocomplete = node_str(json_obj_get(doc, "autocomplete"));
    out->label = node_str(json_obj_get(doc, "label"));
    out->name = node_str(json_obj_get(doc, "name"));
    out->type = node_str(json_obj_get(doc, "type"));

    json_t *fi = json_obj_get(doc, "formIndex");
    if (!fi) fi = json_obj_get(doc, "form_index");
    if (fi && fi->type != JSON_NULL) {
        out->has_form_index = 1;
        out->form_index = (fi->type == JSON_NUMBER) ? (int)fi->num_val : 0;
    }

    json_t *ml = json_obj_get(doc, "maxLength");
    if (!ml) ml = json_obj_get(doc, "max_length");
    if (ml && ml->type != JSON_NULL) {
        out->has_max_length = 1;
        out->max_length = (ml->type == JSON_NUMBER) ? (int)ml->num_val : 0;
    }

    json_t *idx = json_obj_get(doc, "index");
    out->index = (idx && idx->type == JSON_NUMBER) ? (int)idx->num_val : 0;
    json_free(doc);
    return 0;
}

static void classified_set(vl_classified_t *out, const vl_control_t *control,
                           int score, const char *token)
{
    memset(out, 0, sizeof(*out));
    out->control.autocomplete = dup_str(control->autocomplete);
    out->control.label = dup_str(control->label);
    out->control.name = dup_str(control->name);
    out->control.type = dup_str(control->type);
    out->control.index = control->index;
    out->control.has_form_index = control->has_form_index;
    out->control.form_index = control->form_index;
    out->control.has_max_length = control->has_max_length;
    out->control.max_length = control->max_length;
    out->score = score;
    out->token = dup_str(token);
}

/* autocomplete attribute tokens: Python lowercases and splits on whitespace
 * only. Punctuation is significant here ("current-password", "one-time-code"),
 * so normalize_text must NOT be used — it would turn the hyphen into a space. */
static int autocomplete_tokens(const char *autocomplete, char **tokens, int max)
{
    char *lower = dup_str(autocomplete ? autocomplete : "");
    if (!lower) return 0;
    for (char *q = lower; *q; q++) *q = (char)tolower((unsigned char)*q);

    int n = 0;
    const char *p = lower;
    while (*p && n < max) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t len = (size_t)(p - start);
        char *tok = (char *)malloc(len + 1);
        if (!tok) break;
        memcpy(tok, start, len);
        tok[len] = ' ';
        tokens[n++] = tok;
    }
    free(lower);
    return n;
}

/* PoP: vl_classify_login_control @ agent/vault_login_classifier.py:classify_login_control */
int vl_classify_login_control(const vl_control_t *control, vl_classified_t *out)
{
    if (!control || !out) return 1;
    static const char *const LOGIN_TOKENS[] = { "username", "email", "tel",
                                                "current-password" };
    static const char *const EXCLUDED[] = { "new-password", "one-time-code" };

    char *tokens[8];
    int nt = autocomplete_tokens(control->autocomplete, tokens, 8);
    int excluded = 0, matched = -1;
    for (int i = 0; i < nt; i++) {
        if (strcmp(tokens[i], "new-password") == 0 ||
            strcmp(tokens[i], "one-time-code") == 0)
            excluded = 1;
        for (int k = 0; k < 4; k++)
            if (matched < 0 && strcmp(tokens[i], LOGIN_TOKENS[k]) == 0)
                matched = k;
    }
    for (int i = 0; i < nt; i++) free(tokens[i]);
    if (excluded) return 1;
    if (matched >= 0) {
        classified_set(out, control, 100, LOGIN_TOKENS[matched]);
        return 0;
    }

    size_t cap = strlen(control->name ? control->name : "") +
                 strlen(control->label ? control->label : "") + 4;
    char *joined = (char *)malloc(cap);
    if (!joined) return 1;
    snprintf(joined, cap, "%s %s", control->name ? control->name : "",
             control->label ? control->label : "");
    char *norm = vl_normalize_text(joined);
    free(joined);
    if (!norm) return 1;

    char *t[VL_MAX_TOKENS];
    int n = tokenize(norm, t, VL_MAX_TOKENS);
    int result = 1;
    if (is_excluded_password(t, n)) {
        result = 1;
    } else {
        const char *type = control->type ? control->type : "";
        if (strcmp(type, "password") == 0) {
            classified_set(out, control, 90, "current-password"); result = 0;
        } else if (strcmp(type, "email") == 0) {
            classified_set(out, control, 85, "email"); result = 0;
        } else if (strcmp(type, "tel") == 0) {
            classified_set(out, control, 85, "tel"); result = 0;
        } else if (is_email_text(t, n)) {
            classified_set(out, control, 75, "email"); result = 0;
        } else if (is_tel_text(t, n)) {
            classified_set(out, control, 75, "tel"); result = 0;
        } else if (is_username_text(t, n)) {
            classified_set(out, control, 70, "username"); result = 0;
        }
    }
    vl_free_tokens(t, n);
    free(norm);
    return result;
}

/* PoP: vl_classify_checkout_control @ agent/vault_login_classifier.py:classify_checkout_control */
int vl_classify_checkout_control(const vl_control_t *control,
                                 vl_classified_t *out)
{
    if (!control || !out) return 1;
    static const char *const TOKENS[] = {
        "cc-number", "cc-name", "cc-csc", "cc-exp-month", "cc-exp-year",
        "cc-exp", "address-line1", "address-line2", "address-level2",
        "address-level1", "postal-code", "country-name", "country",
    };

    char *at[8];
    int nt = autocomplete_tokens(control->autocomplete, at, 8);
    for (int i = 0; i < nt; i++) {
        for (size_t k = 0; k < sizeof(TOKENS) / sizeof(TOKENS[0]); k++) {
            if (strcmp(at[i], TOKENS[k]) == 0) {
                const char *token = strcmp(TOKENS[k], "country") == 0
                                        ? "country-name" : TOKENS[k];
                for (int j = 0; j < nt; j++) free(at[j]);
                classified_set(out, control, 100, token);
                return 0;
            }
        }
    }
    for (int i = 0; i < nt; i++) free(at[i]);

    const char *type = control->type ? control->type : "";
    if (strcmp(type, "password") == 0 || strcmp(type, "email") == 0) return 1;

    size_t cap = strlen(control->name ? control->name : "") +
                 strlen(control->label ? control->label : "") + 4;
    char *joined = (char *)malloc(cap);
    if (!joined) return 1;
    snprintf(joined, cap, "%s %s", control->name ? control->name : "",
             control->label ? control->label : "");
    char *norm = vl_normalize_text(joined);
    free(joined);
    if (!norm) return 1;

    char *t[VL_MAX_TOKENS];
    int n = tokenize(norm, t, VL_MAX_TOKENS);
    const char *token = checkout_token(t, n);
    vl_free_tokens(t, n);
    free(norm);
    if (!token) return 1;
    classified_set(out, control, 70, token);
    return 0;
}

/* PoP: vl_classify_otp_controls @ agent/vault_login_classifier.py:classify_otp_controls */
int vl_classify_otp_controls(const vl_control_t *controls, int n,
                             vl_classified_t **out)
{
    if (out) *out = NULL;
    if (!controls || n <= 0) return 0;
    vl_classified_t *list = (vl_classified_t *)calloc((size_t)n, sizeof(*list));
    if (!list) return 0;
    int count = 0;

    for (int i = 0; i < n; i++) {
        const vl_control_t *c = &controls[i];
        char *at[8];
        int nt = autocomplete_tokens(c->autocomplete, at, 8);
        int authoritative = 0;
        for (int k = 0; k < nt; k++)
            if (strcmp(at[k], "one-time-code") == 0) authoritative = 1;
        for (int k = 0; k < nt; k++) free(at[k]);
        if (authoritative) {
            classified_set(&list[count++], c, 100, "one-time-code");
            continue;
        }
        const char *type = c->type ? c->type : "";
        if (!(strcmp(type, "text") == 0 || strcmp(type, "tel") == 0 ||
              strcmp(type, "number") == 0 || strcmp(type, "password") == 0 ||
              type[0] == '\0'))
            continue;

        size_t cap = strlen(c->name ? c->name : "") +
                     strlen(c->label ? c->label : "") + 4;
        char *joined = (char *)malloc(cap);
        if (!joined) continue;
        snprintf(joined, cap, "%s %s", c->name ? c->name : "",
                 c->label ? c->label : "");
        char *norm = vl_normalize_text(joined);
        free(joined);
        if (!norm) continue;
        char *t[VL_MAX_TOKENS];
        int tn = tokenize(norm, t, VL_MAX_TOKENS);
        int otp = is_otp_text(t, tn);
        vl_free_tokens(t, tn);
        free(norm);
        if (otp) classified_set(&list[count++], c, 70, "one-time-code");
    }
    if (out) *out = list;
    else vl_classified_list_free(list, count);
    return count;
}

/* ── selection ───────────────────────────────────────────────────── */

/* PoP: vl_select_password_fill @ agent/vault_login_classifier.py:select_password_fill */
json_t *vl_select_password_fill(const vl_classified_t *classified, int n,
                                const char *password)
{
    json_t *fills = json_array();
    if (!fills) return NULL;
    if (!classified || !password || !*password) return fills;

    int best = -1;
    for (int i = 0; i < n; i++) {
        if (!classified[i].token || strcmp(classified[i].token, "current-password") != 0)
            continue;
        if (best < 0 || classified[i].score > classified[best].score ||
            (classified[i].score == classified[best].score &&
             classified[i].control.index < classified[best].control.index))
            best = i;
    }
    if (best < 0) return fills;

    json_t *fill = json_object();
    json_set(fill, "index", json_number((double)classified[best].control.index));
    json_set(fill, "token", json_string("current-password"));
    json_set(fill, "value", json_string(password));
    json_append(fills, fill);
    return fills;
}

/* PoP: vl_select_checkout_fills @ agent/vault_login_classifier.py:select_checkout_fills */
json_t *vl_select_checkout_fills(const vl_classified_t *classified, int n,
                                 const char *secret_json,
                                 const char *field_tokens_json)
{
    json_t *fills = json_array();
    if (!fills) return NULL;
    json_t *secret = secret_json ? json_parse(secret_json, NULL) : NULL;
    json_t *field_tokens = field_tokens_json ? json_parse(field_tokens_json, NULL) : NULL;
    if (!secret || secret->type != JSON_OBJECT || !field_tokens ||
        field_tokens->type != JSON_OBJECT) {
        if (secret) json_free(secret);
        if (field_tokens) json_free(field_tokens);
        return fills;
    }

    /* values: token -> value, in field_tokens insertion order */
    struct { char *token; char *value; } values[32];
    int nv = 0;
    for (size_t i = 0; i < field_tokens->c.count && nv < 32; i++) {
        const char *field = field_tokens->c.keys ? field_tokens->c.keys[i] : NULL;
        json_t *toknode = field_tokens->c.items[i];
        if (!field || !toknode || toknode->type != JSON_STRING) continue;
        json_t *val = json_obj_get(secret, field);
        if (!val || val->type != JSON_STRING || !val->str_val[0]) continue;
        values[nv].token = dup_str(toknode->str_val);
        values[nv].value = dup_str(val->str_val);
        nv++;
    }

    /* cc-exp derived from month + year */
    char *exp_month = NULL, *exp_year = NULL;
    for (int i = 0; i < nv; i++) {
        if (strcmp(values[i].token, "cc-exp-month") == 0) exp_month = values[i].value;
        if (strcmp(values[i].token, "cc-exp-year") == 0) exp_year = values[i].value;
    }
    int have_exp = 0;
    char exp_value[32] = "";
    if (exp_month && exp_year && nv < 32) {
        size_t len = strlen(exp_month);
        const char *m = exp_month;
        char zero_padded[4] = "0";
        if (len == 1) { zero_padded[1] = exp_month[0]; zero_padded[2] = '\0'; m = zero_padded; }
        size_t ylen = strlen(exp_year);
        const char *yy = ylen >= 2 ? exp_year + ylen - 2 : exp_year;
        snprintf(exp_value, sizeof(exp_value), "%s/%s", m, yy);
        values[nv].token = dup_str("cc-exp");
        values[nv].value = dup_str(exp_value);
        nv++;
        have_exp = 1;
    }

    /* one fill per token: best score, then lowest index */
    json_t *built[32];
    int nb = 0;
    for (int v = 0; v < nv && nb < 32; v++) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (!classified[i].token || strcmp(classified[i].token, values[v].token) != 0)
                continue;
            if (best < 0 || classified[i].score > classified[best].score ||
                (classified[i].score == classified[best].score &&
                 classified[i].control.index < classified[best].control.index))
                best = i;
        }
        if (best < 0) continue;
        json_t *fill = json_object();
        json_set(fill, "index", json_number((double)classified[best].control.index));
        json_set(fill, "token", json_string(values[v].token));
        json_set(fill, "value", json_string(values[v].value));
        built[nb++] = fill;
    }

    for (int i = 0; i < nb; i++) {
        int drop = 0;
        /* Live: only when a combined cc-exp fill was actually created does it
         * suppress the separate month/year fills — never just because the secret
         * carried month+year but no control classified as token cc-exp. */
        const char *tok_i = json_get_str(built[i], "token", "");
        if (strcmp(tok_i, "cc-exp-month") == 0 || strcmp(tok_i, "cc-exp-year") == 0) {
            for (int j = 0; j < nb; j++) {
                const char *tj = json_get_str(built[j], "token", "");
                if (strcmp(tj, "cc-exp") == 0) { drop = 1; break; }
            }
        }
        if (drop) json_free(built[i]);
        else json_append(fills, built[i]);
    }
    for (int i = 0; i < nv; i++) {
        free(values[i].token);
        if (values[i].value != exp_month && values[i].value != exp_year)
            free(values[i].value);
    }
    json_free(secret);
    json_free(field_tokens);
    return fills;
}

/* PoP: vl_build_otp_fills @ agent/vault_login_classifier.py:build_otp_fills */
json_t *vl_build_otp_fills(const vl_classified_t *otp, int n, const char *code)
{
    json_t *fills = json_array();
    if (!fills || !otp || n <= 0 || !code) return fills;

    int best = 0;
    for (int i = 1; i < n; i++) if (otp[i].score > otp[best].score) best = i;

    /* boxes: maxlength == 1, sorted by DOM index */
    int boxes[64], nb = 0;
    for (int i = 0; i < n && nb < 64; i++)
        if (otp[i].control.has_max_length && otp[i].control.max_length == 1)
            boxes[nb++] = i;
    for (int i = 1; i < nb; i++) {          /* insertion sort by control.index */
        int key = boxes[i], j = i - 1;
        while (j >= 0 && otp[boxes[j]].control.index > otp[key].control.index) {
            boxes[j + 1] = boxes[j];
            j--;
        }
        boxes[j + 1] = key;
    }

    size_t code_len = strlen(code);
    int same_form = 1, adjacent = 1;
    for (int i = 1; i < nb; i++) {
        if (otp[boxes[i]].control.has_form_index != otp[boxes[0]].control.has_form_index ||
            otp[boxes[i]].control.form_index != otp[boxes[0]].control.form_index)
            same_form = 0;
        if (otp[boxes[i]].control.index - otp[boxes[i - 1]].control.index != 1)
            adjacent = 0;
    }

    if ((size_t)nb == code_len && nb > 0 && same_form && adjacent) {
        for (int i = 0; i < nb; i++) {
            json_t *fill = json_object();
            char ch[2] = { code[i], '\0' };
            json_set(fill, "index", json_number((double)otp[boxes[i]].control.index));
            json_set(fill, "token", json_string("one-time-code"));
            json_set(fill, "value", json_string(ch));
            json_append(fills, fill);
        }
        return fills;
    }

    json_t *fill = json_object();
    json_set(fill, "index", json_number((double)otp[best].control.index));
    json_set(fill, "token", json_string("one-time-code"));
    json_set(fill, "value", json_string(code));
    json_append(fills, fill);
    return fills;
}
