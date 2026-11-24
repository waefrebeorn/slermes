/*
 * vault_login_classifier.c — faithful C11 port of agent/vault_login_classifier.py
 * (gate w16_vaultlogin_58f247).
 *
 * PoP: w16_vaultlogin_58f247_classify_login @ agent/vault_login_classifier.py:classify_login_control
 * PoP: w16_vaultlogin_58f247_classify_checkout @ agent/vault_login_classifier.py:classify_checkout_control
 * PoP: w16_vaultlogin_58f247_classify_otp @ agent/vault_login_classifier.py:classify_otp_controls
 * PoP: w16_vaultlogin_58f247_select_password @ agent/vault_login_classifier.py:select_password_fill
 * PoP: w16_vaultlogin_58f247_select_checkout @ agent/vault_login_classifier.py:select_checkout_fills
 * PoP: w16_vaultlogin_58f247_build_otp_fills @ agent/vault_login_classifier.py:build_otp_fills
 *
 * Python's regexes are expressed over the same normalised searchable text
 * (lower + NFKD-lite + re.sub(r'[^a-z0-9]+',' ')). ASCII/Latin inputs are
 * covered; full Unicode NFKD decomposition of exotic scripts is not.
 */
#include "noc_w16_vaultlogin_58f247.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

typedef struct {
    char *autocomplete;
    long long form_index; int has_form_index;
    long long index;
    char *label, *name, *type;
    long long max_length; int has_max_length;
} Control;

typedef struct { const Control *c; int score; const char *token; } Classified;

static const json_t *first_present(const json_t *o, const char *k1, const char *k2) {
    const json_t *v = o && o->type == JSON_OBJECT ? json_obj_get(o, k1) : NULL;
    if (!v || v->type == JSON_NULL) v = o && o->type == JSON_OBJECT ? json_obj_get(o, k2) : NULL;
    return v;
}

static long long read_int(const json_t *v, long long dflt) {
    if (!v || v->type == JSON_NULL) return dflt;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_STRING) return strtoll(v->str_val ? v->str_val : "0", NULL, 10);
    return dflt;
}

static void control_init(Control *c, const json_t *o) {
    const json_t *v;
    memset(c, 0, sizeof(*c));
    c->autocomplete = dupstr(json_get_str(o, "autocomplete", ""));
    c->label = dupstr(json_get_str(o, "label", ""));
    c->name = dupstr(json_get_str(o, "name", ""));
    c->type = dupstr(json_get_str(o, "type", ""));
    v = first_present(o, "formIndex", "form_index");
    c->has_form_index = (v && v->type != JSON_NULL) ? 1 : 0;
    c->form_index = read_int(v, 0);
    v = first_present(o, "maxLength", "max_length");
    c->has_max_length = (v && v->type != JSON_NULL) ? 1 : 0;
    c->max_length = read_int(v, 0);
    c->index = read_int(o && o->type == JSON_OBJECT ? json_obj_get(o, "index") : NULL, 0);
}

static void control_free(Control *c) {
    free(c->autocomplete); free(c->label); free(c->name); free(c->type);
    memset(c, 0, sizeof(*c));
}

/* Same normalisation as the Python module's _normalize_text. */
static char *normalize(const char *name, const char *label) {
    size_t cap = (name ? strlen(name) : 0) + (label ? strlen(label) : 0) + 4;
    char *tmp = (char *)malloc(cap + 1);
    char *out;
    size_t i = 0, j = 0;
    int have = 0;
    if (!tmp) return dupstr("");
    if (name && name[0]) { for (size_t k = 0; name[k]; ++k) tmp[i++] = (char)tolower((unsigned char)name[k]); have = 1; }
    if (label && label[0]) {
        if (have) tmp[i++] = ' ';
        for (size_t k = 0; label[k]; ++k) tmp[i++] = (char)tolower((unsigned char)label[k]);
    }
    tmp[i] = '\0';
    out = (char *)malloc(i + 2);
    if (!out) { free(tmp); return dupstr(""); }
    for (size_t k = 0; k < i; ++k) {
        unsigned char ch = (unsigned char)tmp[k];
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) out[j++] = (char)ch;
        else if (j > 0 && out[j - 1] != ' ') out[j++] = ' ';
    }
    if (j > 0 && out[j - 1] == ' ') j--;
    out[j] = '\0';
    free(tmp);
    return out;
}

