/*
 * bot_mode_dm.c — Faithful C11 port of tools/bot_mode_dm.py.
 *
 * Bot Mode agent-to-agent DM tool — ``message_agent``.
 *
 * Containment contract (must hold): the schema is injected ONLY into a
 * canonical "Bot Chat" session on a Bot-Mode-managed install; dispatch is
 * title-gated again at execution time; everything here is additive. The
 * message rides a temp file — never inline shell text — and delivery runs
 * through the background terminal spawn with completion notification.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "hermes_json.h"
#include "bot_mode_probe.h"
#include "bot_mode_dm.h"

/* ── spawn seam ─────────────────────────────────────────────────────────── */
static dm_terminal_spawn_cb g_spawn;
static void *g_spawn_ctx;
void bot_mode_dm_set_spawn(dm_terminal_spawn_cb cb, void *ctx)
{
    g_spawn = cb;
    g_spawn_ctx = ctx;
}

void bot_mode_dm_free_names(char **names, size_t count)
{
    if (!names) return;
    for (size_t i = 0; i < count; i++) free(names[i]);
    free(names);
}

/* ── small string helpers ───────────────────────────────────────────────── */

static char *bmd_str(const char *s) { return s ? strdup(s) : strdup(""); }

static void strip_inplace(char *s)
{
    size_t n;
    if (!s) return;
    n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    size_t a = 0;
    while (s[a] && isspace((unsigned char)s[a])) a++;
    if (a) memmove(s, s + a, strlen(s + a) + 1);
}

static int str_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* _err: {"error": msg[, "teammates": [...]][, "peers": [...]]} malloc'd JSON */
static char *bmd_err(const char *message,
                     char **roster, size_t rn,
                     char **peers, size_t pn)
{
    json_t *o = json_object();
    if (!o) return strdup("{\"error\":\"alloc failed\"}");
    json_set(o, "error", json_string(message ? message : ""));
    if (roster) {
        json_t *arr = json_array();
        for (size_t i = 0; i < rn; i++) json_append(arr, json_string(roster[i]));
        json_set(o, "teammates", arr);
    }
    if (peers) {
        json_t *arr = json_array();
        for (size_t i = 0; i < pn; i++) json_append(arr, json_string(peers[i]));
        json_set(o, "peers", arr);
    }
    char *out = json_serialize(o);
    json_free(o);
    return out;
}

/* shlex.quote: leave shell-safe tokens bare; otherwise single-quote with the
 * classic '"'"' escape for embedded quotes. */
static char *shlex_quote(const char *s)
{
    if (!s) s = "";
    int safe = *s != '\0';
    for (const char *p = s; *p; p++) {
        if (!(('A' <= *p && *p <= 'Z') || ('a' <= *p && *p <= 'z') ||
              ('0' <= *p && *p <= '9') || strchr("@%+=:,./-_", *p))) {
            safe = 0;
            break;
        }
    }
    if (safe) return bmd_str(s);
    size_t cap = strlen(s) * 6 + 8, o = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[o++] = '\'';
    for (const char *p = s; *p; p++) {
        if (*p == '\'') {
            /* Python shlex: ' -> '"'"'  (close, double-quoted quote, reopen) */
            if (o + 6 >= cap) { cap *= 2; out = realloc(out, cap); if (!out) return NULL; }
            out[o++] = '\'';
            out[o++] = '"';
            out[o++] = '\'';
            out[o++] = '"';
            out[o++] = '\'';
        } else {
            if (o + 2 >= cap) { cap *= 2; out = realloc(out, cap); if (!out) return NULL; }
            out[o++] = *p;
        }
    }
    if (o + 2 >= cap) { cap += 3; out = realloc(out, cap); if (!out) return NULL; }
    out[o++] = '\'';
    out[o] = '\0';
    return out;
}

/* ── schema ─────────────────────────────────────────────────────────────── */

