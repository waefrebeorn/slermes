/*
 * free_tier_task_done.c -- real C11 port of upstream Hermes
 *   tui_gateway/free_tier_task_done.py.
 *
 * PoP: w22_freetierta_e95c57_is_anonymous_agent @ hermes_cli/anon_auth.py:is_anonymous_agent
 * PoP: w22_freetierta_e95c57_is_clean_task @ tui_gateway/free_tier_task_done.py:is_clean_task
 * PoP: w22_freetierta_e95c57_should_record @ tui_gateway/free_tier_task_done.py:note_task_done
 *
 * `note_task_done` itself only wraps the decision in a try/except and calls
 * `record_task_done()`; that write is the seam. The decision -- and the
 * `is_anonymous_agent` read it ends with, including the JWT payload claim --
 * is computed here.
 */
#include "hermes_json.h"
#include "noc_w22_freetierta_e95c57.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FT_ANON_TIER "anonymous"

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int ft_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* str(x) for the scalar shapes a final_response can carry. */
static char *ft_str_scalar(const json_t *v) {
    char buf[64];
    size_t n;
    char *out;
    if (!v) return NULL;
    switch (v->type) {
    case JSON_STRING:
        buf[0] = '\0';
        break;
    case JSON_BOOL:
        snprintf(buf, sizeof(buf), "%s", v->bool_val ? "True" : "False");
        break;
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else {
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        }
        break;
    default:
        return NULL;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        n = strlen(s);
        out = (char *)malloc(n + 1);
        if (!out) return NULL;
        memcpy(out, s, n + 1);
        return out;
    }
    n = strlen(buf);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, buf, n + 1);
    return out;
}

/* `str(result.get("final_response") or "").strip()` is empty? */
static int ft_blank_response(const json_t *result) {
    json_t *v = json_obj_get(result, "final_response");
    char *text;
    int blank;
    if (!ft_truthy(v)) return 1;            /* `or ""` */
    text = ft_str_scalar(v);
    if (!text) return 0;                    /* a container renders non-blank */
    blank = text[0] == '\0';
    if (!blank) {
        const char *p = text;
        int only_space = 1;
        while (*p) {
            char c = *p++;
            if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v')) {
                only_space = 0;
                break;
            }
        }
        blank = only_space;
    }
    free(text);
    return blank;
}

/* --- _decode_jwt_claims --------------------------------------------------- */

static int ft_b64_val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

/* base64.urlsafe_b64decode of an already length-checked body: characters
 * outside the alphabet are dropped (binascii's default is not to validate), and
 * a leftover that cannot form a byte reads as a decode failure. */
static unsigned char *ft_b64url_decode(const char *in, size_t *out_len) {
    size_t cap = strlen(in) / 4 * 3 + 4;
    unsigned char *out = (unsigned char *)malloc(cap);
    size_t len = 0;
    int sextet[4];
    int have = 0;
    const char *p = in;
    if (!out) return NULL;
    while (*p) {
        int v = ft_b64_val((unsigned char)*p++);
        if (v < 0) continue;               /* '=', whitespace, junk */
        sextet[have++] = v;
        if (have == 4) {
            unsigned int word = ((unsigned)sextet[0] << 18) | ((unsigned)sextet[1] << 12)
                              | ((unsigned)sextet[2] << 6) | (unsigned)sextet[3];
            out[len++] = (unsigned char)((word >> 16) & 0xFF);
            out[len++] = (unsigned char)((word >> 8) & 0xFF);
            out[len++] = (unsigned char)(word & 0xFF);
            have = 0;
        }
    }
    if (have == 1) { free(out); return NULL; }
    if (have == 2) {
        unsigned int word = ((unsigned)sextet[0] << 18) | ((unsigned)sextet[1] << 12);
        out[len++] = (unsigned char)((word >> 16) & 0xFF);
    } else if (have == 3) {
        unsigned int word = ((unsigned)sextet[0] << 18) | ((unsigned)sextet[1] << 12)
                          | ((unsigned)sextet[2] << 6);
        out[len++] = (unsigned char)((word >> 16) & 0xFF);
        out[len++] = (unsigned char)((word >> 8) & 0xFF);
    }
    out[len] = '\0';                       /* NUL-terminate after shortening */
    *out_len = len;
    return out;
}

/* `_decode_jwt_claims(api_key).get("account_tier") == ANON_ACCOUNT_TIER` */
static int ft_token_is_anon(const char *token) {
    const char *dot1, *dot2;
    size_t body_len;
    char *body;
    unsigned char *raw;
    size_t raw_len = 0;
    json_t *claims;
    int is_anon = 0;

    if (!token) return 0;
    dot1 = strchr(token, '.');
    if (!dot1) return 0;
    dot2 = strchr(dot1 + 1, '.');
    if (!dot2) return 0;
    if (strchr(dot2 + 1, '.')) return 0;   /* token.count(".") != 2 */

    body_len = (size_t)(dot2 - (dot1 + 1));
    body = (char *)malloc(body_len + 5);
    if (!body) return 0;
    memcpy(body, dot1 + 1, body_len);
    body[body_len] = '\0';                 /* NUL-terminate the payload slice */
    /* payload += "=" * ((4 - len(payload) % 4) % 4) */
    while (strlen(body) % 4) strcat(body, "=");

    raw = ft_b64url_decode(body, &raw_len);
    free(body);
    if (!raw || raw_len == 0) { free(raw); return 0; }
    claims = json_parse((const char *)raw, NULL);
    free(raw);
    if (!claims) return 0;
    if (claims->type == JSON_OBJECT) {
        json_t *tier = json_obj_get(claims, "account_tier");
        if (tier && tier->type == JSON_STRING && tier->str_val
            && !strcmp(tier->str_val, FT_ANON_TIER)) {
            is_anon = 1;
        }
    }
    json_free(claims);
    return is_anon;
}

int w22_freetierta_e95c57_is_anonymous_agent(const char *provider, const char *api_key) {
    if (!provider || strcmp(provider, "nous")) return 0;
    return ft_token_is_anon(api_key);
}

/* --- is_clean_task / note_task_done -------------------------------------- */

int w22_freetierta_e95c57_is_clean_task(const json_t *result, const char *provider,
                                        const char *api_key, int user_input,
                                        const char *display_kind) {
    if (display_kind && !strcmp(display_kind, "auto_continue")) return 0;
    if (user_input) return 0;
    if (!result || result->type != JSON_OBJECT) return 0;      /* isinstance(dict) */
    if (!ft_truthy(json_obj_get(result, "completed"))) return 0;
    if (ft_truthy(json_obj_get(result, "pending_steer"))) return 0;
    if (ft_truthy(json_obj_get(result, "user_intervened"))) return 0;
    if (ft_blank_response(result)) return 0;
    return w22_freetierta_e95c57_is_anonymous_agent(provider, api_key);
}

int w22_freetierta_e95c57_should_record(const json_t *session, const json_t *result,
                                        const char *provider, const char *api_key,
                                        const char *display_kind) {
    int user_input;
    /* `session.get(...)` on anything but a mapping raises, and note_task_done
     * swallows it: the offer is not armed. */
    if (!session || session->type != JSON_OBJECT) return 0;
    user_input = ft_truthy(json_obj_get(session, "_turn_user_input")) ? 1 : 0;
    return w22_freetierta_e95c57_is_clean_task(result, provider, api_key, user_input,
                                              display_kind);
}