/* Whole-phrase / whole-word match inside normalised text. */
static int has_phrase(const char *norm, const char *phrase) {
    size_t lp = strlen(phrase);
    const char *p = norm;
    if (!lp) return 0;
    while ((p = strstr(p, phrase)) != NULL) {
        int lok = (p == norm) || (p[-1] == ' ');
        int rok = (p[lp] == '\0') || (p[lp] == ' ');
        if (lok && rok) return 1;
        p++;
    }
    return 0;
}

static int any_phrase(const char *norm, const char *const *phrases) {
    size_t i;
    for (i = 0; phrases[i]; ++i) if (has_phrase(norm, phrases[i])) return 1;
    return 0;
}

static int has_token(const char *lower_ac, const char *tok) {
    const char *p = lower_ac;
    size_t lt = strlen(tok);
    while (*p) {
        const char *s;
        size_t n;
        while (*p && isspace((unsigned char)*p)) p++;
        s = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        n = (size_t)(p - s);
        if (n == lt && !memcmp(s, tok, n)) return 1;
    }
    return 0;
}

static char *lower_dup(const char *s) {
    char *t = dupstr(s ? s : "");
    for (size_t i = 0; t && t[i]; ++i) t[i] = (char)tolower((unsigned char)t[i]);
    return t;
}

static json_t *class_result(int ok, int score, const char *token) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(ok ? 1 : 0));
    json_set(o, "score", json_number((double)score));
    json_set(o, "token", token ? json_string(token) : json_null());
    return o;
}

static const char *const EXCLUDED_PASSWORD[] = {
    "new password","confirm password","create password","repeat password",
    "newpassword","confirmpassword","createpassword","repeatpassword", NULL};
static const char *const EMAIL_PHRASES[] = {"email","e mail","email address", NULL};
static const char *const TEL_PHRASES[] = {"phone","telephone","mobile", NULL};
static const char *const USERNAME_PHRASES[] = {
    "username","user name","login","account","member","membership","mileageplus", NULL};

static const char *const PAYMENT_TOKENS[] = {
    "cc-number","cc-name","cc-exp","cc-exp-month","cc-exp-year","cc-csc", NULL};
static const char *const ADDRESS_TOKENS[] = {
    "address-line1","address-line2","address-level2","address-level1","postal-code","country-name","country", NULL};

typedef struct { const char *const *phrases; const char *token; } Heur;

static const Heur CHECKOUT_HEURS[] = {
    { (const char *const[]){"card number","cardnumber","ccnumber","cc num","ccnum","pan", NULL}, "cc-number" },
    { (const char *const[]){"name on card","nameoncard","cardholder","cc name","ccname", NULL}, "cc-name" },
    { (const char *const[]){"cvc","cvv","csc","security code","securitycode","card code","cardcode", NULL}, "cc-csc" },
    { (const char *const[]){"exp month","expmonth","expiry month","expirymonth","expiration month","expirationmonth","exp mm","expmm","ccmonth", NULL}, "cc-exp-month" },
    { (const char *const[]){"exp year","expyear","expiry year","expiryyear","expiration year","expirationyear","exp yy","expyy","exp yyyy","expyyyy","ccyear", NULL}, "cc-exp-year" },
    { (const char *const[]){"exp","expiry","expiration","exp date","expdate","expiry date","expirydate","expiration date","expirationdate","mm yy","mmyy","valid thru","validthru", NULL}, "cc-exp" },
    { (const char *const[]){"address 2","addressline2","address line 2","apt","suite","unit", NULL}, "address-line2" },
    { (const char *const[]){"address","addressline1","address line 1","street", NULL}, "address-line1" },
    { (const char *const[]){"city","town","locality", NULL}, "address-level2" },
    { (const char *const[]){"state","province","region","county", NULL}, "address-level1" },
    { (const char *const[]){"zip","postal","postcode", NULL}, "postal-code" },
    { (const char *const[]){"country", NULL}, "country-name" },
};
static const size_t N_CHECKOUT_HEURS = sizeof(CHECKOUT_HEURS) / sizeof(CHECKOUT_HEURS[0]);