/* PoP: message_agent_tool_schema @ tools/bot_mode_dm.py:message_agent_tool_schema */
json_t *message_agent_tool_schema(void)
{
    json_t *root = json_object();
    json_t *fn = json_object();
    json_t *params = json_object();
    json_t *props = json_object();
    json_t *target_p = json_object();
    json_t *message_p = json_object();
    json_t *required = json_array();
    if (!root || !fn || !params || !props || !target_p || !message_p || !required)
        return NULL;

    json_set(root, "type", json_string("function"));

    json_set(fn, "name", json_string(MESSAGE_AGENT_TOOL_NAME));
    json_set(fn, "description", json_string(
        "Send a message to ANOTHER agent (teammate) on this install, or to an "
        "agent on a registered peer gateway. This is FIRE-AND-FORGET and "
        "asynchronous, like texting: it validates the target against the live "
        "roster, delivers your message into that agent's own Bot Chat with your "
        "attribution automatically prefixed, and returns immediately with a "
        "delivery acknowledgement. It does NOT return their reply and you must "
        "not wait or poll for one — send it, finish your turn, and the reply "
        "arrives later as a background-process completion notification that "
        "wakes you. COMPOSE the message yourself: write what YOU want to say to "
        "that agent (lead with the point; include the concrete ask or result). "
        "Never paste the user's words verbatim — paraphrase the actionable "
        "substance, and keep private 1:1 chat content private. Message one "
        "clearly relevant teammate when it genuinely helps the user's goal; "
        "don't fan out to several agents unless the user explicitly asked. "
        "Use the teammate roster in your system prompt (names + roles) to pick "
        "the right recipient; targets: a teammate name (e.g. 'researcher'), or "
        "'<peer>/<agent>' for an agent on a registered peer gateway "
        "(e.g. 'spark/researcher', or just '<peer>' for the peer's main agent)."));

    json_set(params, "type", json_string("object"));
    json_set(target_p, "type", json_string("string"));
    json_set(target_p, "description", json_string(
        "Who to message: a teammate profile name from your roster "
        "('researcher', 'hermes' for the default agent), or "
        "'<peer>' / '<peer>/<agent>' for a registered peer gateway."));
    char mdesc[512];
    snprintf(mdesc, sizeof mdesc,
             "The message YOU composed for that agent (max %d chars). Do not include the "
             "'Message from …' prefix — it is added automatically.", MESSAGE_MAX_CHARS);
    json_set(message_p, "type", json_string("string"));
    json_set(message_p, "description", json_string(mdesc));
    json_set(props, "target", target_p);
    json_set(props, "message", message_p);
    json_append(required, json_string("target"));
    json_append(required, json_string("message"));
    json_set(params, "properties", props);
    json_set(params, "required", required);
    json_set(fn, "parameters", params);
    json_set(root, "function", fn);
    return root;
}

/* PoP: ensure_message_agent_tool @ tools/bot_mode_dm.py:ensure_message_agent_tool */
int ensure_message_agent_tool(const bot_mode_dm_agent_vtab_t *agent)
{
    if (!agent) return 0;

    int enabled = agent->protocol_enabled ? agent->protocol_enabled(agent->ctx) : 1;
    if (!enabled)
        return 0;

    json_t *tools = agent->get_tools ? agent->get_tools(agent->ctx) : NULL;
    if (tools && tools->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(tools); i++) {
            json_t *tool = json_get(tools, i);
            if (tool && tool->type == JSON_OBJECT) {
                json_t *fno = json_obj_get(tool, "function");
                const char *name = fno ? json_get_str(fno, "name", "") : "";
                if (strcmp(name, MESSAGE_AGENT_TOOL_NAME) == 0)
                    return 1; /* already injected */
            }
        }
    }

    char *title = agent->session_title ? agent->session_title(agent->ctx) : NULL;
    if (title) strip_inplace(title);
    if (!title || strcmp(title, BOT_MODE_DM_BOT_CHAT_TITLE) != 0) {
        free(title);
        return 0;
    }
    free(title);

    char *home = agent->agent_home ? agent->agent_home(agent->ctx) : NULL;
    const char *section = home ? get_bot_mode_protocol_section(home, 0) : NULL;
    int managed = section && section[0];
    free(home);
    if (!managed)
        return 0;

    json_t *schema = message_agent_tool_schema();
    if (!schema)
        return 0;
    int ok = agent->append_tool && agent->append_tool(agent->ctx, schema);
    json_free(schema);
    if (!ok)
        return 0;
    if (agent->add_valid_tool_name)
        agent->add_valid_tool_name(agent->ctx, MESSAGE_AGENT_TOOL_NAME);
    return 1;
}

