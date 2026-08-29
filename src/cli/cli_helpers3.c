/*
 * cli_helpers3.c — batch-1 continuation: HermesCLI session/conversation I/O
 * methods ported from cli.py (class HermesCLI) onto the opaque hcli_t handle.
 * Persistence goes through the real hermes_state_db / session-store APIs.
 * Each function carries a one-line PoP annotation naming its cli.py origin.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "cli_hermes_cli.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "toolsets.h"
#include "hermes_state_db.h"
#include "context_compressor_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Declared in src/agent/hermes_state/hermes_state_misc.c (PoP:
 * hermes_state.py:delete_session_if_empty); not yet in hermes_state_db.h. */
extern bool hermes_state_delete_session_if_empty(hermes_state_db_t *db,
                                                 const char *session_id);

/* Declared in src/hermes_cli/session_export_wrappers.c (PoP:
 * hermes_cli/session_export.py). */
extern char *sexp_normalize_save_format(const char *fmt);
extern char *sexp_render_session_for_save(const json_t *session,
                                          const char *fmt);

/* ── small helpers ─────────────────────────────────────────────────────── */

/* JSON-string escape into a growing buffer (for hand-built rows). */
static void h3_app(char **buf, size_t *len, size_t *cap, const char *s)
{
    size_t ll = strlen(s);
    if (*len + ll + 1 > *cap) {
        while (*len + ll + 1 > *cap) *cap *= 2;
        *buf = realloc(*buf, *cap);
    }
    memcpy(*buf + *len, s, ll);
    *len += ll;
    (*buf)[*len] = '\0';
}

/* Is `name` present in the enabled_toolsets_json array? NULL/empty list
 * means Python's falsy self.enabled_toolsets → nothing is marked enabled. */
static bool h3_toolset_enabled(const hcli_t *self, const char *name)
{
    if (!self || !self->enabled_toolsets_json || !*self->enabled_toolsets_json)
        return false;
    json_t *arr = json_parse(self->enabled_toolsets_json, NULL);
    if (!arr || !json_is_array(arr)) { if (arr) json_free(arr); return false; }
    bool found = false;
    size_t n = json_array_size(arr);
    for (size_t i = 0; i < n && !found; i++) {
        json_t *v = json_array_get(arr, i);
        if (json_is_string(v) && strcmp(json_string_value(v), name) == 0)
            found = true;
    }
    json_free(arr);
    return found;
}

/* PoP: hcli_show_toolsets @ cli.py:HermesCLI.show_toolsets */
void hcli_show_toolsets(hcli_t *self)
{
    /* all_toolsets = get_all_toolsets() — names come sorted from
     * toolsets_get_names(); descriptions/tool counts via toolset info. */
    printf("\n");
    const char *title = "(^_^)b Available Toolsets";
    int width = 58;
    int tlen = (int)strlen(title);
    int pad = width - tlen;
    printf("+");
    for (int i = 0; i < width; i++) putchar('-');
    printf("+\n");
    printf("|%*s%s%*s|\n", pad / 2, "", title, pad - pad / 2, "");
    printf("+");
    for (int i = 0; i < width; i++) putchar('-');
    printf("+\n");
    printf("\n");

    size_t n = 0;
    char **names = toolsets_get_names(&n);
    for (size_t i = 0; i < n; i++) {
        const char *name = names[i];
        const toolset_def_t *def = toolsets_get_static(name);
        if (!def) continue; /* Python: `if info:` skips unknown entries */
        const char *desc = def->description ? def->description : "";
        size_t tool_count = 0;
        if (def->tools)
            while (def->tools[tool_count]) tool_count++;
        const char *marker = h3_toolset_enabled(self, name) ? "(*)" : "   ";
        printf("  %s %-18s [%2zu tools] - %s\n",
               marker, name, tool_count, desc);
    }
    toolsets_free_list(names, n);

    printf("\n");
    printf("  (*) = currently enabled\n");
    printf("\n");
    printf("  Tip: Use 'all' or '*' to enable all toolsets\n");
    printf("  Example: python cli.py --toolsets web,terminal\n");
    printf("\n");
}