static const char *const OTP_TRIGGERS[] = {
    "one time","onetime","verification","security","auth","authentication","authenticator",
    "2fa","two factor","twofactor","mfa","totp","otp","passcode","sms", NULL};
static const char *const OTP_TARGETS[] = {"code","pin","token", NULL};
static const char *const OTP_STANDALONE[] = {"otp","totp","2fa","mfa","verification code","verificationcode","passcode", NULL};

static int otp_match(const char *norm) {
    size_t i;
    if (any_phrase(norm, OTP_STANDALONE)) return 1;
    for (i = 0; OTP_TRIGGERS[i]; ++i) {
        const char *p = norm;
        size_t lt = strlen(OTP_TRIGGERS[i]);
        while ((p = strstr(p, OTP_TRIGGERS[i])) != NULL) {
            int lok = (p == norm) || (p[-1] == ' ');
            int rok = (p[lt] == '\0') || (p[lt] == ' ');
            if (lok && rok && any_phrase(p + lt, OTP_TARGETS)) return 1;
            p++;
        }
    }
    return 0;
}

json_t *w16_vaultlogin_58f247_classify_login(const json_t *o) {
    Control c;
    char *ac, *searchable;
    json_t *res;
    const char *tok;
    static const char *const LOGIN_TOKENS[] = {"username","email","tel","current-password", NULL};
    size_t i;
    control_init(&c, o);
    ac = lower_dup(c.autocomplete);
    if (has_token(ac, "new-password") || has_token(ac, "one-time-code")) {
        free(ac); control_free(&c); return class_result(0, 0, NULL);
    }
    for (i = 0; LOGIN_TOKENS[i]; ++i) {
        if (has_token(ac, LOGIN_TOKENS[i])) {
            res = class_result(1, 100, LOGIN_TOKENS[i]);
            free(ac); control_free(&c); return res;
        }
    }
    free(ac);
    searchable = normalize(c.name, c.label);
    if (any_phrase(searchable, EXCLUDED_PASSWORD)) {
        free(searchable); control_free(&c); return class_result(0, 0, NULL);
    }
    tok = NULL;
    if (!strcmp(c.type, "password")) { res = class_result(1, 90, "current-password"); goto done; }
    if (!strcmp(c.type, "email")) { res = class_result(1, 85, "email"); goto done; }
    if (!strcmp(c.type, "tel")) { res = class_result(1, 85, "tel"); goto done; }
    if (any_phrase(searchable, EMAIL_PHRASES)) { res = class_result(1, 75, "email"); goto done; }
    if (any_phrase(searchable, TEL_PHRASES)) { res = class_result(1, 75, "tel"); goto done; }
    if (any_phrase(searchable, USERNAME_PHRASES)) { res = class_result(1, 70, "username"); goto done; }
    res = class_result(0, 0, NULL);
done:
    (void)tok;
    free(searchable); control_free(&c);
    return res;
}

json_t *w16_vaultlogin_58f247_classify_checkout(const json_t *o) {
    Control c;
    char *ac;
    json_t *res = NULL;
    char *searchable;
    size_t i;
    control_init(&c, o);
    ac = lower_dup(c.autocomplete);
    for (i = 0; PAYMENT_TOKENS[i]; ++i) {
        if (has_token(ac, PAYMENT_TOKENS[i])) {
            const char *t = !strcmp(PAYMENT_TOKENS[i], "country") ? "country-name" : PAYMENT_TOKENS[i];
            res = class_result(1, 100, t); break;
        }
    }
    if (!res) for (i = 0; ADDRESS_TOKENS[i]; ++i) {
        if (has_token(ac, ADDRESS_TOKENS[i])) {
            const char *t = !strcmp(ADDRESS_TOKENS[i], "country") ? "country-name" : ADDRESS_TOKENS[i];
            res = class_result(1, 100, t); break;
        }
    }
    free(ac);
    if (res) { control_free(&c); return res; }
    if (!strcmp(c.type, "password") || !strcmp(c.type, "email")) {
        control_free(&c); return class_result(0, 0, NULL);
    }
    searchable = normalize(c.name, c.label);
    for (i = 0; i < N_CHECKOUT_HEURS; ++i) {
        if (any_phrase(searchable, CHECKOUT_HEURS[i].phrases)) {
            res = class_result(1, 70, CHECKOUT_HEURS[i].token);
            break;
        }
    }
    free(searchable); control_free(&c);
    return res ? res : class_result(0, 0, NULL);
}

