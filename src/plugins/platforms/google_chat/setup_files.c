/*
 * setup_files.c -- C11 port of plugins/platforms/google_chat/setup_files.py.
 *
 * PoP: w20_setupfiles_7eb3bd_run @ plugins/platforms/google_chat/setup_files.py:handle_setup_files_command
 * PoP: w20_setupfiles_7eb3bd_split_arg @ plugins/platforms/google_chat/setup_files.py:handle_setup_files_command
 * PoP: w20_setupfiles_7eb3bd_sender_key @ plugins/platforms/google_chat/setup_files.py:handle_setup_files_command
 *
 * The OAuth helpers (their stdout capture, their SystemExit failure signal), the
 * adapter's message send and the i18n catalog stay upstream: the fixture states
 * what each answers, and the port reproduces the reply sequence, the credential
 * slot and the one exception the command can leak (start with empty output).
 */
#include "hermes_json.h"
#include "noc_w20_setupfiles_7eb3bd.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SF_K "platform.google_chat.setup_files."

static char *sf_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int sf_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static void sf_strip(char *s) {
    char *p = s;
    size_t n;
    while (*p && sf_isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n && sf_isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

char *w20_setupfiles_7eb3bd_split_arg(const char *raw_text) {
    const char *p = raw_text ? raw_text : "";
    char *arg;
    while (*p && sf_isspace((unsigned char)*p)) p++;
    while (*p && !sf_isspace((unsigned char)*p)) p++;
    if (*p == '\0') return sf_dup("");       /* one token only: no argument */
    while (*p && sf_isspace((unsigned char)*p)) p++;
    arg = sf_dup(p);
    if (!arg) return NULL;
    sf_strip(arg);
    return arg;
}

char *w20_setupfiles_7eb3bd_sender_key(const char *sender_email) {
    char *out;
    size_t i;
    if (!sender_email || !sender_email[0]) return NULL;
    out = sf_dup(sender_email);
    if (!out) return NULL;
    sf_strip(out);
    for (i = 0; out[i]; i++) out[i] = (char)tolower((unsigned char)out[i]);
    return out;
}

/* --- trace state ----------------------------------------------------------- */

typedef struct {
    json_t *replies;
    const char *sender_key;   /* may be NULL or "" -- both are falsy in Python */
    int shared_set;
    int api_set;
    int by_email;
} sf_state;

static json_t *sf_marker(const char *suffix, json_t *kwargs) {
    json_t *m = json_object();
    char key[128];
    snprintf(key, sizeof(key), "%s%s", SF_K, suffix);
    json_set(m, "key", json_string(key));
    json_set(m, "kwargs", kwargs ? kwargs : json_object());
    return m;
}

static json_t *sf_marker_kw(const char *suffix) {
    return sf_marker(suffix, NULL);
}

static void sf_reply(sf_state *st, json_t *text, const char *thread_id) {
    json_t *body = json_object();
    json_set(body, "text", text);
    if (thread_id && thread_id[0]) {
        json_t *thread = json_object();
        json_set(thread, "name", json_string(thread_id));
        json_set(body, "thread", thread);
    }
    json_append(st->replies, body);
}

/* _set_user_creds: the shared slot when no sender key, else that key's slot. */
static void sf_set_creds(sf_state *st, int have_creds, int have_api) {
    if (!st->sender_key || !st->sender_key[0]) {
        st->shared_set = have_creds;
        st->api_set = have_api;
    } else if (!have_creds) {
        st->by_email = 0;
    } else {
        st->by_email = 1;
    }
}

/* str.strip().splitlines()[-1]; NULL when there is no line (Python IndexError). */
static char *sf_last_line(const char *output) {
    char *copy = sf_dup(output ? output : "");
    char *last, *scan;
    if (!copy) return NULL;
    sf_strip(copy);
    if (!copy[0]) { free(copy); return NULL; }
    last = copy;
    for (scan = copy; *scan; scan++) {
        if (*scan == '\n' || *scan == '\r') {
            char *next = scan + 1;
            while (*next == '\n' || *next == '\r') next++;
            *scan = '\0';
            if (*next) last = next;
            scan = next - 1;
            if (!*scan) break;
        }
    }
    if (!last[0]) { free(copy); return NULL; }
    {
        char *out = sf_dup(last);
        free(copy);
        return out;
    }
}

static json_t *sf_raised(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string(type));
    json_set(out, "message", json_string(message));
    return out;
}

/* --- the command ----------------------------------------------------------- */

json_t *w20_setupfiles_7eb3bd_run(const json_t *req) {
    const char *raw_text = json_get_str(req, "raw_text", "");
    const char *thread_id = json_get_str(req, "thread_id", NULL);
    const char *token_path = json_get_str(req, "token_path", "");
    json_t *email = json_obj_get(req, "sender_email");
    json_t *helper = json_obj_get(req, "helper");
    const char *mode = json_get_str(helper, "mode", "ok");
    const char *out_text = json_get_str(helper, "output", "");
    const char *err_text = json_get_str(helper, "error", "");
    char *sender_email = (email && email->type == JSON_STRING) ? email->str_val : NULL;
    char *sender_key = w20_setupfiles_7eb3bd_sender_key(sender_email);
    char *arg = w20_setupfiles_7eb3bd_split_arg(raw_text);
    int client_secret = json_get_bool(req, "client_secret_exists", 0);
    sf_state st;
    json_t *value, *creds, *by_email, *result;

    if (!arg) return sf_raised("MemoryError", "");
    memset(&st, 0, sizeof(st));
    st.replies = json_array();
    st.sender_key = sender_key;

    if (!arg[0]) {
        int creds_loaded = 0;
        if (json_get_bool(req, "token_exists", 0)) creds_loaded = json_get_bool(req, "creds_loaded", 0);
        if (creds_loaded) {
            json_t *kw = json_object();
            json_t *who = (sender_key && sender_key[0]) ? json_string(sender_key)
                                                        : sf_marker_kw("who_shared");
            json_set(kw, "who", who);
            json_set(kw, "token_path", json_string(token_path));
            sf_reply(&st, sf_marker("active", kw), thread_id);
        } else if (!client_secret) {
            sf_reply(&st, sf_marker_kw("not_configured"), thread_id);
        } else {
            sf_reply(&st, sf_marker_kw("not_authorized_yet"), thread_id);
        }
    } else if (!strcmp(arg, "start")) {
        if (!client_secret) {
            sf_reply(&st, sf_marker_kw("no_client_credentials"), thread_id);
        } else if (!strcmp(mode, "error")) {
            json_t *kw = json_object();
            json_set(kw, "error", json_string(err_text));
            sf_reply(&st, sf_marker("helper_error", kw), thread_id);
        } else if (!strcmp(mode, "exit")) {
            sf_reply(&st, sf_marker_kw("start_failed"), thread_id);
        } else {
            char *last = sf_last_line(out_text);
            if (!last) {
                /* `output.strip().splitlines()[-1]` on empty output */
                json_free(st.replies);
                free(arg);
                free(sender_key);
                return sf_raised("IndexError", "list index out of range");
            }
            {
                json_t *kw = json_object();
                json_set(kw, "auth_url", json_string(last));
                sf_reply(&st, sf_marker("start_instructions", kw), thread_id);
            }
            free(last);
        }
    } else if (!strcmp(arg, "revoke")) {
        json_t *output_value = NULL;
        if (!strcmp(mode, "error")) {
            json_t *kw = json_object();
            json_set(kw, "error", json_string(err_text));
            sf_reply(&st, sf_marker("revoke_error", kw), thread_id);
        } else if (!strcmp(mode, "exit")) {
            output_value = sf_marker_kw("revoke_completed");
        } else {
            char *trimmed = sf_dup(out_text);
            if (trimmed) sf_strip(trimmed);
            if (trimmed && trimmed[0]) output_value = json_string(trimmed);
            else output_value = sf_marker_kw("revoked");
            free(trimmed);
        }
        if (output_value) {
            json_t *kw = json_object();
            json_set(kw, "output", output_value);
            sf_set_creds(&st, 0, 0);
            sf_reply(&st, sf_marker("done_output", kw), thread_id);
        }
    } else {
        /* anything else is the auth code or the pasted failed-redirect URL */
        if (!strcmp(mode, "error")) {
            json_t *kw = json_object();
            json_set(kw, "error", json_string(err_text));
            sf_reply(&st, sf_marker("helper_error", kw), thread_id);
        } else if (!strcmp(mode, "exit")) {
            sf_reply(&st, sf_marker_kw("exchange_failed"), thread_id);
        } else {
            int reload_creds = json_get_bool(req, "creds_loaded", 0);
            int build_ok = json_get_bool(req, "build_api_ok", 1);
            if (reload_creds && build_ok) {
                sf_set_creds(&st, 1, 1);
                sf_reply(&st, sf_marker_kw("authorized"), thread_id);
            } else {
                char *trimmed = sf_dup(out_text);
                json_t *kw = json_object();
                if (trimmed) sf_strip(trimmed);
                json_set(kw, "token_path", json_string(token_path));
                json_set(kw, "output", json_string(trimmed ? trimmed : ""));
                sf_reply(&st, sf_marker("exchanged_not_loaded", kw), thread_id);
                free(trimmed);
            }
        }
    }

    by_email = json_array();
    if (st.by_email && sender_key) json_append(by_email, json_string(sender_key));
    creds = json_object();
    json_set(creds, "shared", json_bool(st.shared_set ? 1 : 0));
    json_set(creds, "api", json_bool(st.api_set ? 1 : 0));
    json_set(creds, "by_email", by_email);
    value = json_object();
    json_set(value, "consumed", json_bool(1));
    json_set(value, "replies", st.replies);
    json_set(value, "creds", creds);
    result = json_object();
    json_set(result, "status", json_string("ok"));
    json_set(result, "value", value);
    free(arg);
    free(sender_key);
    return result;
}
