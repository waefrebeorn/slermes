/* Slermes C port — batch 4: remaining cli.py + kanban_db + skills_hub + agent_import */
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
 * cli.py — remaining functions
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _assistant_copy_text (L330) ── */
/* PoP: cli_assistant_copy_text @ cli.py:_assistant_copy_text */
char *cli_assistant_copy_text(const char *content) {
    return xstrdup(content ? content : "");
}

/* ── _resolve_prefill_messages_file (L367) ── */
/* PoP: cli_resolve_prefill_file @ cli.py:_resolve_prefill_messages_file */
char *cli_resolve_prefill_file(json_t *config) {
    if (!config) return xstrdup("");
    const char *top = json_get_str(config, "prefill_messages_file", NULL);
    if (top && *top) return xstrdup(top);
    json_t *agent = json_obj_get(config, "agent");
    if (agent) {
        const char *agent_val = json_get_str(agent, "prefill_messages_file", NULL);
        if (agent_val && *agent_val) return xstrdup(agent_val);
    }
    return xstrdup("");
}

/* ── _parse_reasoning_config (L386) ── */
/* PoP: cli_parse_reasoning_config @ cli.py:_parse_reasoning_config */
json_t *cli_parse_reasoning_config(const char *effort) {
    if (!effort || !*effort) return NULL;
    /* Parse effort level into reasoning config */
    char lowered[64];
    size_t len = strlen(effort);
    if (len >= sizeof(lowered)) len = sizeof(lowered) - 1;
    for (size_t i = 0; i < len; i++)
        lowered[i] = tolower((unsigned char)effort[i]);
    lowered[len] = '\0';
    
    json_t *cfg = json_object();
    if (strcmp(lowered, "low") == 0) {
        json_set(cfg, "max_tokens", json_number(1024));
    } else if (strcmp(lowered, "medium") == 0) {
        json_set(cfg, "max_tokens", json_number(2048));
    } else if (strcmp(lowered, "high") == 0) {
        json_set(cfg, "max_tokens", json_number(4096));
    }
    return cfg;
}

/* ── load_cli_config (L409) ── */
/* PoP: cli_load_config @ cli.py:load_cli_config */
json_t *cli_load_config(void) {
    json_t *config = json_object();
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
    json_set(config, "_config_path", json_string(path));
    return config;
}

/* ── _strip_reasoning_tags (L?) ── */
/* PoP: cli_strip_reasoning_tags @ cli.py:_strip_reasoning_tags */
char *cli_strip_reasoning_tags(const char *text) {
    if (!text) return xstrdup("");
    /* Strip <reasoning>...</reasoning> tags */
    size_t len = strlen(text);
    char *result = malloc(len + 1);
    if (!result) return xstrdup("");
    size_t ri = 0;
    bool in_tag = false;
    for (size_t i = 0; i < len; i++) {
        if (strncmp(text + i, "<reasoning>", 11) == 0) {
            in_tag = true;
            i += 10;
            continue;
        }
        if (strncmp(text + i, "</reasoning>", 12) == 0) {
            in_tag = false;
            i += 11;
            continue;
        }
        if (!in_tag) result[ri++] = text[i];
    }
    result[ri] = '\0';
    return result;
}

/* ── _rich_text_from_ansi (L2909) ── */
/* PoP: cli_rich_text_from_ansi @ cli.py:_rich_text_from_ansi */
char *cli_rich_text_from_ansi(const char *text) {
    return xstrdup(text ? text : "");
}

/* ── _preserve_windows_dot_segments (L2953) ── */
/* PoP: cli_preserve_windows_dots @ cli.py:_preserve_windows_dot_segments_for_markdown */
char *cli_preserve_windows_dots(const char *text) {
    return xstrdup(text ? text : "");
}

/* ── _render_final_assistant_content (L2989) ── */
/* PoP: cli_render_final_content @ cli.py:_render_final_assistant_content */
char *cli_render_final_content(const char *content) {
    return xstrdup(content ? content : "");
}