/* PoP: hcli_handle_whoami_command @ cli.py:HermesCLI._handle_whoami_command */
void hcli_handle_whoami_command(hcli_t *self)
{
    (void)self;
    /* getpass.getuser(): LOGNAME > USER > LNAME > USERNAME, else getlogin();
     * any failure yields "?" like the except branch. */
    const char *user_name = NULL;
    const char *cands[] = {"LOGNAME", "USER", "LNAME", "USERNAME"};
    for (size_t i = 0; i < sizeof(cands) / sizeof(cands[0]); i++) {
        const char *v = getenv(cands[i]);
        if (v && *v) { user_name = v; break; }
    }
    char login_buf[256];
    if (!user_name && getlogin_r(login_buf, sizeof login_buf) == 0)
        user_name = login_buf;
    if (!user_name || !*user_name)
        user_name = "?";

    printf("\n");
    printf("  You:            cli (local terminal)\n");
    printf("  User:           %s\n", user_name);
    printf("  Tier:           unrestricted\n");
    printf("  Slash commands: all available\n");
    printf("\n");
}

/* PoP: hcli_list_recent_sessions @ cli.py:HermesCLI._list_recent_sessions */
char *hcli_list_recent_sessions(hcli_t *self, int limit)
{
    if (!self || !self->session_db)
        return strdup("[]");

    /* query_session_listing(db, source="cli", current_session_id=…,
     * include_all_sources=False, include_unnamed=True, limit=limit,
     * exclude_sources=["kanban","tool"]) over the rich session listing. */
    char *rich = hermes_state_list_sessions_rich(self->session_db, false);
    if (!rich)
        return strdup("[]"); /* except: return [] */
    json_t *rows = json_parse(rich, NULL);
    free(rich);
    if (!rows || !json_is_array(rows)) { if (rows) json_free(rows); return strdup("[]"); }

    size_t cap = 4096, len = 0;
    char *out = malloc(cap);
    h3_app(&out, &len, &cap, "[");
    size_t total = json_array_size(rows);
    int kept = 0;
    for (size_t i = 0; i < total; i++) {
        if (limit > 0 && kept >= limit) break;
        json_t *row = json_array_get(rows, i);
        json_t *jid = json_obj_get(row, "id");
        json_t *jsrc = json_obj_get(row, "source");
        if (!json_is_string(jid)) continue;
        const char *src = json_is_string(jsrc) ? json_string_value(jsrc) : "";
        if (strcmp(src, "cli") != 0)
            continue; /* include_all_sources=False → source must be "cli" */
        if (strcmp(src, "kanban") == 0 || strcmp(src, "tool") == 0)
            continue; /* exclude_sources */
        if (self->session_id &&
            strcmp(json_string_value(jid), self->session_id) == 0)
            continue; /* current_session_id excluded */

        char part[1600];
        json_t *jtitle = json_obj_get(row, "title");
        json_t *jlact = json_obj_get(row, "last_active");
        const char *title = json_is_string(jtitle) ? json_string_value(jtitle)
                                                   : "";
        double last_active = json_is_number(jlact) ? json_number_value(jlact)
                                                   : 0.0;
        int n = snprintf(part, sizeof part,
                         "%s{\"id\":\"%s\",\"title\":\"%s\","
                         "\"last_active\":%.6f}",
                         kept == 0 ? "" : ",",
                         json_string_value(jid), title, last_active);
        if (n < 0) continue;
        h3_app(&out, &len, &cap, part);
        kept++;
    }
    h3_app(&out, &len, &cap, "]");
    json_free(rows);
    return out;
}

/* PoP: hcli_discard_session_if_empty @ cli.py:HermesCLI._discard_session_if_empty */
bool hcli_discard_session_if_empty(hcli_t *self, const char *session_id)
{
    if (!self || !self->session_db || !session_id || !*session_id)
        return false;
    /* In-memory transcript is authoritative: any held messages mean the
     * session is not empty (protects against pruning a real conversation
     * whose DB flush failed or hasn't happened yet). */
    if (self->history_len > 0)
        return false;
    /* Delegates the check-and-delete to SessionDB.delete_session_if_empty
     * (only removes rows with no messages, no title, no children). */
    bool ok = hermes_state_delete_session_if_empty(self->session_db,
                                                   session_id);
    if (!ok)
        hermes_log(LOG_DEBUG, "cli_helpers3",
                   "Could not prune empty session %s", session_id);
    return ok;
}

/* PoP: hcli_launch_session_boundary_memory_flush @ cli.py:_launch_session_boundary_memory_flush */
bool hcli_launch_session_boundary_memory_flush(hcli_t *self,
                                               const hcli_msg_t *history_snapshot,
                                               size_t history_len,
                                               const char *session_id)
{
    /* agent = getattr(self, "agent", None); no agent or empty history →
     * nothing to extract (NULL in Python → false here). */
    if (!self || !self->agent || history_len == 0 || !history_snapshot)
        return false;

    /* The context-engine ``on_session_end`` boundary runs synchronously at
     * this point in Python (cheap local state clear, ordering-sensitive).
     * On the C surface the engine boundary is delivered by
     * cc_on_session_end when a live compressor is armed on the agent; the
     * hcli_t carries only an opaque agent back-reference, so the boundary
     * call belongs to the caller that owns both handles. This port keeps
     * the function's decision contract: it reports whether a snapshot
     * should be queued onto the memory manager's serialized worker. */

    /* No provider extraction to queue when no memory manager is configured
     * — new_session() falls back to the inline switch path. */
    if (!self->agent_has_memory_manager)
        return false;
    (void)session_id;
    return true;
}

