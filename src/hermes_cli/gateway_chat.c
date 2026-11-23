/*
 * gateway_chat.c — faithful C11 port of upstream Hermes hermes_cli/gateway_chat.py.
 *
 * PoP: w9_gatewaychat_13d2b8_continue_title @ hermes_cli/gateway_chat.py:continue_title
 * PoP: w9_gatewaychat_13d2b8_wants_latest @ hermes_cli/gateway_chat.py:wants_latest
 * PoP: w9_gatewaychat_13d2b8_env_launch_flags @ hermes_cli/gateway_chat.py:env_launch_flags
 * PoP: w9_gatewaychat_13d2b8_bypass_launch @ hermes_cli/gateway_chat.py:bypass_launch
 * PoP: w9_gatewaychat_13d2b8_launch_flags @ hermes_cli/gateway_chat.py:_launch_flags
 * PoP: w9_gatewaychat_13d2b8_requested_policy @ hermes_cli/gateway_chat.py:_requested_policy
 * PoP: w9_gatewaychat_13d2b8_validate_options @ hermes_cli/gateway_chat.py:validate_options
 * PoP: w9_gatewaychat_13d2b8_resume_policy_error @ hermes_cli/gateway_chat.py:resume_policy_error
 * PoP: w9_gatewaychat_13d2b8_client_env_policy @ hermes_cli/gateway_chat.py:client_env_policy
 *
 * os.environ, the shell client-env resolver and session-source creation are
 * fixture seams; no C process environment is mutated.
 */
#define _GNU_SOURCE
#include "noc_w9_gatewaychat_13d2b8.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default:          return 0;
    }
}

static const char *strval(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static void py_str_into(char *buf, size_t cap, const json_t *v) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
        case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, cap, "%lld", (long long)v->num_val);
            else snprintf(buf, cap, "%g", v->num_val);
            return;
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
        default:          snprintf(buf, cap, ""); return;
    }
}

static void strip_lower(char *out, size_t cap, const char *s) {
    const char *p = s ? s : "";
    while (*p && isspace((unsigned char)*p)) p++;
    size_t n = 0;
    while (*p && n + 1 < cap) { out[n++] = (char)tolower((unsigned char)*p); p++; }
    while (n > 0 && isspace((unsigned char)out[n - 1])) n--;
    out[n] = '\0';
}

static int is_true_literal(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}

static const char *LAUNCH_FIELDS[5] = {
    "yolo", "ignore_rules", "safe_mode", "ignore_user_config", "accept_hooks"
};
static const char *LAUNCH_ENVS[5] = {
    "HERMES_YOLO_MODE", "HERMES_IGNORE_RULES", "HERMES_SAFE_MODE",
    "HERMES_IGNORE_USER_CONFIG", "HERMES_ACCEPT_HOOKS"
};

json_t *w9_gatewaychat_13d2b8_env_launch_flags(const json_t *env) {
    json_t *out = json_object();
    for (int i = 0; i < 5; i++) {
        json_t *v = json_obj_get(env, LAUNCH_ENVS[i]);
        char word[128];
        strip_lower(word, sizeof word, strval(v));
        int on = 0;
        if (v && v->type != JSON_NULL && v->type != JSON_STRING) on = py_truthy(v);
        else on = !strcmp(word, "1") || !strcmp(word, "true") ||
                  !strcmp(word, "yes") || !strcmp(word, "on");
        if (on) json_set(out, LAUNCH_FIELDS[i], json_bool(true));
    }
    return out;
}

json_t *w9_gatewaychat_13d2b8_continue_title(const json_t *args) {
    json_t *name = json_obj_get(args, "continue_last");
    if (name && name->type == JSON_STRING && !py_truthy(json_obj_get(args, "resume")))
        return json_copy(name);
    return json_null();
}

