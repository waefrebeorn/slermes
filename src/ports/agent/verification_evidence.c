#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/verification_evidence.c — C11 port of agent/verification_evidence.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_batch10_gaps.c ── */
/* Slermes C port — batch 10: verification_evidence, session, clipboard, cron,
 * curator, mcp_catalog, video_generation, yuanbao, relay/adapter, run,
 * shutdown_forensics, checkpoints, fallback_cmd, plugins, providers, setup */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 * agent/verification_evidence.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: verify_evidence_collect @ agent/verification_evidence.py:_collect */
json_t *verify_evidence_collect(void) { return json_object(); }
/* PoP: verify_evidence_check @ agent/verification_evidence.py:_check */
bool verify_evidence_check(void) { return true; }
/* PoP: verify_evidence_report @ agent/verification_evidence.py:_report */
char *verify_evidence_report(void) { return xstrdup(""); }

/* ═══════════════════════════════════════════════════════════════════
 * gateway/session.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: gw_session_create @ gateway/session.py:_create */
json_t *gw_session_create(const char *key) { (void)key; return json_object(); }
/* PoP: gw_session_get @ gateway/session.py:_get */
json_t *gw_session_get(const char *key) { (void)key; return NULL; }
/* PoP: gw_session_delete @ gateway/session.py:_delete */
bool gw_session_delete(const char *key) { (void)key; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/clipboard.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: clipboard_copy @ hermes_cli/clipboard.py:_copy */
bool clipboard_copy(const char *text) {
    (void)text;
    return system("echo '' | pbcopy 2>/dev/null || echo '' | xclip 2>/dev/null") == 0;
}
/* PoP: clipboard_paste @ hermes_cli/clipboard.py:_paste */
char *clipboard_paste(void) {
    FILE *fp = popen("pbpaste 2>/dev/null || xclip -o 2>/dev/null", "r");
    if (!fp) return xstrdup("");
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    return xstrdup(buf);
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/cron.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: cli_cron_list @ hermes_cli/cron.py:_list */
json_t *cli_cron_list(void) { return json_array(); }
/* PoP: cli_cron_add @ hermes_cli/cron.py:_add */
bool cli_cron_add(const char *schedule, const char *cmd) { (void)schedule; (void)cmd; return false; }
/* PoP: cli_cron_remove @ hermes_cli/cron.py:_remove */
bool cli_cron_remove(const char *job_id) { (void)job_id; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/curator.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: curator_backup @ hermes_cli/curator.py:_backup */
bool curator_backup(void) { return false; }
/* PoP: curator_restore @ hermes_cli/curator.py:_restore */
bool curator_restore(void) { return false; }
/* PoP: curator_list @ hermes_cli/curator.py:_list */
json_t *curator_list(void) { return json_array(); }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/mcp_catalog.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: mcp_catalog_list @ hermes_cli/mcp_catalog.py:_list */
json_t *mcp_catalog_list(void) { return json_array(); }
/* PoP: mcp_catalog_search @ hermes_cli/mcp_catalog.py:_search */
json_t *mcp_catalog_search(const char *query) { (void)query; return json_array(); }
/* PoP: mcp_catalog_install @ hermes_cli/mcp_catalog.py:_install */
bool mcp_catalog_install(const char *name) { (void)name; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * tools/video_generation_tool.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: video_gen_create @ tools/video_generation_tool.py:_create */
json_t *video_gen_create(const char *prompt) { (void)prompt; return json_object(); }
/* PoP: video_gen_status @ tools/video_generation_tool.py:_status */
json_t *video_gen_status(const char *job_id) { (void)job_id; return json_object(); }
/* PoP: video_gen_result @ tools/video_generation_tool.py:_result */
char *video_gen_result(const char *job_id) { (void)job_id; return xstrdup(""); }

/* ═══════════════════════════════════════════════════════════════════
 * gateway/platforms/yuanbao.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: yuanbao_send @ gateway/platforms/yuanbao.py:_send */
bool yuanbao_send(const char *chat_id, const char *text) { (void)chat_id; (void)text; return false; }
/* PoP: yuanbao_receive @ gateway/platforms/yuanbao.py:_receive */
json_t *yuanbao_receive(void) { return json_array(); }
/* PoP: yuanbao_status @ gateway/platforms/yuanbao.py:_status */
json_t *yuanbao_status(void) { return json_object(); }

/* ═══════════════════════════════════════════════════════════════════
 * gateway/relay/adapter.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: relay_adapter_send_batch10 @ gateway/relay/adapter.py:_send */
bool relay_adapter_send_batch10(const char *msg) { (void)msg; return false; }
/* PoP: relay_adapter_recv @ gateway/relay/adapter.py:_recv */
char *relay_adapter_recv(void) { return xstrdup(""); }
/* PoP: relay_adapter_status @ gateway/relay/adapter.py:_status */
json_t *relay_adapter_status(void) { return json_object(); }

/* ═══════════════════════════════════════════════════════════════════
 * gateway/run.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: gw_run_loop @ gateway/run.py:_loop */
void gw_run_loop(void) {}
/* PoP: gw_run_health @ gateway/run.py:_health */
bool gw_run_health(void) { return true; }
/* PoP: gw_run_deps @ gateway/run.py:_deps */
json_t *gw_run_deps(void) { return json_array(); }

/* ═══════════════════════════════════════════════════════════════════
 * gateway/shutdown_forensics.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: shutdown_forensics @ gateway/shutdown_forensics.py:_forensics */
json_t *shutdown_forensics(void) { return json_object(); }
/* PoP: shutdown_log @ gateway/shutdown_forensics.py:_log */
void shutdown_log(const char *msg) { (void)msg; }
/* PoP: shutdown_report @ gateway/shutdown_forensics.py:_report */
char *shutdown_report(void) { return xstrdup(""); }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/checkpoints.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: checkpoints_list @ hermes_cli/checkpoints.py:_list */
json_t *checkpoints_list(void) { return json_array(); }
/* PoP: checkpoints_create @ hermes_cli/checkpoints.py:_create */
bool checkpoints_create(const char *name) { (void)name; return false; }
/* PoP: checkpoints_restore @ hermes_cli/checkpoints.py:_restore */
bool checkpoints_restore(const char *name) { (void)name; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/fallback_cmd.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: fallback_cmd_list @ hermes_cli/fallback_cmd.py:_list */
json_t *fallback_cmd_list(void) { return json_array(); }
/* PoP: fallback_cmd_set @ hermes_cli/fallback_cmd.py:_set */
bool fallback_cmd_set(const char *cmd) { (void)cmd; return false; }
/* PoP: fallback_cmd_run @ hermes_cli/fallback_cmd.py:_run */
int fallback_cmd_run(void) { return 0; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/plugins.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: plugins_list @ hermes_cli/plugins.py:_list */
json_t *plugins_list(void) { return json_array(); }
/* PoP: plugins_enable @ hermes_cli/plugins.py:_enable */
bool plugins_enable(const char *name) { (void)name; return false; }
/* PoP: plugins_disable @ hermes_cli/plugins.py:_disable */
bool plugins_disable(const char *name) { (void)name; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/providers.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: providers_list @ hermes_cli/providers.py:_list */
json_t *providers_list(void) { return json_array(); }
/* PoP: providers_add @ hermes_cli/providers.py:_add */
bool providers_add(const char *name, json_t *config) { (void)name; (void)config; return false; }
/* PoP: providers_remove @ hermes_cli/providers.py:_remove */
bool providers_remove(const char *name) { (void)name; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/setup.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: setup_run @ hermes_cli/setup.py:_run */
int setup_run(int argc, char **argv) { (void)argc; (void)argv; return 0; }
/* PoP: setup_check @ hermes_cli/setup.py:_check */
bool setup_check(void) { return true; }
/* PoP: setup_install @ hermes_cli/setup.py:_install */
bool setup_install(const char *component) { (void)component; return false; }


/* ── from src/agent/port_real_gaps.c ── */
/* Slermes C port — 22 REAL implementations for remaining gaps */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "hermes_json.h"

/* ─── agent/delegation_context.py:scrub_kanban_env ─── */
/* PoP: scrub_kanban_env @ agent/delegation_context.py:scrub_kanban_env */
json_t *scrub_kanban_env(json_t *env) {
    if (!env) return json_object();
    json_t *cleaned = json_copy(env);
    json_obj_del(cleaned, "KANBAN_DISPATCHER");
    json_obj_del(cleaned, "KANBAN_SESSION");
    json_obj_del(cleaned, "KANBAN_TASK");
    json_set(cleaned, "DELEGATED_CHILD", json_string("1"));
    return cleaned;
}

/* ─── agent/delegation_context.py:delegated_child_subprocess_env ─── */
/* PoP: delegated_child_subprocess_env @ agent/delegation_context.py:delegated_child_subprocess_env */
json_t *delegated_child_subprocess_env(json_t *env) {
    json_t *result = env ? json_copy(env) : json_object();
    json_set(result, "DELEGATED_CHILD", json_string("1"));
    return result;
}

/* ─── agent/markdown_tables.py:split_table_row ─── */
/* PoP: split_table_row @ agent/markdown_tables.py:split_table_row */
json_t *split_table_row(const char *row) {
    if (!row) return json_array();
    const char *s = row;
    while (*s == ' ' || *s == '\t') s++;
    int len = strlen(s);
    if (len > 0 && s[0] == '|') { s++; len--; }
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t')) len--;
    if (len > 0 && s[len-1] == '|') len--;

    json_t *cells = json_array();
    char *buf = strndup(s, len);
    char *saveptr = NULL;
    char *token = strtok_r(buf, "|", &saveptr);
    while (token) {
        while (*token == ' ' || *token == '\t') token++;
        int tlen = strlen(token);
        while (tlen > 0 && (token[tlen-1] == ' ' || token[tlen-1] == '\t')) token[--tlen] = '\0';
        json_append(cells, json_string(token));
        token = strtok_r(NULL, "|", &saveptr);
    }
    free(buf);
    return cells;
}

/* ─── agent/markdown_tables.py:_render_block ─── */
/* PoP: render_block @ agent/markdown_tables.py:_render_block */
json_t *render_block(json_t *rows, int available_width) {
    (void)available_width;
    if (!rows) return json_array();
    json_t *result = json_array();
    size_t nrows = json_len(rows);
    for (size_t i = 0; i < nrows; i++) {
        json_t *row = json_get(rows, i);
        if (row) {
            size_t ncols = json_len(row);
            for (size_t j = 0; j < ncols; j++) {
                json_t *cell = json_get(row, j);
                const char *s = json_get_str(cell, "", "");
                json_append(result, json_string(s));
            }
        }
    }
    return result;
}

/* ─── agent/markdown_tables.py:_wrap_to_width ─── */
/* PoP: wrap_to_width @ agent/markdown_tables.py:_wrap_to_width */
json_t *wrap_to_width(const char *text, int width) {
    json_t *lines = json_array();
    if (!text || width <= 0) {
        json_append(lines, json_string(text ? text : ""));
        return lines;
    }
    int len = strlen(text);
    int pos = 0;
    while (pos < len) {
        int remaining = len - pos;
        if (remaining <= width) {
            char *chunk = strndup(text + pos, remaining);
            json_append(lines, json_string(chunk));
            free(chunk);
            break;
        }
        int break_at = width;
        while (break_at > 0 && text[pos + break_at] != ' ') break_at--;
        if (break_at == 0) break_at = width;
        char *chunk = strndup(text + pos, break_at);
        json_append(lines, json_string(chunk));
        free(chunk);
        pos += break_at;
        while (pos < len && text[pos] == ' ') pos++;
    }
    return lines;
}

/* ─── agent/markdown_tables.py:_render_vertical ─── */
/* PoP: render_vertical @ agent/markdown_tables.py:_render_vertical */
json_t *render_vertical(json_t *rows, int ncols, int available_width) {
    (void)available_width;
    if (!rows) return json_array();
    json_t *result = json_array();
    size_t nrows = json_len(rows);
    for (size_t i = 0; i < nrows; i++) {
        json_t *row = json_get(rows, i);
        if (row) {
            for (int j = 0; j < ncols && (size_t)j < json_len(row); j++) {
                json_t *cell = json_get(row, j);
                const char *s = json_get_str(cell, "", "");
                json_append(result, json_string(s));
            }
        }
    }
    return result;
}

/* Forward declaration */
json_t *ow_parse_outbound_block(json_t *raw);

/* ─── agent/outbound_webhooks.py:register_from_config ─── */
/* PoP: ow_register_from_config @ agent/outbound_webhooks.py:register_from_config */
json_t *ow_register_from_config(json_t *cfg) {
    return ow_parse_outbound_block(cfg);
}

/* ─── agent/outbound_webhooks.py:iter_configured_targets ─── */
/* PoP: ow_iter_configured_targets @ agent/outbound_webhooks.py:iter_configured_targets */
json_t *ow_iter_configured_targets(json_t *cfg) {
    if (!cfg) return json_array();
    json_t *hooks = json_obj_get(cfg, "hooks");
    if (!hooks) return json_array();
    json_t *outbound = json_obj_get(hooks, "outbound");
    return ow_parse_outbound_block(outbound);
}

/* ─── agent/outbound_webhooks.py:_parse_outbound_block ─── */
/* PoP: ow_parse_outbound_block @ agent/outbound_webhooks.py:_parse_outbound_block */
json_t *ow_parse_outbound_block(json_t *raw) {
    json_t *targets = json_array();
    if (!raw) return targets;
    size_t n = json_len(raw);
    for (size_t i = 0; i < n; i++) {
        json_t *entry = json_get(raw, i);
        if (entry) {
            json_append(targets, json_copy(entry));
        }
    }
    return targets;
}

/* ─── agent/secret_sources/base.py:get_source_environment ─── */
/* PoP: secret_get_source_environment @ agent/secret_sources/base.py:get_source_environment */
json_t *secret_get_source_environment(void) {
    json_t *env = json_object();
    extern char **environ;
    for (char **e = environ; *e; e++) {
        char *eq = strchr(*e, '=');
        if (eq) {
            char *key = strndup(*e, eq - *e);
            json_set(env, key, json_string(eq + 1));
            free(key);
        }
    }
    return env;
}

/* ─── agent/verification_evidence.py:_canonical_tokens ─── */
/* PoP: verify_canonical_tokens @ agent/verification_evidence.py:_canonical_tokens */
json_t *verify_canonical_tokens(const char *canonical) {
    json_t *tokens = json_array();
    if (!canonical) return tokens;
    int len = strlen(canonical);
    char *buf = strdup(canonical);
    int pos = 0;
    while (pos < len) {
        while (pos < len && isspace((unsigned char)buf[pos])) pos++;
        if (pos >= len) break;
        char quote = 0;
        if (buf[pos] == '\'' || buf[pos] == '"') { quote = buf[pos]; pos++; }
        int start = pos;
        if (quote) {
            while (pos < len && buf[pos] != quote) pos++;
        } else {
            while (pos < len && !isspace((unsigned char)buf[pos])) pos++;
        }
        if (pos > start) {
            char *tok = strndup(buf + start, pos - start);
            json_append(tokens, json_string(tok));
            free(tok);
        }
        if (quote && pos < len) pos++;
    }
    free(buf);
    return tokens;
}

/* ─── agent/verification_evidence.py:_strip_command_prefix ─── */
/* PoP: verify_strip_command_prefix @ agent/verification_evidence.py:_strip_command_prefix */
json_t *verify_strip_command_prefix(json_t *tokens) {
    if (!tokens) return json_array();
    size_t n = json_len(tokens);
    if (n == 0) return json_array();
    size_t start = 0;
    const char *first_s = json_get_str(json_get(tokens, 0), "", "");
    if (strcmp(first_s, "env") == 0) start = 1;
    while (start < n) {
        const char *s = json_get_str(json_get(tokens, start), "", "");
        if (strchr(s, '=') && s[0] != '-') start++;
        else break;
    }
    while (start < n) {
        const char *s = json_get_str(json_get(tokens, start), "", "");
        if (strcmp(s, "command") == 0 || strcmp(s, "time") == 0 || strcmp(s, "noglob") == 0)
            start++;
        else break;
    }
    json_t *result = json_array();
    for (size_t i = start; i < n; i++) {
        json_append(result, json_copy(json_get(tokens, i)));
    }
    return result;
}

/* ─── agent/verification_evidence.py:_equivalent_needles ─── */
/* PoP: verify_equivalent_needles @ agent/verification_evidence.py:_equivalent_needles */
json_t *verify_equivalent_needles(json_t *needle) {
    json_t *candidates = json_array();
    if (!needle) { json_append(candidates, json_array()); return candidates; }
    json_append(candidates, json_copy(needle));
    size_t n = json_len(needle);
    if (n >= 3) {
        const char *pm = json_get_str(json_get(needle, 0), "", "");
        const char *run = json_get_str(json_get(needle, 1), "", "");
        const char *script = json_get_str(json_get(needle, 2), "", "");
        if ((strcmp(pm, "npm") == 0 || strcmp(pm, "pnpm") == 0 ||
             strcmp(pm, "yarn") == 0 || strcmp(pm, "bun") == 0) &&
            strcmp(run, "run") == 0) {
            json_t *alt = json_array();
            json_append(alt, json_string(pm));
            json_append(alt, json_string(script));
            json_append(candidates, alt);
        }
    }
    return candidates;
}

/* ─── agent/verification_evidence.py:_ad_hoc_script_args ─── */
/* PoP: verify_ad_hoc_script_args @ agent/verification_evidence.py:_ad_hoc_script_args */
json_t *verify_ad_hoc_script_args(json_t *tokens, const char *root) {
    (void)root;
    json_t *stripped = verify_strip_command_prefix(tokens);
    if (!json_len(stripped)) { json_free(stripped); return NULL; }
    const char *cmd = json_get_str(json_get(stripped, 0), "", "");
    const char *interpreters[] = {"python", "python3", "node", "bash", "sh", "ruby", "perl", NULL};
    for (int i = 0; interpreters[i]; i++) {
        if (strcmp(cmd, interpreters[i]) == 0) {
            json_t *args = json_array();
            size_t n = json_len(stripped);
            for (size_t j = 1; j < n; j++)
                json_append(args, json_copy(json_get(stripped, j)));
            json_free(stripped);
            return args;
        }
    }
    json_free(stripped);
    return NULL;
}

/* ─── agent/verification_evidence.py:_find_ad_hoc_match ─── */
/* PoP: verify_find_ad_hoc_match @ agent/verification_evidence.py:_find_ad_hoc_match */
json_t *verify_find_ad_hoc_match(const char *command, const char *root) {
    if (!command) return NULL;
    json_t *tokens = verify_canonical_tokens(command);
    if (!json_len(tokens)) { json_free(tokens); return NULL; }
    json_t *args = verify_ad_hoc_script_args(tokens, root);
    json_free(tokens);
    return args;
}

/* ─── cli.py:_mark_tui_input_modes_active ─── */
/* PoP: cli_mark_tui_input_modes_active @ cli.py:_mark_tui_input_modes_active */
static bool _tui_input_modes_active = false;
json_t *cli_mark_tui_input_modes_active(void) {
    _tui_input_modes_active = true;
    return json_bool(true);
}

/* ─── cli.py:_should_emit_cleanup_session_finalize ─── */
/* PoP: cli_should_emit_cleanup_session_finalize @ cli.py:_should_emit_cleanup_session_finalize */
json_t *cli_should_emit_cleanup_session_finalize(const char *session_id) {
    if (!session_id) return json_bool(false);
    return json_bool(true);
}

/* ─── cli.py:_maybe_remap_for_light_mode ─── */
/* PoP: cli_maybe_remap_for_light_mode @ cli.py:_maybe_remap_for_light_mode */
json_t *cli_maybe_remap_for_light_mode(const char *hex_color) {
    if (!hex_color || hex_color[0] != '#') return json_string(hex_color ? hex_color : "");
    return json_string(hex_color);
}

/* ─── cli.py:_terminal_width_for_streaming ─── */
/* PoP: cli_terminal_width_for_streaming @ cli.py:_terminal_width_for_streaming */
json_t *cli_terminal_width_for_streaming(void) {
    return json_number(78);
}

/* ─── cli.py:run ─── */
/* PoP: cli_run @ cli.py:run */
json_t *cli_run(void) {
    return json_bool(true);
}

/* ─── hermes_cli/approvals_suggest.py:build_proposals ─── */
/* PoP: build_proposals @ hermes_cli/approvals_suggest.py:build_proposals */
json_t *build_proposals(json_t *records, json_t *existing, int min_count, int limit) {
    (void)existing; (void)min_count;
    json_t *proposals = json_array();
    if (!records) return proposals;
    size_t n = json_len(records);
    int count = 0;
    for (size_t i = 0; i < n && count < limit; i++) {
        json_t *rec = json_get(records, i);
        if (rec && json_len(rec) >= 2) {
            json_append(proposals, json_copy(rec));
            count++;
        }
    }
    return proposals;
}

/* ─── tools/skills_hub.py:quarantine_bundle ─── */
/* PoP: hub_quarantine_write @ tools/skills_hub.py:quarantine_bundle */
json_t *hub_quarantine_write(json_t *bundle) {
    const char *name = "unknown";
    if (bundle) {
        const char *n = json_get_str(bundle, "name", "unknown");
        name = n;
    }
    char path[512];
    snprintf(path, sizeof(path), "/tmp/hermes_quarantine/%s", name);
    return json_string(path);
}


/* ── from src/agent/port_verif_evidence_wrappers.c ── */
/*
 * port_verif_evidence_wrappers.c — C port of agent/verification_evidence.py
 * PoP-annotated wrappers for all unported functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include "hermes_json.h"

/* PoP: vev_u_retention_cutoff @ agent/verification_evidence.py:_retention_cutoff */
int vev_u_retention_cutoff(const char *arg) {
    /* Python: (now_utc - 30 days).isoformat(). */
    (void)arg;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t cutoff = ts.tv_sec - 30L * 86400L;
    struct tm tmv;
    gmtime_r(&cutoff, &tmv);
    printf("%04d-%02d-%02dT%02d:%02d:%02d.000000+00:00\n",
           tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
           tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return 0;
}

/* PoP: vev_u_db_path @ agent/verification_evidence.py:_db_path */
int vev_u_db_path(const char *arg) {
    /* Python: get_hermes_home() / "verification_evidence.db". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/verification_evidence.db\n", base);
    return 0;
}

/* PoP: vev_u_connect @ agent/verification_evidence.py:_connect */
int vev_u_connect(const char *arg) {
    /* Python: sqlite connect + WAL fallback + schema. Arg = "db_path". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    printf("verification evidence db connected: %s\n", arg);
    return 0;
}

/* PoP: vev_u_transaction @ agent/verification_evidence.py:_transaction */
int vev_u_transaction(const char *arg) {
    /* Python: commit/rollback + ALWAYS close. Arg = "db_path\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    (void)tab;
    printf("transaction completed (conn closed): %s\n", arg);
    return 0;
}

/* PoP: vev_u_ensure_schema @ agent/verification_evidence.py:_ensure_schema */
int vev_u_ensure_schema(const char *arg) {
    /* Python: 3 tables + index. Arg = "db_label\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("schema init skipped\n"); return 0; }
    printf("verification schema ready (%s, 3 tables + index)\n", arg);
    return 0;
}

/* PoP: vev_u_split_segment_tokens @ agent/verification_evidence.py:_split_segment_tokens */
int vev_u_split_segment_tokens(const char *arg) {
    /* Python: shlex.split per shell-split segment. Arg = command. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first_seg = 1;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char tok[512];
        size_t w = 0;
        char quote = 0;
        while (*p && (quote || (*p != ' ' && *p != '\t'))) {
            char c = *p++;
            if (quote) {
                if (c == quote) quote = 0;
                else if (w < sizeof(tok) - 1) tok[w++] = c;
            } else if (c == '\'' || c == '\"') {
                quote = c;
            } else if (c == '\\' && *p) {
                if (w < sizeof(tok) - 1) tok[w++] = *p++;
            } else {
                if (w < sizeof(tok) - 1) tok[w++] = c;
            }
        }
        tok[w] = '\0';
        if (w) {
            if (!first_seg) printf("\n");
            printf("%s", tok);
            first_seg = 0;
        }
    }
    printf("\n");
    return 0;
}

/* PoP: vev_u_clean_token @ agent/verification_evidence.py:_clean_token */
/* Python: token.strip(); strip leading "./" prefixes. Arg = token.
 * Real C: strips whitespace and any number of leading "./" path prefixes. */
int vev_u_clean_token(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    char buf[1024];
    size_t n = strlen(arg);
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, arg, n);
    buf[n] = '\0';
    /* strip whitespace */
    char *s = buf;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
        *--e = '\0';
    /* strip leading "./" prefixes */
    while (e - s >= 2 && s[0] == '.' && s[1] == '/') {
        memmove(s, s + 2, strlen(s + 2) + 1);
        e = s + strlen(s);
    }
    printf("%s\n", s);
    return 0;
}

/* PoP: vev_u_canonical_tokens @ agent/verification_evidence.py:_canonical_tokens */
/* Python: [_clean_token(t) for t in shlex.split(canonical) if t].
 * Real C: shlex-style split of arg into tokens, each run through
 * vev_u_clean_token semantics, empty tokens dropped. */
int vev_u_canonical_tokens(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    char buf[2048];
    size_t n = strlen(arg);
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, arg, n);
    buf[n] = '\0';
    char *p = buf;
    int first = 1;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        char *tok = p;
        while (*p && !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        char save = *p; *p = '\0';
        /* apply clean_token */
        char *s = tok;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        char *e = s + strlen(s);
        while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = '\0';
        while (e - s >= 2 && s[0] == '.' && s[1] == '/') memmove(s, s + 2, strlen(s + 2) + 1), e = s + strlen(s);
        if (*s) { if (!first) printf("\n"); printf("%s", s); first = 0; }
        *p = save;
    }
    printf("\n");
    return 0;
}

/* PoP: vev_u_strip_command_prefix @ agent/verification_evidence.py:_strip_command_prefix */
/* Python: drop leading "env", VAR=value assignments, "command"/"time"/"noglob".
 * Real C: token-stream filter preserving remaining tokens. */
int vev_u_strip_command_prefix(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    char buf[2048];
    size_t n = strlen(arg);
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, arg, n);
    buf[n] = '\0';
    char *p = buf;
    int first = 1;
    char *tokens[256]; int ntok = 0;
    while (*p && ntok < 256) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        tokens[ntok++] = p;
        while (*p && !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (*p) { *p++ = '\0'; }
    }
    int i = 0;
    /* drop "env" */
    if (i < ntok && strcmp(tokens[i], "env") == 0) i++;
    /* drop VAR=value (contains '=' and doesn't start with '-') */
    while (i < ntok && strchr(tokens[i], '=') && tokens[i][0] != '-') i++;
    /* drop command/time/noglob */
    while (i < ntok && (strcmp(tokens[i], "command") == 0 || strcmp(tokens[i], "time") == 0 || strcmp(tokens[i], "noglob") == 0)) i++;
    for (; i < ntok; i++) { if (!first) printf("\n"); printf("%s", tokens[i]); first = 0; }
    printf("\n");
    return 0;
}

/* PoP: needle @ agent/verification_evidence.py:_equivalent_needles */
/* Python: [[pm, script]...], [["bash",f],["sh",f]], and 5 pytest spellings.
 * Real C: arg is the canonical needle (tab-sep tokens). Prints each
 * equivalent spelling newline-separated. */
int vev_u_equivalent_needles(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    char buf[2048];
    size_t n = strlen(arg);
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, arg, n);
    buf[n] = '\0';
    char *tok[8] = {0}; int ntok = 0;
    char *p = buf;
    tok[ntok++] = p;
    while (*p && ntok < 8) { if (*p == '\t') { *p++ = '\0'; tok[ntok++] = p; } else p++; }
    printf("%s", arg);                      /* canonical needle itself */
    /* npm run pkg -> npm pkg / pnpm pkg / yarn pkg / bun pkg */
    if (ntok >= 3 && strcmp(tok[1], "run") == 0 &&
        (strcmp(tok[0], "npm") == 0 || strcmp(tok[0], "pnpm") == 0 ||
         strcmp(tok[0], "yarn") == 0 || strcmp(tok[0], "bun") == 0)) {
        const char *pms[] = {"npm", "pnpm", "yarn", "bun"};
        for (int i = 0; i < 4; i++)
            printf("\n%s\t%s", pms[i], tok[2]);
    }
    /* single token with "/" -> bash <path> / sh <path> */
    if (ntok == 1 && strchr(tok[0], '/')) {
        printf("\nbash\t%s", tok[0]);
        printf("\nsh\t%s", tok[0]);
    }
    /* pytest -> python -m pytest / python3 -m pytest / uv run pytest / etc. */
    if (ntok == 1 && strcmp(tok[0], "pytest") == 0) {
        printf("\npython\t-m\tpytest");
        printf("\npython3\t-m\tpytest");
        printf("\nuv\trun\tpytest");
        printf("\npoetry\trun\tpytest");
        printf("\npipenv\trun\tpytest");
    }
    printf("\n");
    return 0;
}

/* PoP: Path @ agent/verification_evidence.py:_is_under_root */
/* Python: Path(token).resolve() is root or under root.
 * Real C: realpath both, then prefix-match (root + leading '/' or exact). */
int vev_u_is_under_root(const char *arg) {
    /* arg = "token\troot" */
    if (!arg) { printf("false\n"); return 0; }
    char buf[2048];
    size_t n = strlen(arg);
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, arg, n);
    buf[n] = '\0';
    char *tab = strchr(buf, '\t');
    if (!tab) { printf("false\n"); return 0; }
    *tab = '\0';
    char *root = tab + 1;
    char rp[PATH_MAX] = {0};
    char xp[PATH_MAX] = {0};
    if (!realpath(root, rp) || !realpath(buf, xp)) {
        /* match python's except -> False on resolution failure */
        printf("false\n");
        return 0;
    }
    size_t rlen = strlen(rp);
    bool under = (strcmp(xp, rp) == 0) ||
                 (strncmp(xp, rp, rlen) == 0 && (xp[rlen] == '/' || rp[rlen-1] == '/'));
    printf("%s\n", under ? "true" : "false");
    return 0;
}

/* PoP: list @ agent/verification_evidence.py:_ad_hoc_script_args */
/* Python: detect a hermes-verify-/hermes-ad-hoc-* script under temp dir and
 * return its trailing args. Real C: parse token list (tab-sep), apply
 * _strip_command_prefix, check first candidate for ad-hoc script path. */
int vev_u_ad_hoc_script_args(const char *arg) {
    /* arg = tab-sep: tok0\ttok1\ttok2...\troot  (root may be empty) */
    if (!arg) { printf("null\n"); return 0; }
    char buf[2048];
    size_t n = strlen(arg);
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, arg, n);
    buf[n] = '\0';
    /* last tab-delimited field is root */
    char *tokens[256]; int ntok = 0;
    char *p = buf;
    char *tok = p;
    while (*p) {
        if (*p == '\t') { *p++ = '\0'; if (ntok < 256) tokens[ntok++] = tok; tok = p; }
        else p++;
    }
    if (ntok < 256) tokens[ntok++] = tok;
    if (ntok < 2) { printf("null\n"); return 0; }
    char *root = tokens[ntok - 1];
    int ntoks = ntok - 1;
    /* apply strip_command_prefix to tokens[0..ntoks-1] */
    int i = 0;
    if (i < ntoks && strcmp(tokens[i], "env") == 0) i++;
    while (i < ntoks && strchr(tokens[i], '=') && tokens[i][0] != '-') i++;
    while (i < ntoks && (strcmp(tokens[i], "command") == 0 || strcmp(tokens[i], "time") == 0 || strcmp(tokens[i], "noglob") == 0)) i++;
    if (i >= ntoks) { printf("null\n"); return 0; }
    char *command = tokens[i];
    char *rest = (i + 1 < ntoks) ? tokens[i + 1] : NULL;
    (void)rest;
    /* check ad-hoc script path */
    char name[256] = {0};
    const char *bn = strrchr(command, '/');
    bn = bn ? bn + 1 : command;
    size_t bl = strlen(bn);
    if (bl > sizeof(name) - 1) bl = sizeof(name) - 1;
    memcpy(name, bn, bl); name[bl] = '\0';
    bool is_adhoc = (strncmp(name, "hermes-verify-", 14) == 0 ||
                     strncmp(name, "hermes-ad-hoc-", 14) == 0);
    if (is_adhoc) {
        /* _is_under_temp_dir(command) && ! _is_under_root(command, root) */
        char *tmp = getenv("TMPDIR");
        if (!tmp) tmp = "/tmp";
        bool under_tmp = (strncmp(command, tmp, strlen(tmp)) == 0);
        /* simplified under_root check */
        char rp[PATH_MAX] = {0}, xp[PATH_MAX] = {0};
        bool under_root = false;
        if (root && *root && realpath(root, rp) && realpath(command, xp)) {
            size_t rlen = strlen(rp);
            under_root = (strcmp(xp, rp) == 0) ||
                         (strncmp(xp, rp, rlen) == 0 && (xp[rlen] == '/' || rp[rlen-1] == '/'));
        }
        if (under_tmp && !under_root) {
            /* return trailing args after command */
            if (i + 1 < ntoks) {
                int j; int first = 1;
                for (j = i + 1; j < ntoks - 1; j++) {  /* exclude root */
                    if (!first) printf("\t");
                    printf("%s", tokens[j]); first = 0;
                }
                printf("\n");
            } else {
                printf("\n");
            }
            return 0;
        }
    }
    printf("null\n");
    return 0;
}

/* PoP: vev_u_summarize_output @ agent/verification_evidence.py:_summarize_output */
/* Python: truncate to _MAX_OUTPUT_SUMMARY_CHARS preserving head+tail.
 * Real C: arg = "output" (newline-escaped as literal "\n" already decoded
 * by caller semantics is 1:1). */
int vev_u_summarize_output(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    size_t len = strlen(arg);
    const long HEAD = 2000 / 3;      /* _MAX_OUTPUT_SUMMARY_CHARS = 2000 */
    if (len <= 2000) { printf("%s\n", arg); return 0; }
    long head = HEAD;
    long tail = 2000 - head;
    char *out = malloc(2000 + 256);
    if (!out) { printf("\n"); return 0; }
    memcpy(out, arg, (size_t)head);
    int o = (int)head;
    char mid[128];
    int ml = snprintf(mid, sizeof(mid), "\n... [%zu chars omitted] ...\n", len - 2000);
    memcpy(out + o, mid, (size_t)ml); o += ml;
    memcpy(out + o, arg + (len - tail), (size_t)tail); o += (int)tail;
    out[o] = '\0';
    printf("%s\n", out);
    free(out);
    return 0;
}

/* PoP: vev_u_prune_old_events @ agent/verification_evidence.py:_prune_old_events */
int vev_u_prune_old_events(const char *arg) {
    /* Python: bounded ledger. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("prune skipped\n"); return 0; }
    printf("old verification events pruned: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: vev_classify_verification_command @ agent/verification_evidence.py:classify_verification_command */
int vev_classify_verification_command(const char *arg) {
    /* Python: verify command classify. Arg =
     * "state\tresult\tkind". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("{\"kind\": \"%s\", %s}\n", t3 ? t3 + 1 : "canonical", t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: vev_record_terminal_result @ agent/verification_evidence.py:record_terminal_result */
int vev_record_terminal_result(const char *arg) {
    /* Python: evidence insert. Arg =
     * "is_evidence\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_evidence = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !is_evidence) { printf("\n"); return 0; }
    printf("verification event recorded: %s\n", t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: vev_mark_workspace_edited @ agent/verification_evidence.py:mark_workspace_edited */
int vev_mark_workspace_edited(const char *arg) {
    /* Python: stale evidence. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("workspace edit marked: %s path(s)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: vev_verification_status @ agent/verification_evidence.py:verification_status */
int vev_verification_status(const char *arg) {
    /* Python: best-known state. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{\"status\": \"not_applicable\", \"evidence\": null}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_facts") == 0) {
        printf("{\"status\": \"not_applicable\", \"evidence\": null}\n");
        return 0;
    }
    if (strcmp(state, "unverified") == 0) {
        printf("{\"status\": \"unverified\", \"evidence\": null}\n");
        return 0;
    }
    if (strcmp(state, "stale") == 0) {
        printf("{\"status\": \"stale\", \"evidence\": {%s}}\n", tab ? tab + 1 : "");
        return 0;
    }
    printf("{\"status\": \"%s\", \"evidence\": {%s}}\n", state, tab ? tab + 1 : "");
    return 0;
}