static Classified *classify_controls(const json_t *controls, size_t *out_n, int mode) {
    size_t n = (controls && controls->type == JSON_ARRAY) ? controls->c.count : 0;
    Classified *list = n ? (Classified *)calloc(n, sizeof(Classified)) : NULL;
    size_t k = 0, i;
    for (i = 0; i < n; ++i) {
        Control c;
        control_init(&c, json_get(controls, i));
        if (mode == 1) { /* otp */
            char *ac = lower_dup(c.autocomplete);
            char *searchable = normalize(c.name, c.label);
            int hit = 0;
            const char *token = NULL;
            int score = 0;
            if (has_token(ac, "one-time-code")) { hit = 1; score = 100; token = "one-time-code"; }
            else if (!strcmp(c.type, "text") || !strcmp(c.type, "tel") || !strcmp(c.type, "number")
                     || !strcmp(c.type, "password") || !strcmp(c.type, "")) {
                if (otp_match(searchable)) { hit = 1; score = 70; token = "one-time-code"; }
            }
            if (hit) { list[k].c = NULL; list[k].score = score; list[k].token = token;
                       /* keep a heap copy of the control so fills can read index/form/max */
                       { Control *cp = (Control *)malloc(sizeof(Control)); *cp = c; list[k].c = cp; k++; } }
            else { control_free(&c); }
            free(ac); free(searchable);
        } else { /* login or checkout */
            char *ac = lower_dup(c.autocomplete);
            char *searchable = normalize(c.name, c.label);
            int hit = 0, score = 0; const char *token = NULL; size_t j;
            if (mode == 0) {
                static const char *const LOGIN_TOKENS[] = {"username","email","tel","current-password", NULL};
                if (has_token(ac,"new-password") || has_token(ac,"one-time-code")) hit = 0;
                else {
                    for (j = 0; LOGIN_TOKENS[j] && !hit; ++j)
                        if (has_token(ac, LOGIN_TOKENS[j])) { hit=1; score=100; token=LOGIN_TOKENS[j]; }
                    if (!hit && !any_phrase(searchable, EXCLUDED_PASSWORD)) {
                        if (!strcmp(c.type,"password")) { hit=1; score=90; token="current-password"; }
                        else if (!strcmp(c.type,"email")) { hit=1; score=85; token="email"; }
                        else if (!strcmp(c.type,"tel")) { hit=1; score=85; token="tel"; }
                        else if (any_phrase(searchable,EMAIL_PHRASES)) { hit=1; score=75; token="email"; }
                        else if (any_phrase(searchable,TEL_PHRASES)) { hit=1; score=75; token="tel"; }
                        else if (any_phrase(searchable,USERNAME_PHRASES)) { hit=1; score=70; token="username"; }
                    }
                }
            } else {
                for (j = 0; PAYMENT_TOKENS[j] && !hit; ++j)
                    if (has_token(ac, PAYMENT_TOKENS[j])) {
                        hit=1; score=100;
                        token = !strcmp(PAYMENT_TOKENS[j],"country") ? "country-name" : PAYMENT_TOKENS[j];
                    }
                for (j = 0; ADDRESS_TOKENS[j] && !hit; ++j)
                    if (has_token(ac, ADDRESS_TOKENS[j])) {
                        hit=1; score=100;
                        token = !strcmp(ADDRESS_TOKENS[j],"country") ? "country-name" : ADDRESS_TOKENS[j];
                    }
                if (!hit && strcmp(c.type,"password") && strcmp(c.type,"email")) {
                    for (j = 0; j < N_CHECKOUT_HEURS; ++j)
                        if (any_phrase(searchable, CHECKOUT_HEURS[j].phrases)) {
                            hit=1; score=70; token=CHECKOUT_HEURS[j].token; break;
                        }
                }
            }
            free(ac); free(searchable);
            if (hit) { Control *cp = (Control *)malloc(sizeof(Control)); *cp = c;
                       list[k].c = cp; list[k].score = score; list[k].token = token; k++; }
            else control_free(&c);
        }
    }
    *out_n = k;
    return list;
}