json_t *w9_gatewaychat_13d2b8_wants_latest(const json_t *args) {
    json_t *resume = json_obj_get(args, "resume");
    if (resume && resume->type == JSON_STRING) {
        char word[256];
        strip_lower(word, sizeof word, resume->str_val);
        return json_bool(!strcmp(word, "latest") ? true : false);
    }
    return json_bool(is_true_literal(json_obj_get(args, "continue_last")) ? true : false);
}

json_t *w9_gatewaychat_13d2b8_bypass_launch(const json_t *args, const json_t *env) {
    json_t *flags = w9_gatewaychat_13d2b8_env_launch_flags(env);
    int on = 0;
    const char *names[2] = {"safe_mode", "ignore_user_config"};
    for (int i = 0; i < 2; i++) {
        if (py_truthy(json_obj_get(args, names[i])) || py_truthy(json_obj_get(flags, names[i]))) {
            on = 1; break;
        }
    }
    json_free(flags);
    return json_bool(on ? true : false);
}

static const char *POLICY_KEYS[15] = {
    "model", "provider", "reasoning", "toolsets", "max_turns", "base_url", "ignore_rules",
    "api_key", "yolo", "safe_mode", "ignore_user_config", "skills", "checkpoints",
    "accept_hooks", "pass_session_id"
};

json_t *w9_gatewaychat_13d2b8_launch_flags(const json_t *args) {
    json_t *out = json_object();
    for (int i = 0; i < 15; i++) {
        json_t *v = json_obj_get(args, POLICY_KEYS[i]);
        if (!v || v->type == JSON_NULL) continue;
        if (v->type == JSON_BOOL && !v->bool_val) continue; /* 0 is kept; False is not */
        json_set(out, POLICY_KEYS[i], json_copy(v));
    }
    return out;
}

static void split_names(json_t *out, const char *s, int dedupe) {
    const char *p = s;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t n = comma ? (size_t)(comma - p) : strlen(p);
        char piece[512];
        snprintf(piece, sizeof piece, "%.*s", (int)n, p);
        char trimmed[512];
        strip_lower(trimmed, sizeof trimmed, ""); /* reset */
        const char *a = piece;
        while (*a && isspace((unsigned char)*a)) a++;
        const char *b = a + strlen(a);
        while (b > a && isspace((unsigned char)b[-1])) b--;
        size_t len = (size_t)(b - a);
        if (len) {
            char name[512];
            snprintf(name, sizeof name, "%.*s", (int)len, a);
            int seen = 0;
            if (dedupe) for (size_t k = 0; k < json_len(out); k++) {
                const char *old = strval(json_get(out, k));
                if (old && !strcmp(old, name)) { seen = 1; break; }
            }
            if (!seen) json_append(out, json_string(name));
        }
        if (!comma) break;
        p = comma + 1;
    }
}

json_t *w9_gatewaychat_13d2b8_requested_policy(const json_t *args, const json_t *env) {
    (void)env;
    json_t *policy = w9_gatewaychat_13d2b8_launch_flags(args);
    json_t *ts = json_obj_get(policy, "toolsets");
    if (ts && ts->type == JSON_STRING) {
        char tsbuf[512];
        snprintf(tsbuf, sizeof tsbuf, "%s", ts->str_val ? ts->str_val : "");
        json_obj_del(policy, "toolsets");
        json_t *arr = json_array();
        split_names(arr, tsbuf, 0);
        json_set(policy, "toolsets", arr);
    }
    json_t *skills = json_obj_get(policy, "skills");
    if (skills) {
        json_t *names = json_array();
        if (skills->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(skills); i++) {
                char s[512]; py_str_into(s, sizeof s, json_get(skills, i));
                split_names(names, s, 1);
            }
        } else {
            char s[512]; py_str_into(s, sizeof s, skills);
            split_names(names, s, 1);
        }
        json_obj_del(policy, "skills");
        if (json_len(names)) json_set(policy, "skills", names);
        else json_free(names);
    }
    json_t *source = json_obj_get(args, "source");
    if (py_truthy(source)) {
        const char *s = strval(source);
        json_set(policy, "source", json_string(s && !strcmp(s, "desktop") ? "gui" : (s ? s : "")));
    }
    return policy;
}

