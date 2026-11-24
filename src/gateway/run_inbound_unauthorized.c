/*
 * run_inbound_unauthorized.c -- real C11 port of the copy and owner-side
 * signalling of upstream Hermes gateway/run_inbound_unauthorized.py.
 *
 * PoP: w21_runinbound_92c923_profile_arg @ gateway/run_inbound_unauthorized.py:pairing_profile_arg
 * PoP: w21_runinbound_92c923_code_reply @ gateway/run_inbound_unauthorized.py:pairing_code_reply
 * PoP: w21_runinbound_92c923_rate_limited @ gateway/run_inbound_unauthorized.py:pairing_rate_limited_reply
 * PoP: w21_runinbound_92c923_owner_hint @ gateway/run_inbound_unauthorized.py:unauthorized_owner_hint
 * PoP: w21_runinbound_92c923_first_time @ gateway/run_inbound_unauthorized.py:UnauthorizedOwnerNotifier.first_time
 * PoP: w21_runinbound_92c923_allowlist_env @ gateway/pairing.py:_allowlist_env_for_platform
 *
 * The English catalogue is rendered inline (keys named at each constant); the
 * home-channel post (`notify`) stays upstream -- it drives live transports.
 */
#include "hermes_json.h"
#include "noc_w21_runinbound_92c923.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- locale copy --------------------------------------------------------- */
/* gateway.pairing.validity_hour / validity_hours */
static const char RI_VALIDITY_ONE[] = "%ld hour";
static const char RI_VALIDITY_MANY[] = "%ld hours";
/* gateway.pairing.code_reply */
static const char RI_CODE_REPLY[] =
    "Hi! I don't recognize you yet, so I can't reply until the person running this bot approves you."
    "\n\nYour pairing code: `%s` (valid for %s)"
    "\n\nIf you run this bot, open a terminal and run: `%s`. Otherwise send that command to the bot owner."
    " After approval, send your message again.";
/* gateway.pairing.rate_limited */
static const char RI_RATE_LIMITED[] =
    "Too many pairing requests right now. Wait a few minutes, then send your message again.";
/* gateway.unauthorized.owner_hint_env */
static const char RI_HINT_ENV[] = "add the ID to %s in %s/.env and restart the gateway";
/* gateway.unauthorized.owner_hint_generic */
static const char RI_HINT_GENERIC[] =
    "add the ID to this platform's allowed-users list and restart the gateway";
/* gateway.unauthorized.owner_notice */
static const char RI_OWNER_NOTICE[] =
    "Dropped a message from unrecognized %s user %s. If that is you or someone you trust, %s;"
    " or set `unauthorized_dm_behavior: pair` for %s in %s/config.yaml so unknown senders receive"
    " a pairing code you can approve with `hermes pairing approve %s <code>`.";

/* gateway/pairing.py: CODE_TTL_SECONDS */
static const long RI_CODE_TTL_SECONDS = 3600;
/* run_inbound_unauthorized._MAX_SENDER_NAME_CHARS */
static const long RI_MAX_SENDER_NAME_CHARS = 64;

/* --- tiny helpers -------------------------------------------------------- */