static void free_classified(Classified *list, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) { if (list[i].c) { control_free((Control *)list[i].c); free((void *)list[i].c); } }
    free(list);
}

json_t *w16_vaultlogin_58f247_classify_otp(const json_t *controls) {
    size_t n = 0, i;
    Classified *list = classify_controls(controls, &n, 1);
    json_t *arr = json_array();
    for (i = 0; i < n; ++i) {
        json_t *o = json_object();
        json_set(o, "index", json_number((double)list[i].c->index));
        json_set(o, "score", json_number((double)list[i].score));
        json_set(o, "token", json_string(list[i].token));
        json_append(arr, o);
    }
    free_classified(list, n);
    return arr;
}

static json_t *fill_obj(long long index, const char *token, const char *value) {
    json_t *o = json_object();
    json_set(o, "index", json_number((double)index));
    json_set(o, "token", json_string(token));
    json_set(o, "value", json_string(value));
    return o;
}

json_t *w16_vaultlogin_58f247_select_password(const json_t *controls, const char *password) {
    json_t *arr = json_array();
    size_t n = 0, i;
    Classified *list;
    long long best_index = 0;
    int best_score = 0, found = 0;
    if (!password || !password[0]) { (void)controls; return arr; }
    list = classify_controls(controls, &n, 0);
    for (i = 0; i < n; ++i) {
        if (!strcmp(list[i].token, "current-password")) {
            if (!found || list[i].score > best_score ||
                (list[i].score == best_score && list[i].c->index < best_index)) {
                found = 1; best_score = list[i].score; best_index = list[i].c->index;
            }
        }
    }
    if (found) json_append(arr, fill_obj(best_index, "current-password", password));
    free_classified(list, n);
    return arr;
}

static char *zfill2(const char *s) {
    size_t n = strlen(s);
    char *out;
    if (n >= 2) return dupstr(s);
    out = (char *)malloc(3);
    if (!out) return dupstr(s);
    out[0] = '0'; memcpy(out + 1, s, n + 1);
    return out;
}

static const char *last2(const char *s) {
    size_t n = strlen(s);
    return n <= 2 ? s : s + n - 2;
}