/* Build the exact GatewayClientError message for unsupported options. */
static json_t *unsupported_error(const json_t *args, const json_t *env) {
    static const char *UNSUPPORTED[7] = {
        "image", "worktree", "w", "no_restore_cwd", "run_budget", "verbose", "compact"
    };
    static const char *RELOCATED[8] = {
        "`hermes --tui` or the Desktop app to attach the image",
        "`hermes --tui -w`", "`hermes --tui -w`", "`--in <dir>` (the gateway keeps the session's frozen cwd)",
        "`agent.run_budget_seconds` in config.yaml", "`hermes logs --follow`, or `hermes chat --tui -v`",
        "`display.compact: true` in config.yaml",
        "`-c <name> --create-if-missing`"
    };
    char msg[8192];
    size_t pos = 0;
    int any = 0;
    int idx[8];
    int nidx = 0;
    for (int i = 0; i < 7; i++)
        if (py_truthy(json_obj_get(args, UNSUPPORTED[i]))) idx[nidx++] = i;
    json_t *title = w9_gatewaychat_13d2b8_continue_title(args);
    int no_title = (title->type == JSON_NULL);
    json_free(title);
    if (py_truthy(json_obj_get(args, "create_if_missing")) && no_title) idx[nidx++] = 7;
    if (nidx) {
        pos += (size_t)snprintf(msg + pos, sizeof msg - pos,
                                "Unsupported gateway CLI options: ");
        for (int k = 0; k < nidx; k++) {
            char flag[96];
            if (idx[k] == 7) snprintf(flag, sizeof flag, "--create-if-missing without -c <name>");
            else if (!strcmp(UNSUPPORTED[idx[k]], "w")) snprintf(flag, sizeof flag, "-w");
            else {
                char rep[64];
                const char *src = UNSUPPORTED[idx[k]];
                for (size_t j = 0, r = 0; src[j] && r + 1 < sizeof rep; j++)
                    rep[r++] = src[j] == '_' ? '-' : src[j], rep[r] = '\0';
                snprintf(flag, sizeof flag, "--%s", rep);
            }
            pos += (size_t)snprintf(msg + pos, sizeof msg - pos, "%s%s",
                                    k ? ", " : "", flag);
        }
        pos += (size_t)snprintf(msg + pos, sizeof msg - pos,
                                ". No local fallback or policy changes were made.");
        for (int k = 0; k < nidx; k++) {
            char flag[96];
            if (idx[k] == 7) snprintf(flag, sizeof flag, "--create-if-missing without -c <name>");
            else if (!strcmp(UNSUPPORTED[idx[k]], "w")) snprintf(flag, sizeof flag, "-w");
            else {
                char rep[64];
                const char *src = UNSUPPORTED[idx[k]];
                for (size_t j = 0, r = 0; src[j] && r + 1 < sizeof rep; j++)
                    rep[r++] = src[j] == '_' ? '-' : src[j], rep[r] = '\0';
                snprintf(flag, sizeof flag, "--%s", rep);
            }
            pos += (size_t)snprintf(msg + pos, sizeof msg - pos, "\n  %s: use %s",
                                    flag, RELOCATED[idx[k]]);
        }
        any = 1;
    }
    if (!any) {
        json_t *bypass = w9_gatewaychat_13d2b8_bypass_launch(args, env);
        int bp = (bypass->type == JSON_BOOL && bypass->bool_val);
        json_free(bypass);
        int resuming = py_truthy(json_obj_get(args, "resume")) ||
                       py_truthy(json_obj_get(args, "continue_last"));
        if (bp && !py_truthy(json_obj_get(args, "model")) && !resuming) {
            snprintf(msg, sizeof msg,
                     "--safe-mode / --ignore-user-config (or HERMES_SAFE_MODE / HERMES_IGNORE_USER_CONFIG) "
                     "read no profile default: pass --model explicitly.\n"
                     "  Example: hermes chat --safe-mode --provider openrouter --model anthropic/claude-sonnet-4 -q \"hello\"");
            any = 1;
        }
    }
    return any ? json_string(msg) : json_null();
}