static char *ri_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* malloc'd concatenation of four parts (NULL reads as ""). */
static char *ri_cat4(const char *a, const char *b, const char *c, const char *d) {
    size_t n = strlen(a ? a : "") + strlen(b ? b : "") + strlen(c ? c : "") + strlen(d ? d : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    out[0] = '\0';
    if (a) strcat(out, a);
    if (b) strcat(out, b);
    if (c) strcat(out, c);
    if (d) strcat(out, d);
    return out;
}

static int ri_py_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d)) return 0;
    if (floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* str(value): the scalar spellings a display name / id can arrive as. */
static char *ri_str_of(const json_t *v) {
    char buf[64];
    if (!v) return ri_dup("None");
    switch (v->type) {
    case JSON_NULL:   return ri_dup("None");
    case JSON_BOOL:   return ri_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (ri_py_int(v)) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return ri_dup(buf);
    case JSON_STRING: return ri_dup(v->str_val ? v->str_val : "");
    case JSON_ARRAY:  return ri_dup("[]");
    case JSON_OBJECT: return ri_dup("{}");
    }
    return ri_dup("");
}

/* `(platform or "").lower().strip()` */
static char *ri_lower_strip(const char *s) {
    size_t i, n;
    char *out;
    if (!s) return ri_dup("");
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        char c = s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[n] = '\0';
    i = 0;
    while (out[i] == ' ' || out[i] == '\t' || out[i] == '\n' || out[i] == '\r' ||
           out[i] == '\f' || out[i] == '\v') i++;
    if (i) memmove(out, out + i, strlen(out + i) + 1);
    n = strlen(out);
    while (n && (out[n - 1] == ' ' || out[n - 1] == '\t' || out[n - 1] == '\n' ||
                 out[n - 1] == '\r' || out[n - 1] == '\f' || out[n - 1] == '\v')) out[--n] = '\0';
    return out;
}

/* --- allowlist env table ------------------------------------------------- */

typedef struct {
    const char *platform;
    const char *env_var;
} ri_env_row;

static const ri_env_row RI_ENV_TABLE[] = {
    {"a2a",            "A2A_ALLOWED_USERS"},
    {"bluebubbles",    "BLUEBUBBLES_ALLOWED_USERS"},
    {"buzz",           "BUZZ_ALLOWED_USERS"},
    {"dingtalk",       "DINGTALK_ALLOWED_USERS"},
    {"discord",        "DISCORD_ALLOWED_USERS"},
    {"email",          "EMAIL_ALLOWED_USERS"},
    {"feishu",         "FEISHU_ALLOWED_USERS"},
    {"google_chat",    "GOOGLE_CHAT_ALLOWED_USERS"},
    {"irc",            "IRC_ALLOWED_USERS"},
    {"line",           "LINE_ALLOWED_USERS"},
    {"matrix",         "MATRIX_ALLOWED_USERS"},
    {"mattermost",     "MATTERMOST_ALLOWED_USERS"},
    {"ntfy",           "NTFY_ALLOWED_USERS"},
    {"photon",         "PHOTON_ALLOWED_USERS"},
    {"qqbot",          "QQ_ALLOWED_USERS"},
    {"signal",         "SIGNAL_ALLOWED_USERS"},
    {"simplex",        "SIMPLEX_ALLOWED_USERS"},
    {"slack",          "SLACK_ALLOWED_USERS"},
    {"sms",            "SMS_ALLOWED_USERS"},
    {"teams",          "TEAMS_ALLOWED_USERS"},
    {"telegram",       "TELEGRAM_ALLOWED_USERS"},
    {"wecom",          "WECOM_ALLOWED_USERS"},
    {"weixin",         "WEIXIN_ALLOWED_USERS"},
    {"whatsapp",       "WHATSAPP_ALLOWED_USERS"},
    {"whatsapp_cloud", "WHATSAPP_CLOUD_ALLOWED_USERS"},
    {NULL, NULL}
};

const char *w21_runinbound_92c923_allowlist_env(const char *platform_name) {
    char *key = ri_lower_strip(platform_name);
    int i;
    const char *found = NULL;
    if (!key) return NULL;
    for (i = 0; RI_ENV_TABLE[i].platform; i++) {
        if (!strcmp(key, RI_ENV_TABLE[i].platform)) { found = RI_ENV_TABLE[i].env_var; break; }
    }
    free(key);
    return found;
}

/* --- pairing copy -------------------------------------------------------- */

char *w21_runinbound_92c923_profile_arg(const json_t *profile) {
    if (!profile || profile->type != JSON_STRING) return ri_dup("");   /* isinstance(str) */
    if (!profile->str_val || !profile->str_val[0]) return ri_dup("");
    if (!strcmp(profile->str_val, "default")) return ri_dup("");
    return ri_cat4("-p ", profile->str_val, " ", NULL);
}

char *w21_runinbound_92c923_code_reply(const char *platform_name, const char *code,
                                       const char *profile_arg) {
    long hours = RI_CODE_TTL_SECONDS / 3600;
    char validity[64];
    char *approve_cmd;
    char *out;
    size_t need;
    if (hours < 1) hours = 1;                       /* max(1, CODE_TTL_SECONDS // 3600) */
    snprintf(validity, sizeof(validity), hours == 1 ? RI_VALIDITY_ONE : RI_VALIDITY_MANY, hours);
    /* f"hermes {profile_arg}pairing approve {platform_name} {code}" */
    approve_cmd = ri_cat4("hermes ", profile_arg ? profile_arg : "", "pairing approve ", NULL);
    {
        char *tail = ri_cat4(platform_name ? platform_name : "", " ", code ? code : "", NULL);
        char *joined = ri_cat4(approve_cmd, tail, NULL, NULL);
        free(approve_cmd);
        free(tail);
        approve_cmd = joined;
    }
    need = strlen(RI_CODE_REPLY) + strlen(code ? code : "") + strlen(validity) +
           strlen(approve_cmd) + 16;
    out = (char *)malloc(need);
    if (out)
        snprintf(out, need, RI_CODE_REPLY, code ? code : "", validity, approve_cmd);
    free(approve_cmd);
    return out;
}

char *w21_runinbound_92c923_rate_limited(void) {
    return ri_dup(RI_RATE_LIMITED);
}

/* gateway/session.py:neutralize_untrusted_inline_text(value, max_chars=64) */
static char *ri_neutralize(const char *value, long max_chars) {
    char *stage;
    size_t i, n, w;
    n = strlen(value);
    stage = (char *)malloc(n + 1);
    if (!stage) return NULL;
    w = 0;
    for (i = 0; i < n; i++) {
        char c = value[i];
        if (c == '\r') {
            if (i + 1 < n && value[i + 1] == '\n') i++;
            stage[w++] = ' ';
            continue;
        }
        if (c == '\n') { stage[w++] = ' '; continue; }
        if ((unsigned char)c < 0x20 && c != '\t') { stage[w++] = ' '; continue; }
        stage[w++] = c;
    }
    stage[w] = '\0';
    /* " ".join(text.split()) */
    {
        char *collapsed = (char *)malloc(w + 1);
        size_t r = 0;
        int pending_space = 0;
        if (!collapsed) { free(stage); return NULL; }
        for (i = 0; i < w; i++) {
            char c = stage[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
                if (r) pending_space = 1;
                continue;
            }
            if (pending_space) { collapsed[r++] = ' '; pending_space = 0; }
            collapsed[r++] = c;
        }
        collapsed[r] = '\0';
        free(stage);
        stage = collapsed;
        w = r;
    }
    if (max_chars && (long)w > max_chars) {
        size_t keep = (size_t)(max_chars - 3);
        char *cut = (char *)malloc(keep + 4);
        if (cut) {
            memcpy(cut, stage, keep);
            memcpy(cut + keep, "...", 4);
            free(stage);
            stage = cut;
        }
    }
    return stage;
}

char *w21_runinbound_92c923_owner_hint(const char *platform_name, const char *user_id,
                                       const char *user_name, const char *hermes_home) {
    static const char SIGILS[] = "@<>[]()`*_~#";
    char *neutral = ri_neutralize(user_name ? user_name : "", RI_MAX_SENDER_NAME_CHARS);
    char *safe;
    size_t i, w;
    char *who;
    const char *env_var;
    char *allowlist;
    char *out;
    size_t need;
    const char *home = hermes_home ? hermes_home : "";

    if (!neutral) return NULL;
    safe = (char *)malloc(strlen(neutral) + 1);
    if (!safe) { free(neutral); return NULL; }
    w = 0;
    for (i = 0; neutral[i]; i++) {
        if (strchr(SIGILS, neutral[i])) continue;
        safe[w++] = neutral[i];
    }
    safe[w] = '\0';
    free(neutral);
    i = 0;
    while (safe[i] == ' ' || safe[i] == '\t' || safe[i] == '\n' || safe[i] == '\r') i++;
    if (i) memmove(safe, safe + i, strlen(safe + i) + 1);
    w = strlen(safe);
    while (w && (safe[w - 1] == ' ' || safe[w - 1] == '\t' || safe[w - 1] == '\n' ||
                 safe[w - 1] == '\r')) safe[--w] = '\0';

    if (safe[0]) {
        /* f"{safe_name} ({user_id})" */
        char *open = ri_cat4(safe, " (", user_id ? user_id : "", NULL);
        who = ri_cat4(open, ")", NULL, NULL);
        free(open);
    } else {
        who = ri_dup(user_id ? user_id : "");
    }
    free(safe);

    env_var = w21_runinbound_92c923_allowlist_env(platform_name);
    if (env_var) {
        need = strlen(RI_HINT_ENV) + strlen(env_var) + strlen(home) + 8;
        allowlist = (char *)malloc(need);
        if (allowlist) snprintf(allowlist, need, RI_HINT_ENV, env_var, home);
    } else {
        allowlist = ri_dup(RI_HINT_GENERIC);
    }

    need = strlen(RI_OWNER_NOTICE) + strlen(platform_name ? platform_name : "") * 3 +
           strlen(who) + strlen(allowlist ? allowlist : "") + strlen(home) + 64;
    out = (char *)malloc(need);
    if (out)
        snprintf(out, need, RI_OWNER_NOTICE, platform_name ? platform_name : "", who,
                 allowlist ? allowlist : "", platform_name ? platform_name : "", home,
                 platform_name ? platform_name : "");
    free(allowlist);
    free(who);
    return out;
}

/* --- first-drop memory --------------------------------------------------- */

static int ri_key_is(const json_t *pair, const char *platform_name, const char *user_id) {
    if (!pair || pair->type != JSON_ARRAY || pair->c.count < 2) return 0;
    if (pair->c.items[0] == NULL || pair->c.items[0]->type != JSON_STRING) return 0;
    if (pair->c.items[1] == NULL || pair->c.items[1]->type != JSON_STRING) return 0;
    return !strcmp(pair->c.items[0]->str_val, platform_name) &&
           !strcmp(pair->c.items[1]->str_val, user_id);
}

json_t *w21_runinbound_92c923_first_time(const json_t *state, const char *platform_name,
                                         const json_t *user_id, double max_seen) {
    long limit = (long)max_seen;
    char *uid = ri_str_of(user_id);
    json_t *order = json_array();
    json_t *out = json_object();
    size_t i, count;
    int first = 1;

    if (limit < 1) limit = 1;                       /* max(1, int(max_seen)) */
    if (!uid) uid = ri_dup("");

    count = (state && state->type == JSON_ARRAY) ? state->c.count : 0;
    for (i = 0; i < count; i++) {
        if (ri_key_is(state->c.items[i], platform_name ? platform_name : "", uid)) {
            /* hit: move_to_end, no eviction */
            size_t j;
            first = 0;
            for (j = 0; j < count; j++) {
                if (j == i) continue;
                json_append(order, json_copy(state->c.items[j]));
            }
            {
                json_t *moved = json_array();
                json_append(moved, json_string(platform_name ? platform_name : ""));
                json_append(moved, json_string(uid));
                json_append(order, moved);
            }
            break;
        }
    }
    if (first) {
        size_t keep_from = 0;
        json_t *key = json_array();
        json_append(key, json_string(platform_name ? platform_name : ""));
        json_append(key, json_string(uid));
        for (i = 0; i < count; i++) json_append(order, json_copy(state->c.items[i]));
        json_append(order, key);
        /* while len(seen) > max_seen: popitem(last=False) */
        {
            size_t total = json_len(order);
            if ((long)total > limit) keep_from = (size_t)((long)total - limit);
        }
        if (keep_from) {
            json_t *trimmed = json_array();
            for (i = keep_from; i < json_len(order); i++)
                json_append(trimmed, json_copy(json_get(order, i)));
            json_free(order);
            order = trimmed;
        }
    }
    json_set(out, "first", json_bool(first));
    json_set(out, "order", order);
    free(uid);
    return out;
}