/* ── roster resolution ──────────────────────────────────────────────────── */

/* parent directory's name of `path` ("profiles" check in Python). */
static void path_parent_name(const char *path, char *out, size_t n)
{
    if (!path || !*path) { snprintf(out, n, ""); return; }
    const char *slash = strrchr(path, '/');
    if (!slash) { snprintf(out, n, ""); return; }   /* no parent */
    const char *start = slash;
    while (start > path && start[-1] != '/') start--;
    size_t len = (size_t)(slash - start);
    if (len >= n) len = n - 1;
    memcpy(out, start, len);
    out[len] = '\0';
}

static void base_name(const char *path, char *out, size_t n)
{
    const char *base = strrchr(path ? path : "", '/');
    base = base ? base + 1 : path;
    snprintf(out, n, "%s", base ? base : "");
}

/* PoP: _hermes_root @ tools/bot_mode_dm.py:_hermes_root */
char *bot_mode_dm_hermes_root(const char *home, char *out, size_t n)
{
    if (!home || !*home) { snprintf(out, n, "."); return out; }
    char pname[256];
    path_parent_name(home, pname, sizeof pname);
    if (strcmp(pname, "profiles") == 0) {
        /* home.parent.parent: strip two trailing components */
        char tmp[4096];
        snprintf(tmp, sizeof tmp, "%s", home);
        char *last = strrchr(tmp, '/');
        if (last) { *last = '\0'; char *prev = strrchr(tmp, '/'); if (prev && prev != tmp) *prev = '\0'; }
        snprintf(out, n, "%s", tmp);
    } else {
        snprintf(out, n, "%s", home);
    }
    return out;
}

/* PoP: _self_profile_name @ tools/bot_mode_dm.py:_self_profile_name */
char *bot_mode_dm_self_profile_name(const char *home, char *out, size_t n)
{
    char pname[256];
    path_parent_name(home ? home : "", pname, sizeof pname);
    if (strcmp(pname, "profiles") == 0)
        base_name(home ? home : "", out, n);
    else
        snprintf(out, n, "default");
    return out;
}

static int roster_cmp(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/*
 * PoP: _local_roster @ tools/bot_mode_dm.py:_local_roster
 */
char **bot_mode_dm_local_roster(const char *root, size_t *count)
{
    size_t cap = 8, nc = 1;
    char **names = malloc(sizeof(char *) * (cap + 1));
    if (!names) { *count = 0; return NULL; }
    names[0] = strdup("default");

    char pdir[4096];
    snprintf(pdir, sizeof pdir, "%s/profiles", root ? root : ".");
    DIR *d = opendir(pdir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.') continue;
            char full[4608];
            snprintf(full, sizeof full, "%s/%s", pdir, de->d_name);
            struct stat st;
            if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (nc == cap) {
                    cap *= 2;
                    names = realloc(names, sizeof(char *) * (cap + 1));
                }
                names[nc++] = strdup(de->d_name);
            }
        }
        closedir(d);
        /* sorted(iterdir): lexicographic over profile names only */
        qsort(names + 1, nc - 1, sizeof(char *), roster_cmp);
    }
    names[nc] = NULL;
    *count = nc;
    return names;
}

/* PoP: _handle @ tools/bot_mode_dm.py:_handle */
char *bot_mode_dm_handle(const char *name, char *out, size_t n)
{
    if (name && strcmp(name, "default") == 0)
        snprintf(out, n, "hermes");
    else
        snprintf(out, n, "%s", name ? name : "");
    return out;
}

/*
 * PoP: _resolve_local_name @ tools/bot_mode_dm.py:_resolve_local_name
 */
char *bot_mode_dm_resolve_local_name(const char *target,
                                     const char *const *roster, size_t n)
{
    if (!target) return NULL;
    char want[256];
    snprintf(want, sizeof want, "%s", target);
    strip_inplace(want);
    if (!want[0]) return NULL;
    if (str_ieq(want, "hermes")) {
        for (size_t i = 0; i < n; i++)
            if (strcmp(roster[i], "default") == 0)
                return strdup("default");
        return NULL;
    }
    for (size_t i = 0; i < n; i++)
        if (str_ieq(roster[i], want))
            return strdup(roster[i]);
    return NULL;
}

