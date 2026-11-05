/*
 * t_port_vault_login_classifier.c — Oracle harness for
 * agent/vault_login_classifier.py (classification half).
 */
#include "port_vault_login_classifier.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_str(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

static char g_control_json[2048];

/* control fixture: ac, form_index (-1 = absent), index, label, name, type,
 * max_length (-1 = absent) */
struct fx { const char *ac, *label, *name, *type; int form, index, maxlen; };

static void build_control(const struct fx *f, vl_control_t *c)
{
    json_t *o = json_object();
    json_set(o, "autocomplete", json_string(f->ac ? f->ac : ""));
    json_set(o, "index", json_number(f->index));
    json_set(o, "label", json_string(f->label ? f->label : ""));
    json_set(o, "name", json_string(f->name ? f->name : ""));
    json_set(o, "type", json_string(f->type ? f->type : ""));
    if (f->form >= 0) json_set(o, "formIndex", json_number(f->form));
    else json_set(o, "formIndex", json_null());
    if (f->maxlen >= 0) json_set(o, "maxLength", json_number(f->maxlen));
    else json_set(o, "maxLength", json_null());
    char *s = json_serialize(o);
    vl_control_from_json(s, c);
    snprintf(g_control_json, sizeof(g_control_json), "%s", s ? s : "{}");
    free(s);
    json_free(o);
}

static void emit_classified(const char *name, int rc, const vl_classified_t *c)
{
    printf("{\"family\":\"login\",\"case\":\"%s\",\"control\":%s,\"ok\":%d", name,
           g_control_json, rc == 0);
    if (rc == 0) {
        printf(",\"score\":%d,\"token\":", c->score);
        emit_json_str(c->token);
    }
    printf("}\n");
}

static void emit_login(const char *name, const struct fx *f)
{
    vl_control_t c;
    vl_classified_t out;
    build_control(f, &c);
    int rc = vl_classify_login_control(&c, &out);
    emit_classified(name, rc, &out);
    if (rc == 0) vl_classified_free(&out);
    vl_control_free(&c);
}

static void emit_checkout(const char *name, const struct fx *f)
{
    vl_control_t c;
    vl_classified_t out;
    build_control(f, &c);
    int rc = vl_classify_checkout_control(&c, &out);
    printf("{\"family\":\"checkout\",\"case\":\"%s\",\"control\":%s,\"ok\":%d", name,
           g_control_json, rc == 0);
    if (rc == 0) {
        printf(",\"score\":%d,\"token\":", out.score);
        emit_json_str(out.token);
    }
    printf("}\n");
    if (rc == 0) vl_classified_free(&out);
    vl_control_free(&c);
}