json_t *w16_vaultlogin_58f247_select_checkout(const json_t *controls, const json_t *secret,
                                              const json_t *field_tokens) {
    size_t n = 0, i;
    Classified *list = classify_controls(controls, &n, 2);
    json_t *arr = json_array();
    char **vtoks = NULL; char **vvals = NULL; size_t nv = 0, cap = 0;
    int have_month = 0, have_year = 0;
    char *month = NULL, *year = NULL;
    const char *mtok = NULL, *ytok = NULL;

    if (field_tokens && field_tokens->type == JSON_OBJECT) {
        size_t m = json_object_size(field_tokens);
        cap = m + 1;
        vtoks = (char **)calloc(cap, sizeof(char *));
        vvals = (char **)calloc(cap, sizeof(char *));
        for (i = 0; i < m; ++i) {
            const char *field = json_object_get_key_at(field_tokens, i);
            const json_t *tv = json_object_get_at(field_tokens, i);
            const json_t *sv = secret && secret->type == JSON_OBJECT ? json_obj_get(secret, field) : NULL;
            const char *token = tv && tv->type == JSON_STRING ? tv->str_val : NULL;
            if (!field || !token || !sv || sv->type != JSON_STRING || !sv->str_val || !sv->str_val[0]) continue;
            vtoks[nv] = dupstr(token);
            vvals[nv] = dupstr(sv->str_val);
            if (!strcmp(token, "cc-exp-month")) { have_month = 1; month = vvals[nv]; mtok = token; }
            if (!strcmp(token, "cc-exp-year")) { have_year = 1; year = vvals[nv]; ytok = token; }
            nv++;
        }
    }
    if (have_month && have_year) {
        char *mm = zfill2(month);
        char *yy2 = dupstr(last2(year));
        size_t need = strlen(mm) + 1 + strlen(yy2) + 1;
        char *combined = (char *)malloc(need);
        if (combined) {
            snprintf(combined, need, "%s/%s", mm, yy2);
            vtoks[nv] = dupstr("cc-exp");
            vvals[nv] = combined;
            nv++;
        }
        free(mm); free(yy2);
    }
    (void)mtok; (void)ytok;

    for (i = 0; i < nv; ++i) {
        size_t j;
        long long best_index = 0; int best_score = 0, found = 0;
        for (j = 0; j < n; ++j) {
            if (!strcmp(list[j].token, vtoks[i])) {
                if (!found || list[j].score > best_score ||
                    (list[j].score == best_score && list[j].c->index < best_index)) {
                    found = 1; best_score = list[j].score; best_index = list[j].c->index;
                }
            }
        }
        if (found) json_append(arr, fill_obj(best_index, vtoks[i], vvals[i]));
    }
    {
        int has_exp = 0;
        for (i = 0; i < json_array_size(arr); ++i) {
            const json_t *o = json_get(arr, i);
            const json_t *t = o && o->type == JSON_OBJECT ? json_obj_get(o, "token") : NULL;
            if (t && t->type == JSON_STRING && !strcmp(t->str_val, "cc-exp")) { has_exp = 1; break; }
        }
        if (has_exp) {
            json_t *kept = json_array();
            for (i = 0; i < json_array_size(arr); ++i) {
                const json_t *o = json_get(arr, i);
                const json_t *t = o && o->type == JSON_OBJECT ? json_obj_get(o, "token") : NULL;
                if (t && t->type == JSON_STRING &&
                    (!strcmp(t->str_val, "cc-exp-month") || !strcmp(t->str_val, "cc-exp-year"))) continue;
                json_append(kept, json_copy(o));
            }
            json_free(arr); arr = kept;
        }
    }
    for (i = 0; i < nv; ++i) { free(vtoks[i]); free(vvals[i]); }
    free(vtoks); free(vvals);
    free_classified(list, n);
    return arr;
}

static int cmp_class_index(const void *a, const void *b) {
    const Classified *ca = *(const Classified *const *)a;
    const Classified *cb = *(const Classified *const *)b;
    if (ca->c->index < cb->c->index) return -1;
    if (ca->c->index > cb->c->index) return 1;
    return 0;
}

json_t *w16_vaultlogin_58f247_build_otp_fills(const json_t *controls, const char *code) {
    json_t *arr = json_array();
    size_t n = 0, i, nb = 0;
    Classified *list = classify_controls(controls, &n, 1);
    Classified *best = NULL;
    Classified **boxes = NULL;
    size_t clen = code ? strlen(code) : 0;

    for (i = 0; i < n; ++i)
        if (!best || list[i].score > best->score) best = &list[i];
    if (!best) { free_classified(list, n); return arr; }

    boxes = n ? (Classified **)calloc(n, sizeof(Classified *)) : NULL;
    for (i = 0; i < n; ++i)
        if (list[i].c->has_max_length && list[i].c->max_length == 1) boxes[nb++] = &list[i];
    if (nb > 1) qsort(boxes, nb, sizeof(Classified *), cmp_class_index);

    if (nb == clen && nb > 0) {
        int one_form = 1, adjacent = 1;
        int have_form = boxes[0]->c->has_form_index;
        long long form = boxes[0]->c->form_index;
        for (i = 1; i < nb; ++i) {
            if (boxes[i]->c->has_form_index != have_form ||
                (have_form && boxes[i]->c->form_index != form)) { one_form = 0; break; }
        }
        for (i = 1; i < nb; ++i)
            if (boxes[i]->c->index - boxes[i - 1]->c->index != 1) { adjacent = 0; break; }
        if (one_form && adjacent) {
            for (i = 0; i < nb; ++i) {
                char val[2] = {code[i], '\0'};
                json_append(arr, fill_obj(boxes[i]->c->index, "one-time-code", val));
            }
            free(boxes); free_classified(list, n);
            return arr;
        }
    }
    json_append(arr, fill_obj(best->c->index, "one-time-code", code ? code : ""));
    free(boxes); free_classified(list, n);
    return arr;
}