/*
 * PoP: _write_dm_file @ tools/bot_mode_dm.py:_write_dm_file
 */
char *bot_mode_dm_write_dm_file(const char *content)
{
    char tmpl[] = "/tmp/hermes-dm-XXXXXX.txt";
    int fd = mkstemps(tmpl, 4);
    if (fd < 0)
        return NULL;
    if (content && *content) {
        size_t len = strlen(content), off = 0;
        while (off < len) {
            ssize_t w = write(fd, content + off, len - off);
            if (w <= 0) break;
            off += (size_t)w;
        }
    }
    close(fd);
    return strdup(tmpl);
}

/* ── delivery ───────────────────────────────────────────────────────────── */

/*
 * PoP: _spawn_delivery @ tools/bot_mode_dm.py:_spawn_delivery
 */
static char *bmd_spawn_delivery(const char *command, const char *label,
                                const char *task_id)
{
    if (!g_spawn) {
        char buf[256];
        snprintf(buf, sizeof buf,
                 "Delivery to %s could not be started: no terminal backend.",
                 label);
        return bmd_err(buf, NULL, 0, NULL, 0);
    }
    char *err = NULL;
    char *raw = g_spawn(g_spawn_ctx, command, task_id, &err);
    if (!raw) {
        char buf[512];
        snprintf(buf, sizeof buf, "Delivery to %s could not be started: %s",
                 label, err ? err : "unknown error");
        free(err);
        return bmd_err(buf, NULL, 0, NULL, 0);
    }
    json_t *parsed = json_parse(raw, NULL);
    free(raw);

    const char *proc_id = "";
    if (parsed && parsed->type == JSON_OBJECT) {
        proc_id = json_get_str(parsed, "session_id", "");
        json_t *jerr = json_obj_get(parsed, "error");
        if (jerr && jerr->type == JSON_STRING && jerr->str_val[0]) {
            char buf[768];
            snprintf(buf, sizeof buf, "Delivery to %s failed to start: %s",
                     label, jerr->str_val);
            json_free(parsed);
            return bmd_err(buf, NULL, 0, NULL, 0);
        }
    }

    json_t *res = json_object();
    if (!res) {
        if (parsed) json_free(parsed);
        return bmd_err("alloc failed", NULL, 0, NULL, 0);
    }
    char detail[1024];
    snprintf(detail, sizeof detail,
             "Message dispatched to %s. This is asynchronous — do NOT wait "
             "or poll. Finish your turn now; when the delivery completes, its "
             "notification carries the reply — relay it then, attributed to "
             "that agent.", label);
    json_set(res, "status", json_string("sent"));
    json_set(res, "to", json_string(label));
    json_set(res, "detail", json_string(detail));
    if (proc_id[0])
        json_set(res, "process_id", json_string(proc_id));
    json_set(res, "sent_at", json_number((double)time(NULL)));
    if (parsed) json_free(parsed);
    char *out = json_serialize(res);
    json_free(res);
    return out;
}

/* ── the tool ───────────────────────────────────────────────────────────── */