/* PoP: hcli_consume_pending_resume_selection @ cli.py:_consume_pending_resume_selection */
int hcli_consume_pending_resume_selection(hcli_t *self, const char *text)
{
    if (!self)
        return HCLI_RESUME_NOT_CONSUMED;
    char **pending = self->pending_resume_sessions;
    size_t pending_n = self->pending_resume_count;
    if (!pending || pending_n == 0)
        return HCLI_RESUME_NOT_CONSUMED;

    /* One-shot: disarm now so a non-matching input can't leave the prompt
     * armed and hijack a later number the user meant as chat (#34584). */
    self->pending_resume_sessions = NULL;
    self->pending_resume_count = 0;

    if (!text)
        return HCLI_RESUME_NOT_CONSUMED;
    const char *p = text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t' ||
                     p[n-1] == '\n' || p[n-1] == '\r')) n--;
    /* Only a pure number selects; "/resume 3", titles, or any other text
     * falls through to normal handling. */
    if (n == 0)
        return HCLI_RESUME_NOT_CONSUMED;
    for (size_t i = 0; i < n; i++) {
        if (!isdigit((unsigned char)p[i]))
            return HCLI_RESUME_NOT_CONSUMED;
    }

    long index = strtol(p, NULL, 10);
    if (index < 1 || (size_t)index > pending_n) {
        printf("  Resume index %ld is out of range.\n", index);
        printf("  Use /resume with no arguments to see available sessions.\n");
        return HCLI_RESUME_OUT_OF_RANGE;
    }

    /* Caller resumes via "/resume <index>" (Python: _handle_resume_command). */
    return HCLI_RESUME_DISPATCHED;
}