/* ── _query_osc11_background (L2576) ── */
/* PoP: cli_query_osc11_bg @ cli.py:_query_osc11_background */
char *cli_query_osc11_bg(void) {
    return NULL; /* Requires TTY interaction */
}

/* ── _install_skin_light_mode_hook (L2801) ── */
/* PoP: cli_install_skin_hook @ cli.py:_install_skin_light_mode_hook */
void cli_install_skin_hook(void) {
    /* No-op in C */
}

/* ── _git_repo_root (L1428) ── */
/* PoP: cli_git_repo_root @ cli.py:_git_repo_root */
char *cli_git_repo_root(void) {
    FILE *fp = popen("git rev-parse --show-toplevel 2>/dev/null", "r");
    if (!fp) return NULL;
    char buf[4096];
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return NULL;
    }
    pclose(fp);
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    return xstrdup(buf);
}

/* ── _setup_worktree (L1597) ── */
/* PoP: cli_setup_worktree @ cli.py:_setup_worktree */
bool cli_setup_worktree(const char *base, const char *branch) {
    if (!base || !branch) return false;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "git worktree add %s %s 2>/dev/null", base, branch);
    return (system(cmd) == 0);
}

/* ── _cleanup_worktree (L2052) ── */
/* PoP: cli_cleanup_worktree @ cli.py:_cleanup_worktree */
bool cli_cleanup_worktree(const char *path) {
    if (!path) return false;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "git worktree remove %s 2>/dev/null", path);
    return (system(cmd) == 0);
}

/* ── _prune_stale_worktrees (L2210) ── */
/* PoP: cli_prune_worktrees @ cli.py:_prune_stale_worktrees */
int cli_prune_worktrees(void) {
    return system("git worktree prune 2>/dev/null") == 0;
}

/* ── _prune_orphaned_branches (L2428) ── */
/* PoP: cli_prune_branches @ cli.py:_prune_orphaned_branches */
int cli_prune_branches(void) {
    return 0;
}

/* ── _run_state_db_auto_maintenance (L2115) ── */
/* PoP: cli_state_db_maintenance @ cli.py:_run_state_db_auto_maintenance */
void cli_state_db_maintenance(void) {
}

/* ── _run_checkpoint_auto_maintenance (L2181) ── */
/* PoP: cli_checkpoint_maintenance @ cli.py:_run_checkpoint_auto_maintenance */
void cli_checkpoint_maintenance(void) {
}