/* PoP: message_agent_tool @ tools/bot_mode_dm.py:message_agent_tool */
char *message_agent_tool(const char *target, const char *message,
                         const char *task_id,
                         const bot_mode_dm_agent_vtab_t *agent)
{
    char *result = NULL;
    char **roster = NULL; size_t rn = 0;
    char **teammates = NULL; size_t tn = 0;
    char **peers = NULL; size_t pn = 0;
    char *body = NULL;
    char *payload = NULL;
    char *dm_file = NULL;
    char root[4096] = "";

    /* ── defense-in-depth gate: only a canonical Bot Chat may deliver ── */
    char *home = (agent && agent->agent_home) ? agent->agent_home(agent->ctx)
                                              : bmd_str("");
    char *title = (agent && agent->session_title) ? agent->session_title(agent->ctx)
                                                  : NULL;
    if (title) strip_inplace(title);
    if (!title || strcmp(title, BOT_MODE_DM_BOT_CHAT_TITLE) != 0) {
        result = bmd_err(
            "message_agent is only available in a Bot Mode 'Bot Chat' session. "
            "This session is not one; do not retry.", NULL, 0, NULL, 0);
        goto done;
    }

    {
        const char *section = get_bot_mode_protocol_section(home, 0);
        if (!section || !section[0]) {
            result = bmd_err(
                "This install is not Bot-Mode-managed (no bot roster); "
                "message_agent is unavailable. Do not retry.", NULL, 0, NULL, 0);
            goto done;
        }
    }

    /* ── roster ── */
    bot_mode_dm_hermes_root(home, root, sizeof root);
    char me_buf[128];
    bot_mode_dm_self_profile_name(home, me_buf, sizeof me_buf);
    const char *me = me_buf;

    roster = bot_mode_dm_local_roster(root, &rn);
    peers = bot_mode_probe_peers(root, &pn);
    teammates = malloc(sizeof(char *) * (rn + 1));
    if (!roster || !teammates) { result = bmd_err("alloc failed", NULL,0,NULL,0); goto done; }
    for (size_t i = 0; i < rn; i++) {
        if (strcmp(roster[i], me) != 0) {
            char h[128];
            teammates[tn++] = strdup(bot_mode_dm_handle(roster[i], h, sizeof h));
        }
    }

    /* ── body validation ── */
    body = bmd_str(message);
    strip_inplace(body);
    if (!body[0]) {
        result = bmd_err(
            "message is required — compose what you want to say to that agent.",
            NULL, 0, NULL, 0);
        goto done;
    }
    if (strlen(body) > MESSAGE_MAX_CHARS) {
        char buf[256];
        snprintf(buf, sizeof buf,
                 "message too long (%zu chars > %d). Send the essentials; "
                 "share large content as a file path instead.",
                 strlen(body), MESSAGE_MAX_CHARS);
        result = bmd_err(buf, NULL, 0, NULL, 0);
        goto done;
    }

    {
        /* raw_target: stripped, leading '@' removed */
        char rtbuf[1024];
        snprintf(rtbuf, sizeof rtbuf, "%s", target ? target : "");
        strip_inplace(rtbuf);
        const char *rt = rtbuf;
        while (*rt == '@') rt++;

        if (!rt[0]) {
            result = bmd_err("target is required.", teammates, tn, peers, pn);
            goto done;
        }

        char sender_handle[128];
        bot_mode_dm_handle(me, sender_handle, sizeof sender_handle);
        size_t plen = strlen("Message from 🤖 ") + strlen(sender_handle) * 2 +
                      strlen(" (@): ") + strlen(body) + 8;
        payload = malloc(plen);
        if (!payload) { result = bmd_err("alloc failed", NULL,0,NULL,0); goto done; }
        snprintf(payload, plen, "Message from 🤖 %s (@%s): %s",
                 sender_handle, sender_handle, body);

        /* ── peer target: '<peer>/<agent>' or a bare registered peer name ── */
        char peer_name[65] = "", peer_profile[65] = "";
        {
            const char *slash = strchr(rt, '/');
            if (slash && slash != rt && slash[1]) {
                size_t ln = (size_t)(slash - rt);
                int ok = ln >= 1 && ln <= 64;
                for (size_t i = 0; ok && i < ln; i++) {
                    char c = rt[i];
                    if (!(islower((unsigned char)c) || isdigit((unsigned char)c) ||
                          c == '_' || c == '-')) ok = 0;
                    if (i == 0 && !islower((unsigned char)c) &&
                        !isdigit((unsigned char)c)) ok = 0;
                }
                const char *prof = slash + 1;
                size_t ppl = strlen(prof);
                int pok = ppl >= 1 && ppl <= 64;
                for (size_t i = 0; pok && i < ppl; i++) {
                    char c = prof[i];
                    if (!(isalnum((unsigned char)c) || c == '_' || c == '-')) pok = 0;
                    if (i == 0 && !isalnum((unsigned char)c)) pok = 0;
                }
                if (ok && pok) {
                    memcpy(peer_name, rt, ln); peer_name[ln] = '\0';
                    strcpy(peer_profile, prof);
                }
            }
        }

        char lower_rt[1024];
        snprintf(lower_rt, sizeof lower_rt, "%s", rt);
        for (char *q = lower_rt; *q; q++) *q = (char)tolower((unsigned char)*q);
        int bare_peer = 0;
        for (size_t i = 0; i < pn; i++)
            if (strcmp(peers[i], lower_rt) == 0) { bare_peer = 1; break; }

        if (peer_name[0] || bare_peer) {
            const char *pname_ = peer_name[0] ? peer_name : lower_rt;
            int found = 0;
            for (size_t i = 0; i < pn; i++)
                if (strcmp(peers[i], pname_) == 0) { found = 1; break; }
            if (!found) {
                char buf[256];
                snprintf(buf, sizeof buf, "No registered peer named '%s'.", pname_);
                result = bmd_err(buf, teammates, tn, peers, pn);
                goto done;
            }
            char dm_target[160];
            if (peer_profile[0])
                snprintf(dm_target, sizeof dm_target, "%s/%s", pname_, peer_profile);
            else
                snprintf(dm_target, sizeof dm_target, "%s", pname_);
            dm_file = bot_mode_dm_write_dm_file(payload);
            char *qt = shlex_quote(dm_target);
            char *qf = shlex_quote(dm_file ? dm_file : "");
            size_t clen = strlen("hermes peer dm  < ") + strlen(qt) + strlen(qf) + 2;
            char *command = malloc(clen);
            if (!command || !qt || !qf) {
                free(command); free(qt); free(qf);
                result = bmd_err("alloc failed", NULL,0,NULL,0);
                goto done;
            }
            snprintf(command, clen, "hermes peer dm %s < %s", qt, qf);
            free(qt); free(qf);
            char label[256];
            snprintf(label, sizeof label, "@%s on peer '%s'",
                     peer_profile[0] ? peer_profile : pname_, pname_);
            result = bmd_spawn_delivery(command, label, task_id);
            free(command);
            goto done;
        }

        /* ── local teammate ── */
        /* _LOCAL_TARGET_RE: ^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$ */
        size_t L = strlen(rt);
        int valid = L >= 1 && L <= 64;
        for (size_t i = 0; valid && i < L; i++) {
            char c = rt[i];
            if (!(isalnum((unsigned char)c) || c == '_' || c == '-')) valid = 0;
            if (i == 0 && !isalnum((unsigned char)c)) valid = 0;
        }
        if (!valid) {
            char buf[1184];
            snprintf(buf, sizeof buf, "Invalid target: '%s'.", rt);
            result = bmd_err(buf, teammates, tn, peers, pn);
            goto done;
        }

        char *resolved = bot_mode_dm_resolve_local_name(rt,
                            (const char *const *)roster, rn);
        if (!resolved) {
            char buf[512];
            snprintf(buf, sizeof buf,
                     "No teammate named '%s' on this install. Pick a name from "
                     "the roster (roles are listed in your system prompt).", rt);
            result = bmd_err(buf, teammates, tn, peers, pn);
            goto done_free_resolved;
        }
done_free_resolved:
        if (!resolved) goto done;
        if (strcmp(resolved, me) == 0) {
            free(resolved);
            result = bmd_err(
                "You can't message yourself. Pick a teammate from the roster.",
                NULL, 0, NULL, 0);
            goto done;
        }

        dm_file = bot_mode_dm_write_dm_file(payload);
        char *qp = shlex_quote(resolved);
        char *qf = shlex_quote(dm_file ? dm_file : "");
        size_t clen = strlen("hermes -p  chat --in ~ -c \"Bot Chat\" "
                             "--create-if-missing -Q --query-file ")
                      + strlen(qp) + strlen(qf) + 2;
        char *command = malloc(clen);
        if (!command || !qp || !qf) {
            free(command); free(qp); free(qf); free(resolved);
            result = bmd_err("alloc failed", NULL,0,NULL,0);
            goto done;
        }
        snprintf(command, clen,
                 "hermes -p %s chat --in ~ -c \"Bot Chat\" --create-if-missing "
                 "-Q --query-file %s", qp, qf);
        free(qp); free(qf);
        char lhandle[128], label[192];
        bot_mode_dm_handle(resolved, lhandle, sizeof lhandle);
        free(resolved);
        snprintf(label, sizeof label, "@%s", lhandle);
        result = bmd_spawn_delivery(command, label, task_id);
        free(command);
    }

done:
    free(home);
    free(title);
    bot_mode_dm_free_names(roster, rn);
    bot_mode_probe_peers_free(peers, pn);
    bot_mode_dm_free_names(teammates, tn);
    free(body);
    free(payload);
    free(dm_file);
    return result;
}