json_t *w9_gatewaychat_13d2b8_validate_options(const json_t *args, const json_t *env) {
    return unsupported_error(args, env);
}

static int token_in(const char *list, const char *sep, const char *tok) {
    if (!tok || !*tok || !list || !*list) return 0;
    size_t sl = strlen(sep);
    const char *p = list;
    for (;;) {
        const char *q = strstr(p, sep);
        size_t n = q ? (size_t)(q - p) : strlen(p);
        if (n == strlen(tok) && !strncmp(p, tok, n)) return 1;
        if (!q) return 0;
        p = q + sl;
    }
}

json_t *w9_gatewaychat_13d2b8_resume_policy_error(const json_t *keys) {
    if (!keys || keys->type != JSON_ARRAY) return json_null();
    static const char *HINT_FLAG[9] = {
        "model", "provider", "base_url", "reasoning", "yolo", "toolsets", "skills", "verbose", "cwd"
    };
    static const char *HINT_SPELL[9] = {
        "-m/--model", "--provider", "--base-url", "--reasoning", "--yolo",
        "-t/--toolsets", "-s/--skills", "-v/--verbose", "--in"
    };
    static const char *HINT_CMD[9] = {
        "/model", "/model", "/model", "/reasoning", "/yolo", NULL, NULL, NULL, NULL
    };
    char flags[1024] = "", cmds[1024] = "";
    size_t n = json_len(keys);
    for (size_t i = 0; i < n; i++) {
        const char *k = strval(json_get(keys, i));
        const char *flag = NULL, *cmd = NULL;
        for (int j = 0; j < 9; j++)
            if (k && !strcmp(k, HINT_FLAG[j])) { flag = HINT_SPELL[j]; cmd = HINT_CMD[j]; break; }
        char fallback[128];
        if (!flag) {
            if (k) {
                size_t f = 0;
                snprintf(fallback, sizeof fallback, "--");
                f = strlen(fallback);
                for (const char *p = k; *p && f + 1 < sizeof fallback; p++)
                    fallback[f++] = (*p == '_') ? '-' : *p;
                fallback[f] = '\0';
            } else fallback[0] = '\0';
            flag = fallback;
        }
        if (!token_in(flags, ", ", flag)) {
            if (flags[0]) strcat(flags, ", ");
            strcat(flags, flag);
        }
        if (cmd && !token_in(cmds, " or ", cmd)) {
            if (cmds[0]) strcat(cmds, " or ");
            strcat(cmds, cmd);
        }
    }
    const char *them = (n == 1) ? "it" : "them";
    char msg[2400];
    if (cmds[0]) {
        snprintf(msg, sizeof msg,
                 "Resume retains gateway session policy; creation overrides are unsupported on resume. "
                 "%s %s from this session's launch: resume without %s and use %s after resuming to switch.",
                 flags, n == 1 ? "differs" : "differ", them, cmds);
    } else {
        snprintf(msg, sizeof msg,
                 "Resume retains gateway session policy; creation overrides are unsupported on resume. "
                 "%s %s from this session's launch: resume without %s, or start a new session to use %s.",
                 flags, n == 1 ? "differs" : "differ", them, them);
    }
    return json_string(msg);
}

json_t *w9_gatewaychat_13d2b8_client_env_policy(const json_t *parameters,
                                                const json_t *client_env) {
    json_t *out = json_object();
    if (parameters && parameters->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(parameters); i++) {
            const char *s = strval(json_get(parameters, i));
            if (s && !strcmp(s, "client_env")) {
                json_set(out, "client_env", json_copy(client_env));
                return out;
            }
        }
    }
    return out;
}