static void emit_otp(const char *name, const struct fx *fxs, int n, const char *code)
{
    vl_control_t *controls = (vl_control_t *)calloc((size_t)n, sizeof(*controls));
    char (*jsons)[2048] = calloc((size_t)n, 2048);
    for (int i = 0; i < n; i++) {
        build_control(&fxs[i], &controls[i]);
        snprintf(jsons[i], 2048, "%s", g_control_json);
    }
    vl_classified_t *otp = NULL;
    int count = vl_classify_otp_controls(controls, n, &otp);

    printf("{\"family\":\"otp\",\"case\":\"%s\",\"fixtures\":[", name);
    for (int i = 0; i < n; i++) {
        if (i) fputs(",", stdout);
        fputs(jsons[i], stdout);
    }
    printf("],\"controls\":[");
    for (int i = 0; i < count; i++) {
        if (i) fputs(",", stdout);
        printf("{\"index\":%d,\"score\":%d,\"token\":", otp[i].control.index, otp[i].score);
        emit_json_str(otp[i].token);
        printf("}");
    }
    printf("],\"fills\":");
    if (code) {
        json_t *fills = vl_build_otp_fills(otp, count, code);
        char *s = fills ? json_serialize(fills) : NULL;
        fputs(s ? s : "null", stdout);
        free(s);
        if (fills) json_free(fills);
    } else {
        fputs("null", stdout);
    }
    printf("}\n");

    vl_classified_list_free(otp, count);
    for (int i = 0; i < n; i++) vl_control_free(&controls[i]);
    free(controls);
    free(jsons);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* login classification */
    static const struct fx login[] = {
        { "username", "", "", "text", 0, 0, -1 },
        { "current-password", "", "", "password", 0, 1, -1 },
        { "email", "", "", "email", 0, 2, -1 },
        { "tel", "", "", "tel", 0, 3, -1 },
        { "new-password", "", "", "password", 0, 4, -1 },
        { "one-time-code", "", "", "text", 0, 5, -1 },
        { " webauthn new-password ", "", "", "text", 0, 6, -1 },
        { "", "Email address", "login_email", "text", 0, 7, -1 },
        { "", "e-mail", "", "text", 0, 8, -1 },
        { "", "Phone number", "phone", "text", 0, 9, -1 },
        { "", "User name", "login", "text", 0, 10, -1 },
        { "", "Confirm password", "password2", "text", 0, 11, -1 },
        { "", "Repeat Password", "", "text", 0, 12, -1 },
        { "", "New password", "", "password", 0, 13, -1 },
        { "", "", "newpassword", "password", 0, 14, -1 },
        { "", "Membership number", "member", "text", 0, 15, -1 },
        { "", "Caf\xc3\xa9 login", "account", "text", 0, 16, -1 },
        { "", "Random field", "random", "text", 0, 17, -1 },
        { "", "Password", "pwd", "PASSWORD", 0, 18, -1 },
        { "", "Password", "pwd", "password", 0, 19, -1 },
    };
    for (size_t i = 0; i < sizeof(login) / sizeof(login[0]); i++) {
        char name[32];
        snprintf(name, sizeof(name), "login_%zu", i);
        emit_login(name, &login[i]);
    }

    /* checkout classification */
    static const struct fx checkout[] = {
        { "cc-number", "", "", "text", 0, 0, -1 },
        { "cc-name", "", "", "text", 0, 1, -1 },
        { "cc-csc", "", "", "text", 0, 2, -1 },
        { "cc-exp", "", "", "text", 0, 3, -1 },
        { "cc-exp-month", "", "", "text", 0, 4, -1 },
        { "cc-exp-year", "", "", "text", 0, 5, -1 },
        { "address-line1", "", "", "text", 0, 6, -1 },
        { "address-level2", "", "", "text", 0, 7, -1 },
        { "postal-code", "", "", "text", 0, 8, -1 },
        { "country", "", "", "text", 0, 9, -1 },
        { "", "", "card_number", "text", 0, 10, -1 },
        { "", "", "cvv", "text", 0, 11, -1 },
        { "", "Name on card", "", "text", 0, 12, -1 },
        { "", "Expiry month", "", "select", 0, 13, -1 },
        { "", "Expiry year", "", "select", 0, 14, -1 },
        { "", "ZIP code", "", "text", 0, 15, -1 },
        { "", "Address line 2", "", "text", 0, 16, -1 },
        { "", "Suite", "", "text", 0, 17, -1 },
        { "", "City", "", "text", 0, 18, -1 },
        { "", "Province", "", "text", 0, 19, -1 },
        { "", "Country", "", "text", 0, 20, -1 },
        { "", "Email", "", "email", 0, 21, -1 },
        { "", "Security code", "", "password", 0, 22, -1 },
        { "", "Valid thru", "", "text", 0, 23, -1 },
        { "", "Pan", "", "text", 0, 24, -1 },
        { "", "Nothing here", "", "text", 0, 25, -1 },
    };
    for (size_t i = 0; i < sizeof(checkout) / sizeof(checkout[0]); i++) {
        char name[32];
        snprintf(name, sizeof(name), "co_%zu", i);
        emit_checkout(name, &checkout[i]);
    }

    /* OTP: single field, split boxes, loose boxes */
    {
        static const struct fx one[] = {
            { "", "Verification code", "otp", "text", 0, 3, -1 },
        };
        emit_otp("otp_single", one, 1, "123456");
    }
    {
        static const struct fx boxes[] = {
            { "", "code 1", "d1", "text", 0, 10, 1 },
            { "", "code 2", "d2", "text", 0, 11, 1 },
            { "", "code 3", "d3", "text", 0, 12, 1 },
            { "", "code 4", "d4", "text", 0, 13, 1 },
        };
        emit_otp("otp_boxes", boxes, 4, "9876");
    }
    {
        static const struct fx loose[] = {
            { "", "code", "a", "text", 0, 20, 1 },
            { "", "code", "b", "text", 1, 21, 1 },
            { "", "code", "c", "text", 0, 22, 1 },
            { "", "code", "d", "text", 0, 23, 1 },
        };
        emit_otp("otp_mixed_forms", loose, 4, "1111");
    }
    {
        static const struct fx gap[] = {
            { "", "code", "a", "text", 0, 30, 1 },
            { "", "code", "b", "text", 0, 32, 1 },
        };
        emit_otp("otp_gap", gap, 2, "12");
    }
    {
        static const struct fx auth[] = {
            { "one-time-code", "", "", "number", 0, 40, -1 },
            { "", "sms code", "", "tel", 0, 41, -1 },
            { "", "Search", "", "text", 0, 42, -1 },
            { "", "Password", "", "checkbox", 0, 43, -1 },
            { "", "Authenticator token", "", "", 0, 44, -1 },
        };
        emit_otp("otp_mixed_types", auth, 5, "554433");
    }

    /* password selection */
    {
        static const struct fx fxs[] = {
            { "", "", "pw1", "password", 0, 7, -1 },
            { "", "", "pw2", "password", 0, 3, -1 },
            { "current-password", "", "", "text", 0, 9, -1 },
        };
        vl_control_t controls[3];
        vl_classified_t listed[3];
        int n = 0;
        for (int i = 0; i < 3; i++) {
            build_control(&fxs[i], &controls[i]);
            if (vl_classify_login_control(&controls[i], &listed[n]) == 0) n++;
            vl_control_free(&controls[i]);
        }
        json_t *fills = vl_select_password_fill(listed, n, "s3cret");
        char *s = fills ? json_serialize(fills) : NULL;
        printf("{\"family\":\"select_password\",\"case\":\"best\",\"out\":%s}\n",
               s ? s : "null");
        free(s);
        if (fills) json_free(fills);

        json_t *empty = vl_select_password_fill(listed, n, "");
        s = empty ? json_serialize(empty) : NULL;
        printf("{\"family\":\"select_password\",\"case\":\"empty_password\",\"out\":%s}\n",
               s ? s : "null");
        free(s);
        if (empty) json_free(empty);
        vl_classified_list_free(listed, n);
    }

    /* checkout fills */
    {
        static const struct fx fxs[] = {
            { "cc-number", "", "", "text", 0, 1, -1 },
            { "", "Card number", "", "text", 0, 2, -1 },
            { "", "CVV", "", "text", 0, 3, -1 },
            { "", "Expiry month", "", "select", 0, 4, -1 },
            { "", "Expiry year", "", "select", 0, 5, -1 },
            { "", "City", "", "text", 0, 6, -1 },
        };
        vl_control_t controls[6];
        vl_classified_t listed[6];
        int n = 0;
        for (int i = 0; i < 6; i++) {
            build_control(&fxs[i], &controls[i]);
            if (vl_classify_checkout_control(&controls[i], &listed[n]) == 0) n++;
            vl_control_free(&controls[i]);
        }
        const char *secret = "{\"card\":\"4242\",\"cvv\":\"123\",\"exp_month\":\"3\",\"exp_year\":\"2029\",\"city\":\"Paris\"}";
        const char *map = "{\"card\":\"cc-number\",\"cvv\":\"cc-csc\",\"exp_month\":\"cc-exp-month\",\"exp_year\":\"cc-exp-year\",\"city\":\"address-level2\"}";
        json_t *fills = vl_select_checkout_fills(listed, n, secret, map);
        char *s = fills ? json_serialize(fills) : NULL;
        printf("{\"family\":\"select_checkout\",\"case\":\"combined_exp\",\"out\":%s}\n",
               s ? s : "null");
        free(s);
        if (fills) json_free(fills);

        const char *secret2 = "{\"card\":\"4242\",\"cvv\":\"\",\"exp_month\":\"3\",\"city\":\"Paris\"}";
        json_t *f2 = vl_select_checkout_fills(listed, n, secret2, map);
        s = f2 ? json_serialize(f2) : NULL;
        printf("{\"family\":\"select_checkout\",\"case\":\"missing_values\",\"out\":%s}\n",
               s ? s : "null");
        free(s);
        if (f2) json_free(f2);
        vl_classified_list_free(listed, n);
    }
    return 0;
}