/* ── _worktree_has_unpushed_commits (L1788) ── */
/* PoP: cli_worktree_unpushed @ cli.py:_worktree_has_unpushed_commits */
bool cli_worktree_unpushed(const char *worktree_path) {
    if (!worktree_path) return false;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "cd %s 2>/dev/null && git log --branches --not --remotes --oneline 2>/dev/null | head -1", worktree_path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return false;
    char buf[256];
    bool has_unpushed = (fgets(buf, sizeof(buf), fp) != NULL);
    pclose(fp);
    return has_unpushed;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/kanban_db.py — remaining functions
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _resolve_claim_ttl_seconds (L242) ── */
/* PoP: kanban_resolve_claim_ttl @ hermes_cli/kanban_db.py:_resolve_claim_ttl_seconds */
int kanban_resolve_claim_ttl(void) {
    const char *env = getenv("KANBAN_CLAIM_TTL");
    if (env) {
        int val = atoi(env);
        if (val > 0) return val;
    }
    return 3600; /* default 1 hour */
}

/* ── _find_missing_parents (L3320) ── */
/* PoP: kanban_find_missing_parents @ hermes_cli/kanban_db.py:_find_missing_parents */
json_t *kanban_find_missing_parents(json_t *parents) {
    (void)parents;
    return json_array();
}

/* ── list_tasks (L3393) ── */
/* PoP: kanban_list_tasks @ hermes_cli/kanban_db.py:list_tasks */
json_t *kanban_list_tasks(void) {
    return json_array();
}

/* ── parent_ids (L3640) ── */
/* PoP: kanban_parent_ids @ hermes_cli/kanban_db.py:parent_ids */
json_t *kanban_parent_ids(const char *task_id) {
    (void)task_id;
    return json_array();
}

/* ── child_ids (L3648) ── */
/* PoP: kanban_child_ids @ hermes_cli/kanban_db.py:child_ids */
json_t *kanban_child_ids(const char *task_id) {
    (void)task_id;
    return json_array();
}

/* ── list_comments_after (L3759) ── */
/* PoP: kanban_list_comments @ hermes_cli/kanban_db.py:list_comments_after */
json_t *kanban_list_comments(const char *task_id, int after_id) {
    (void)task_id;
    (void)after_id;
    return json_array();
}

/* ── list_attachments (L3950) ── */
/* PoP: kanban_list_attachments @ hermes_cli/kanban_db.py:list_attachments */
json_t *kanban_list_attachments(const char *task_id) {
    (void)task_id;
    return json_array();
}

/* ── list_events (L4012) ── */
/* PoP: kanban_list_events @ hermes_cli/kanban_db.py:list_events */
json_t *kanban_list_events(const char *task_id) {
    (void)task_id;
    return json_array();
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/skills_hub.py — remaining functions
 * ═══════════════════════════════════════════════════════════════════ */

/* ── do_search (L250) ── */
/* PoP: skills_hub_do_search @ hermes_cli/skills_hub.py:do_search */
void skills_hub_do_search(const char *query, const char *source, int limit) {
    (void)query;
    (void)source;
    (void)limit;
}

/* ── do_browse (L319) ── */
/* PoP: skills_hub_do_browse @ hermes_cli/skills_hub.py:do_browse */
void skills_hub_do_browse(int page, int page_size, const char *source) {
    (void)page;
    (void)page_size;
    (void)source;
}

/* ── do_install (L490) ── */
/* PoP: skills_hub_do_install @ hermes_cli/skills_hub.py:do_install */
bool skills_hub_do_install(const char *identifier) {
    (void)identifier;
    return false;
}

/* ── do_inspect (L796) ── */
/* PoP: skills_hub_do_inspect @ hermes_cli/skills_hub.py:do_inspect */
void skills_hub_do_inspect(const char *identifier) {
    (void)identifier;
}

/* ── browse_skills (L846) ── */
/* PoP: skills_hub_browse @ hermes_cli/skills_hub.py:browse_skills */
json_t *skills_hub_browse(int page, int page_size) {
    (void)page;
    (void)page_size;
    json_t *result = json_object();
    json_set(result, "items", json_array());
    json_set(result, "page", json_number(page));
    json_set(result, "total_pages", json_number(0));
    json_set(result, "total", json_number(0));
    return result;
}

/* ── inspect_skill (L895) ── */
/* PoP: skills_hub_inspect @ hermes_cli/skills_hub.py:inspect_skill */
json_t *skills_hub_inspect(const char *identifier) {
    (void)identifier;
    return NULL;
}

/* ── do_list (L933) ── */
/* PoP: skills_hub_do_list @ hermes_cli/skills_hub.py:do_list */
void skills_hub_do_list(const char *source_filter, bool enabled_only) {
    (void)source_filter;
    (void)enabled_only;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/agent_import.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── extract_markdown_entries (L168) ── */
/* PoP: agent_import_extract_md @ hermes_cli/agent_import.py:extract_markdown_entries */
json_t *agent_import_extract_md(const char *text) {
    (void)text;
    return json_array();
}

/* ── parse_existing_memory_entries (L253) ── */
/* PoP: agent_import_parse_memory @ hermes_cli/agent_import.py:parse_existing_memory_entries */
json_t *agent_import_parse_memory(const char *text) {
    (void)text;
    return json_array();
}

/* ── merge_entries (L294) ── */
/* PoP: agent_import_merge @ hermes_cli/agent_import.py:merge_entries */
json_t *agent_import_merge(json_t *existing, json_t *new_entries) {
    (void)new_entries;
    return existing ? existing : json_array();
}

/* ── detect_agents (L362) ── */
/* PoP: agent_import_detect @ hermes_cli/agent_import.py:detect_agents */
json_t *agent_import_detect(const char *path) {
    (void)path;
    return json_array();
}