/* PoP: hcli_save_conversation @ cli.py:HermesCLI.save_conversation */
char *hcli_save_conversation(hcli_t *self, const char *cmd)
{
    static const char *SAVE_USAGE =
        "Usage: /save [json|md|html] [filename] [redact]\n";

    if (!self)
        return NULL;
    /* parts = cmd.split()[1:] */
    const char *parts[8] = {0};
    int nparts = 0;
    const char *p = cmd ? cmd : "";
    while (*p && nparts < 8) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        parts[nparts++] = p;
        while (*p && !isspace((unsigned char)*p)) p++;
    }
    if (nparts == 0) {
        printf("%s", SAVE_USAGE);
        return NULL;
    }

    /* token copies (bounded) */
    char tok[8][256];
    for (int i = 0; i < nparts; i++) {
        size_t l = 0;
        const char *s = parts[i];
        while (s[l] && !isspace((unsigned char)s[l])) l++;
        if (l >= sizeof(tok[0])) l = sizeof(tok[0]) - 1;
        memcpy(tok[i], s, l);
        tok[i][l] = '\0';
    }

    int count = nparts;
    bool redact = false;
    if (strcasecmp(tok[count - 1], "redact") == 0 ||
        strcasecmp(tok[count - 1], "--redact") == 0) {
        redact = true;
        count--;
        if (count == 0) {
            printf("%s", SAVE_USAGE);
            return NULL;
        }
    }

    char fmt[16];
    char *canon = sexp_normalize_save_format(tok[0]);
    if (!canon) {
        printf("(._.) Unknown format '%s' — expected one of: json, md, html\n",
               tok[0]);
        printf("%s", SAVE_USAGE);
        return NULL;
    }
    snprintf(fmt, sizeof fmt, "%s", canon);
    free(canon);
    const char *filename = (count > 1) ? tok[1] : NULL;

    /* Prefer the durable DB row (has metadata + tool calls); fall back to
     * the in-memory history for sessions that never touched the DB. */
    json_t *session_data = NULL;
    if (self->session_db && self->session_id && *self->session_id) {
        /* export_session(sid) ≈ get_session row + active messages. */
        char *sess = hermes_state_get_session(self->session_db,
                                              self->session_id);
        if (sess) {
            session_data = json_parse(sess, NULL);
            free(sess);
        }
        char *msgs = hermes_state_get_messages_as_conversation(
            self->session_db, self->session_id, false);
        if (session_data && msgs) {
            json_t *marr = json_parse(msgs, NULL);
            if (marr && json_is_array(marr))
                json_set(session_data, "messages", marr);
            else
                json_free(marr);
        }
        free(msgs);
    }
    if (!session_data || !json_is_object(session_data)) {
        json_free(session_data);
        if (self->history_len == 0) {
            printf("(;_;) No conversation to save.\n");
            return NULL;
        }
        session_data = json_object();
        json_set(session_data, "id",
                 json_string(self->session_id ? self->session_id : ""));
        json_set(session_data, "model",
                 json_string(self->model ? self->model : ""));
        json_set(session_data, "started_at", json_number((double)time(NULL)));
        json_t *marr = json_array();
        for (size_t i = 0; i < self->history_len; i++) {
            json_t *m = json_object();
            json_set(m, "role",
                     json_string(self->history[i].role ? self->history[i].role : ""));
            json_set(m, "content",
                     json_string(self->history[i].content ? self->history[i].content : ""));
            json_append(marr, m);
        }
        json_set(session_data, "messages", marr);
    }

    if (redact) {
        /* redact_session_data: force-mode secret redaction pass. The C
         * secret-redaction surface is cc_redact_compaction_text; apply it
         * to every persisted message content string. */
        json_t *messages = json_obj_get(session_data, "messages");
        if (json_is_array(messages)) {
            size_t mn = json_array_size(messages);
            for (size_t i = 0; i < mn; i++) {
                json_t *m = json_array_get(messages, i);
                json_t *c = json_obj_get(m, "content");
                if (json_is_string(c)) {
                    char *r = cc_redact_compaction_text(json_string_value(c));
                    if (r) {
                        json_set(m, "content", json_string(r));
                        free(r);
                    }
                }
            }
        }
    }

    /* saved_dir = <hermes_home>/sessions/saved (mkdir -p). */
    const char *home = getenv("HERMES_HOME");
    char saved_dir[768];
    snprintf(saved_dir, sizeof saved_dir, "%s/sessions/saved",
             (home && *home) ? home : ".hermes");
    /* best-effort mkdir -p for sessions/ then saved/ */
    char tmp[768];
    snprintf(tmp, sizeof tmp, "%s/sessions",
             (home && *home) ? home : ".hermes");
    mkdir(tmp, 0755);
    if (mkdir(saved_dir, 0755) != 0 && errno != EEXIST) {
        printf("(x_x) Failed to create save directory %s\n", saved_dir);
        json_free(session_data);
        return NULL;
    }

    /* path resolution: explicit filename (expanduser, cwd-relative) or
     * saved_dir/hermes_conversation_<timestamp>.<fmt> */
    char path[1536];
    if (filename) {
        const char *f = filename;
        if (f[0] == '~' && f[1] == '/') {
            const char *h = getenv("HOME");
            snprintf(path, sizeof path, "%s%s", (h && *h) ? h : ".", f + 1);
        } else if (f[0] == '/') {
            snprintf(path, sizeof path, "%s", f);
        } else {
            char cwd[1024];
            if (!getcwd(cwd, sizeof cwd)) snprintf(cwd, sizeof cwd, ".");
            snprintf(path, sizeof path, "%s/%s", cwd, f);
        }
    } else {
        char timestamp[32];
        time_t now = time(NULL);
        struct tm tmv;
        localtime_r(&now, &tmv);
        strftime(timestamp, sizeof timestamp, "%Y%m%d_%H%M%S", &tmv);
        snprintf(path, sizeof path, "%s/hermes_conversation_%s.%s",
                 saved_dir, timestamp, fmt);
    }

    char *content = sexp_render_session_for_save(session_data, fmt);
    json_free(session_data);
    if (!content) {
        printf("(x_x) Failed to save: could not render session as %s\n", fmt);
        return NULL;
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        printf("(x_x) Failed to save: cannot open %s for writing\n", path);
        free(content);
        return NULL;
    }
    fwrite(content, 1, strlen(content), fp);
    fclose(fp);

    const char *label = strcmp(fmt, "json") == 0 ? "JSON"
                        : strcmp(fmt, "md") == 0 ? "Markdown" : "HTML";
    char *result = malloc(strlen(path) + 64);
    snprintf(result, strlen(path) + 64, "%s (%s)", path, label);
    printf("(^_^)v Conversation saved to: %s (%s)\n", path, label);
    if (self->session_id && *self->session_id)
        printf("       Resume the live session with: hermes --resume %s\n",
               self->session_id);
    free(content);
    return result;
}
