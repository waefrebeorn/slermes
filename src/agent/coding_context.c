/*
 * coding_context.c — Coding-context awareness (B09).
 *
 * Port of Python agent/coding_context.py (731 lines, ~30 functions/classes).
 * Implements the portable subset: workspace detection, profile resolution,
 * edit-format guidance, and RuntimeMode construction.
 * Python-only constructs (dataclasses, config dict access, Path objects)
 * are omitted — C gets config via hermes_config_t.
 *
 * Port of Python agent/coding_context.py:_model_family() — consolidated in coding_context_model_family()
 * Port of Python agent/coding_context.py:_edit_format_line() — consolidated in coding_context_edit_format_line()
 * Port of Python agent/coding_context.py:_coding_mode() — consolidated in coding_context_resolve_mode()
 * Port of Python agent/coding_context.py:_resolve_cwd() — consolidated in coding_context_resolve_cwd()
 * Port of Python agent/coding_context.py:_git_root() — consolidated in coding_context_find_git_root()
 * Port of Python agent/coding_context.py:_home() — consolidated in coding_context_get_home()
 * Port of Python agent/coding_context.py:_marker_root() — consolidated in coding_context_find_marker_root()
 * Port of Python agent/coding_context.py:_detect_profile_name() — consolidated in coding_context_detect_profile_name()
 * Port of Python agent/coding_context.py:get_profile() — consolidated in coding_context_get_profile()
 * Port of Python agent/coding_context.py:RuntimeMode.kind — consolidated in coding_runtime_mode_kind()
 * Port of Python agent/coding_context.py:RuntimeMode.is_coding — consolidated in coding_runtime_mode_is_coding()
 * Port of Python agent/coding_context.py:RuntimeMode.toolset_selection() — consolidated in coding_runtime_mode_toolset_selection()
 * Port of Python agent/coding_context.py:RuntimeMode.system_blocks() — consolidated in coding_runtime_mode_system_blocks()
 * Port of Python agent/coding_context.py:RuntimeMode.compact_skill_categories() — consolidated in coding_runtime_mode_compact_skill_categories()
 * Port of Python agent/coding_context.py:RuntimeMode.profile — consolidated in coding_runtime_mode_profile()
 * Port of Python agent/coding_context.py:CODING_AGENT_GUIDANCE — ported as constant
 * Port of Python agent/coding_context.py:GENERAL_PROFILE/CODING_PROFILE — ported as static instances
 * Port of Python agent/coding_context.py:is_coding_context — consolidated in coding_context_detect_profile_name()
 * Port of Python agent/coding_context.py:coding_selection — coding_context_coding_selection()
 * Port of Python agent/coding_context.py:coding_system_blocks — coding_context_coding_system_blocks()
 * Port of Python agent/coding_context.py:coding_compact_skill_categories — coding_context_coding_compact_skill_categories()
 * Port of Python agent/coding_context.py:_enabled_mcp_servers — coding_context_enabled_mcp_servers()
 * Port of Python agent/coding_context.py:_git — coding_context_run_git()
 * Port of Python agent/coding_context.py:_parse_status — coding_context_parse_git_status()
 * Port of Python agent/coding_context.py:_read_small — coding_context_read_small()
 * Port of Python agent/coding_context.py:_project_facts — coding_context_project_facts()
 * Port of Python agent/coding_context.py:build_coding_workspace_block — ported inline in coding_runtime_mode_system_blocks()
 * Port of Python agent/coding_context.py:resolve_runtime_mode() — consolidated in coding_context_resolve_runtime_mode()
 */

#include "hermes_core_types.h"
#include "hermes_yaml.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <limits.h>

/* Forward declaration of runtime mode struct (defined at line ~526) */
typedef struct coding_runtime_mode_s coding_runtime_mode_t;

/* Forward declaration of coding_context_coding_instructions (defined later) */
char *coding_context_coding_instructions(const hermes_config_t *config);
/* Forward declaration of coding_context_build_workspace_block (defined later) */
char *coding_context_build_workspace_block(const char *cwd);

/* ================================================================== */
/*  Constants (Port of Python module-level constants)                 */
/* ================================================================== */

#define CODING_TOOLSET "coding"

/* Surfaces where a coding posture makes sense under "auto" */
static const char *INTERACTIVE_CODING_PLATFORMS[] = {
    "cli", "tui", "acp", "desktop", "", NULL
};

/* Project-root signals that mark a directory as a code workspace */
static const char *PROJECT_MARKERS[] = {
    "pyproject.toml", "setup.py", "setup.cfg", "requirements.txt",
    "package.json", "tsconfig.json", "deno.json",
    "Cargo.toml", "go.mod", "pom.xml", "build.gradle", "build.gradle.kts",
    "Gemfile", "composer.json", "mix.exs", "pubspec.yaml",
    "CMakeLists.txt", "Makefile", "Dockerfile",
    "AGENTS.md", "CLAUDE.md", ".cursorrules",
    NULL
};

/* Agent-instruction files surfaced separately from manifests in the snapshot */
static const char *CONTEXT_FILES[] = {
    "AGENTS.md", "CLAUDE.md", ".cursorrules",
    NULL
};

/* Lockfile → package manager, checked in priority order */
struct lockfile_map {
    const char *filename;
    const char *manager;
};

/* package.json scripts / Makefile targets worth surfacing as verify commands */
static const char *VERIFY_TARGETS[] = {
    "test", "tests", "lint", "typecheck", "check", "build", "fmt", "format",
    NULL
};

#define MAX_VERIFY_COMMANDS 8
#define MAX_FACT_FILE_BYTES (256 * 1024)
#define GIT_TIMEOUT_SECONDS 2.5

/* Operating brief for the coding posture */
#define CODING_AGENT_GUIDANCE \
    "You are a coding agent pairing with the user inside their codebase. " \
    "Operate like a careful senior engineer.\n" \
    "\n" \
    "Gather context first:\n" \
    "- Read the relevant files with `read_file` and locate code with " \
    "`search_files` before changing anything. Trace a symbol to its definition " \
    "and usages rather than guessing its shape.\n" \
    "- Batch independent lookups: when several reads/searches don't depend on " \
    "each other, issue them together in one turn instead of one at a time.\n" \
    "- Never invent files, symbols, APIs, or imports. If you haven't seen it in " \
    "the repo, go look. Don't assume a library is available — check the project " \
    "manifest (pyproject.toml / package.json / Cargo.toml / go.mod) and how " \
    "neighbouring files import it.\n" \
    "\n" \
    "Make changes through the tools, not the chat:\n" \
    "- Edit with `patch`/`write_file`. Do NOT print code blocks to the user as " \
    "a substitute for editing — apply the change, then summarise it. Only show " \
    "code when the user explicitly asks to see it.\n" \
    "- Match the project's existing style and conventions; AGENTS.md / " \
    "CLAUDE.md / .cursorrules already in context win over your defaults. Touch " \
    "only what the task needs — no drive-by refactors, renames, or reformatting " \
    "— and add any imports/dependencies your code requires.\n" \
    "- If an edit fails to apply, re-read the file to get the current exact " \
    "contents before retrying — don't repeat a stale patch. If the same region " \
    "fails twice, rewrite the enclosing function or file with `write_file` " \
    "instead of attempting a third patch.\n" \
    "\n" \
    "Verify, and know when to stop:\n" \
    "- Use `terminal` for git, builds, tests, and inspection. Run the relevant " \
    "tests/linter/build and confirm they pass before claiming the work is done.\n" \
    "- Terminal state persists across calls: current directory and exported " \
    "environment variables carry forward. Activate a virtualenv or export setup " \
    "vars once, then reuse that state instead of re-sourcing it before every " \
    "test command.\n" \
    "- Fix root causes, not symptoms: when you find a bug, check sibling call " \
    "paths for the same flaw and fix the class, not just the reported site.\n" \
    "- When fixing linter/type errors on a file, stop after about three " \
    "attempts on the same file and ask the user rather than looping.\n" \
    "- Track multi-step work with `todo`. Reference code as `path:line` instead " \
    "of pasting whole files.\n" \
    "\n" \
    "Respect the user's repo: don't commit, push, or rewrite history unless " \
    "asked, and never read, print, or commit secrets — leave `.env` and " \
    "credential files alone unless the user explicitly asks. The Workspace " \
    "block below is a snapshot from session start — re-run `git status`/" \
    "`git branch` before relying on it. Be concise: lead with the change or " \
    "answer, not a preamble."

/* Per-model edit-format steering (Port of Python _EDIT_FORMAT_GUIDANCE) */
struct edit_format_family {
    const char **needles;
    const char *guidance_line;
};

static const char *PATCH_NEEDLES[] = {
    "gpt", "codex", NULL
};
#define PATCH_GUIDANCE \
    "- Edit format: author new files with `write_file`; for edits to " \
    "existing code use `patch` with `mode='patch'` (V4A diff) — including " \
    "single-file edits. It's the edit format you handle most reliably."

static const char *REPLACE_NEEDLES[] = {
    "claude", "sonnet", "opus", "haiku",
    "gemini", "gemma", "deepseek", "qwen", "kimi", "glm", "grok",
    "hermes", "llama", "mistral", "devstral", "minimax", NULL
};
#define REPLACE_GUIDANCE \
    "- Edit format: author new files with `write_file`; for edits to " \
    "existing code prefer `patch` in `mode='replace'` — match a unique " \
    "snippet and swap it. Reach for `mode='patch'` (V4A) only when an edit " \
    "genuinely spans several files at once."

static const struct edit_format_family EDIT_FORMAT_FAMILIES[] = {
    {PATCH_NEEDLES, PATCH_GUIDANCE},
    {REPLACE_NEEDLES, REPLACE_GUIDANCE},
    {NULL, NULL}
};

/* Non-coding skill categories (demoted to names-only under focus mode) */
static const char *NON_CODING_SKILL_CATEGORIES[] = {
    "apple", "communication", "cooking", "creative", "email", "finance",
    "gaming", "gifs", "health", "media", "music", "note-taking",
    "productivity", "shopping", "smart-home", "social-media", "travel",
    "yuanbao",
    NULL
};

/* ================================================================== */
/*  ContextProfile struct (Port of Python dataclass)                  */
/* ================================================================== */

typedef struct {
    const char *name;
    const char *toolset;                 /* toolset to collapse to, or NULL */
    const char *guidance;                /* operating brief, or empty string */
    const char *model_hint;              /* model routing hint, or NULL */
    const char *memory_policy;           /* memory namespace/weighting hint */
    const char **compact_skill_cats;     /* categories to demote in focus mode */
} coding_context_profile_t;

/* Global profile instances (Port of Python GENERAL_PROFILE, CODING_PROFILE) */
static const coding_context_profile_t GENERAL_PROFILE = {
    .name = "general",
    .toolset = NULL,
    .guidance = "",
    .model_hint = NULL,
    .memory_policy = "default",
    .compact_skill_cats = NULL
};

static const coding_context_profile_t CODING_PROFILE = {
    .name = "coding",
    .toolset = CODING_TOOLSET,
    .guidance = CODING_AGENT_GUIDANCE,
    .model_hint = "coding",
    .memory_policy = "project",
    .compact_skill_cats = NON_CODING_SKILL_CATEGORIES
};

/* Profile lookup table */
static const coding_context_profile_t *PROFILES[] = {
    &GENERAL_PROFILE,
    &CODING_PROFILE,
    NULL
};

/* ================================================================== */
/*  Helpers                                                           */
/* ================================================================== */

static bool str_in_list(const char *s, const char **list) {
    if (!s || !list) return false;
    for (int i = 0; list[i]; i++) {
        if (strcmp(s, list[i]) == 0) return true;
    }
    return false;
}

static char *str_dup_lower(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++)
        out[i] = (char)tolower((unsigned char)s[i]);
    out[len] = '\0';
    return out;
}

static void path_dirname(const char *path, char *out, size_t out_size) {
    if (!path || !out || out_size == 0) return;
    char *copy = strdup(path);
    if (!copy) { out[0] = '\0'; return; }
    char *d = dirname(copy);
    snprintf(out, out_size, "%s", d);
    free(copy);
}

/* PoP: file_exists @ agent/pet/store.py:exists */
static bool file_exists(const char *path) {
    if (!path) return false;
    struct stat st;
    return stat(path, &st) == 0;
}

static bool dir_exists(const char *path) {
    if (!path) return false;
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* ================================================================== */
/*  Profile lookup                                                    */
/* ================================================================== */

/* AG26: Port of Python agent/coding_context.py:get_profile() */
const coding_context_profile_t *coding_context_get_profile(const char *name) {
    if (!name) return &GENERAL_PROFILE;
    for (int i = 0; PROFILES[i]; i++) {
        if (strcmp(PROFILES[i]->name, name) == 0)
            return PROFILES[i];
    }
    return &GENERAL_PROFILE;
}

/* ================================================================== */
/*  Config mode resolution (Port of Python _coding_mode)              */
/* ================================================================== */

/* AG26: Port of Python agent/coding_context.py:_coding_mode() */
const char *coding_context_resolve_mode(const hermes_config_t *config) {
    const char *raw = "auto";
    if (config && config->agent.coding_context[0])
        raw = config->agent.coding_context;

    char *mode = str_dup_lower(raw);
    if (!mode) return "auto";

    if (strcmp(mode, "focus") == 0 || strcmp(mode, "strict") == 0 || strcmp(mode, "lean") == 0) {
        free(mode);
        return "focus";
    }
    if (strcmp(mode, "on") == 0 || strcmp(mode, "true") == 0 ||
        strcmp(mode, "yes") == 0 || strcmp(mode, "1") == 0 || strcmp(mode, "always") == 0) {
        free(mode);
        return "on";
    }
    if (strcmp(mode, "off") == 0 || strcmp(mode, "false") == 0 ||
        strcmp(mode, "no") == 0 || strcmp(mode, "0") == 0 || strcmp(mode, "never") == 0) {
        free(mode);
        return "off";
    }
    free(mode);
    return "auto";
}

/* ================================================================== */
/*  CWD resolution (Port of Python _resolve_cwd)                      */
/* ================================================================== */

/* AG26: Port of Python agent/coding_context.py:_resolve_cwd() */
void coding_context_resolve_cwd(const hermes_config_t *config, char *out, size_t out_size) {
    if (config && config->agent.agent_cwd[0]) {
        snprintf(out, out_size, "%s", config->agent.agent_cwd);
        return;
    }
    if (getcwd(out, out_size) == NULL)
        out[0] = '\0';
}

/* ================================================================== */
/*  Git root detection (Port of Python _git_root)                     */
/* ================================================================== */

/* AG26: Port of Python agent/coding_context.py:_git_root() */
bool coding_context_find_git_root(const char *cwd, char *out, size_t out_size) {
    if (!cwd || !cwd[0]) return false;

    char current[PATH_MAX];
    snprintf(current, sizeof(current), "%s", cwd);

    char *resolved = realpath(current, NULL);
    if (!resolved) return false;

    char *p = resolved;
    while (p && p[0]) {
        char git_path[PATH_MAX];
        snprintf(git_path, sizeof(git_path), "%s/.git", p);
        if (file_exists(git_path)) {
            snprintf(out, out_size, "%s", p);
            free(resolved);
            return true;
        }
        char parent[PATH_MAX];
        path_dirname(p, parent, sizeof(parent));
        if (strcmp(parent, p) == 0) break;
        strncpy(p, parent, PATH_MAX - 1);
        p[PATH_MAX - 1] = '\0';
    }

    free(resolved);
    return false;
}

/* ================================================================== */
/*  Home directory (Port of Python _home)                             */
/* ================================================================== */

/* AG26: Port of Python agent/coding_context.py:_home() */
bool coding_context_get_home(char *out, size_t out_size) {
    const char *home = getenv("HOME");
    if (!home) return false;
    char *resolved = realpath(home, NULL);
    if (!resolved) return false;
    snprintf(out, out_size, "%s", resolved);
    free(resolved);
    return true;
}

/* ================================================================== */
/*  Project marker root (Port of Python _marker_root)                 */
/* ================================================================== */

/* AG26: Port of Python agent/coding_context.py:_marker_root() */
bool coding_context_find_marker_root(const char *cwd, char *out, size_t out_size) {
    if (!cwd || !cwd[0]) return false;

    char current[PATH_MAX];
    snprintf(current, sizeof(current), "%s", cwd);

    char *resolved = realpath(current, NULL);
    if (!resolved) return false;

    char home[PATH_MAX] = {0};
    coding_context_get_home(home, sizeof(home));

    char *p = resolved;
    for (int depth = 0; depth <= 6 && p && p[0]; depth++) {
        if (home[0] && strcmp(p, home) == 0) {
            char parent[PATH_MAX];
            path_dirname(p, parent, sizeof(parent));
            if (strcmp(parent, p) == 0) break;
            strncpy(p, parent, PATH_MAX - 1);
            p[PATH_MAX - 1] = '\0';
            continue;
        }

        for (int i = 0; PROJECT_MARKERS[i]; i++) {
            char marker_path[PATH_MAX];
            snprintf(marker_path, sizeof(marker_path), "%s/%s", p, PROJECT_MARKERS[i]);
            if (file_exists(marker_path)) {
                snprintf(out, out_size, "%s", p);
                free(resolved);
                return true;
            }
        }

        char parent[PATH_MAX];
        path_dirname(p, parent, sizeof(parent));
        if (strcmp(parent, p) == 0) break;
        strncpy(p, parent, PATH_MAX - 1);
        p[PATH_MAX - 1] = '\0';
    }

    free(resolved);
    return false;
}

/* ================================================================== */
/*  Profile name detection (Port of Python _detect_profile_name)      */
/* ================================================================== */

/* AG26: Port of Python agent/coding_context.py:_detect_profile_name() */
/* AG26: Port of Python agent/coding_context.py:is_coding_context() */
const char *coding_context_detect_profile_name(
    const char *mode, const char *platform, const char *cwd) {

    if (strcmp(mode, "off") == 0)
        return GENERAL_PROFILE.name;
    if (strcmp(mode, "on") == 0)
        return CODING_PROFILE.name;

    if (platform && platform[0]) {
        char *plat_lower = str_dup_lower(platform);
        if (plat_lower && !str_in_list(plat_lower, INTERACTIVE_CODING_PLATFORMS)) {
            free(plat_lower);
            return GENERAL_PROFILE.name;
        }
        free(plat_lower);
    }

    /* Check for git repo */
    char git_root[PATH_MAX];
    if (coding_context_find_git_root(cwd, git_root, sizeof(git_root))) {
        char home[PATH_MAX];
        if (coding_context_get_home(home, sizeof(home)) && strcmp(git_root, home) == 0) {
            /* Git repo at $HOME — not a code workspace (dotfiles) */
        } else {
            return CODING_PROFILE.name;
        }
    }

    /* Check for project markers */
    char marker_root[PATH_MAX];
    if (coding_context_find_marker_root(cwd, marker_root, sizeof(marker_root))) {
        return CODING_PROFILE.name;
    }

    return GENERAL_PROFILE.name;
}

/* ================================================================== */
/*  Model family classification (Port of Python _model_family)        */
/* ================================================================== */

/* AG26: Port of Python agent/coding_context.py:_model_family() */
const char *coding_context_model_family(const char *model) {
    if (!model || !model[0]) return NULL;

    char *lowered = str_dup_lower(model);
    if (!lowered) return NULL;

    for (int f = 0; EDIT_FORMAT_FAMILIES[f].needles; f++) {
        for (int n = 0; EDIT_FORMAT_FAMILIES[f].needles[n]; n++) {
            if (strstr(lowered, EDIT_FORMAT_FAMILIES[f].needles[n])) {
                const char *family = (f == 0) ? "patch" : "replace";
                free(lowered);
                return family;
            }
        }
    }

    free(lowered);
    return NULL;
}

/* ================================================================== */
/*  Edit format guidance line (Port of Python _edit_format_line)      */
/* ================================================================== */

/* AG26: Port of Python agent/coding_context.py:_edit_format_line() */
const char *coding_context_edit_format_line(const char *model) {
    const char *family = coding_context_model_family(model);
    if (!family) return "";

    for (int f = 0; EDIT_FORMAT_FAMILIES[f].needles; f++) {
        const char *fam_name = (f == 0) ? "patch" : "replace";
        if (strcmp(family, fam_name) == 0)
            return EDIT_FORMAT_FAMILIES[f].guidance_line;
    }
    return "";
}

/* ================================================================== */
/*  RuntimeMode struct (Port of Python dataclass)                     */
/* ================================================================== */

struct coding_runtime_mode_s {
    const coding_context_profile_t *profile;
    char surface[64];
    char cwd[PATH_MAX];
    char config_mode[16];
    char model[128];
    const hermes_config_t *config;
};

coding_runtime_mode_t *coding_runtime_mode_create(
    const char *platform,
    const char *cwd,
    const hermes_config_t *config,
    const char *model) {

    coding_runtime_mode_t *mode = calloc(1, sizeof(coding_runtime_mode_t));
    if (!mode) return NULL;

    /* Resolve cwd */
    char resolved_cwd[PATH_MAX];
    coding_context_resolve_cwd(config, resolved_cwd, sizeof(resolved_cwd));
    snprintf(mode->cwd, sizeof(mode->cwd), "%s", resolved_cwd);

    /* Resolve config mode */
    const char *cfg_mode = coding_context_resolve_mode(config);
    snprintf(mode->config_mode, sizeof(mode->config_mode), "%s", cfg_mode);

    /* Detect profile name */
    const char *profile_name = coding_context_detect_profile_name(cfg_mode, platform, resolved_cwd);
    mode->profile = coding_context_get_profile(profile_name);

    /* Surface */
    if (platform && platform[0])
        snprintf(mode->surface, sizeof(mode->surface), "%s", platform);

    /* Model */
    if (model && model[0])
        snprintf(mode->model, sizeof(mode->model), "%s", model);
    mode->config = config;

    return mode;
}

void coding_runtime_mode_destroy(coding_runtime_mode_t *mode) {
    if (mode) free(mode);
}

/* AG26: Port of Python agent/coding_context.py:is_coding() */
bool coding_runtime_mode_is_coding(const coding_runtime_mode_t *mode) {
    return mode && mode->profile && strcmp(mode->profile->name, "coding") == 0;
}

/* AG26: Port of Python agent/coding_context.py:profile() */
const coding_context_profile_t *coding_runtime_mode_profile(const coding_runtime_mode_t *mode) {
    return mode ? mode->profile : &GENERAL_PROFILE;
}

/* AG26: Port of Python agent/coding_context.py:kind() */
const char *coding_runtime_mode_kind(const coding_runtime_mode_t *mode) {
    return mode && mode->profile ? mode->profile->name : "general";
}

const char *coding_runtime_mode_surface(const coding_runtime_mode_t *mode) {
    return mode ? mode->surface : "";
}

const char *coding_runtime_mode_cwd(const coding_runtime_mode_t *mode) {
    return mode ? mode->cwd : "";
}

const char *coding_runtime_mode_config_mode(const coding_runtime_mode_t *mode) {
    return mode ? mode->config_mode : "auto";
}

const char *coding_runtime_mode_model(const coding_runtime_mode_t *mode) {
    return mode ? mode->model : "";
}

/* AG26: Port of Python agent/coding_context.py:toolset_selection() */
const char **coding_runtime_mode_toolset_selection(
    const coding_runtime_mode_t *mode, const hermes_config_t *config) {
    static const char *result[2];

    if (!mode || strcmp(mode->config_mode, "focus") != 0)
        return NULL;
    if (!mode->profile || !mode->profile->toolset)
        return NULL;

    result[0] = mode->profile->toolset;
    result[1] = NULL; /* MCP servers would be added here if implemented */

    return result;
}

/* AG26: Port of Python agent/coding_context.py:system_prompt_parts()
 *       and agent/coding_context.py:system_prompt_parts():
 *       system_prompt_parts() returns prefix/workspace/trailing blocks,
 *       both implemented via coding_runtime_mode_system_blocks().
 * PoP: system_prompt_parts @ agent/coding_context.py:system_prompt_parts
 * PoP: system_blocks @ agent/coding_context.py:system_blocks
 * PoP: coding_system_prompt_parts @ agent/coding_context.py:coding_system_prompt_parts
 */
char **coding_runtime_mode_system_blocks(
    const coding_runtime_mode_t *mode, int *out_count) {

    if (!mode || !coding_runtime_mode_is_coding(mode)) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    /* Allocate array for up to 3 blocks (prefix/workspace/trailing) */
    char **blocks = calloc(3, sizeof(char *));
    if (!blocks) { if (out_count) *out_count = 0; return NULL; }

    int count = 0;

    /* Prefix: operating brief with optional edit-format guidance */
    if (mode->profile->guidance && mode->profile->guidance[0]) {
        const char *edit_line = coding_context_edit_format_line(mode->model);
        if (edit_line && edit_line[0]) {
            size_t brief_len = strlen(mode->profile->guidance) + strlen(edit_line) + 2;
            blocks[count] = malloc(brief_len);
            if (blocks[count]) {
                snprintf(blocks[count], brief_len, "%s\n%s", mode->profile->guidance, edit_line);
                count++;
            }
        } else {
            blocks[count] = strdup(mode->profile->guidance);
            if (blocks[count]) count++;
        }
    }

    /* Workspace block */
    {
        char *workspace = coding_context_build_workspace_block(mode->cwd);
        if (workspace && workspace[0]) {
            blocks[count++] = workspace;
        } else {
            free(workspace);
        }
    }

    /* Trailing: operator instructions from config */
    {
        char *instructions = coding_context_coding_instructions(mode->config);
        if (instructions && instructions[0]) {
            size_t ilen = strlen(instructions) + 28;
            blocks[count] = malloc(ilen);
            if (blocks[count]) {
                snprintf(blocks[count], ilen, "Operator instructions (from config):\n%s", instructions);
                count++;
            }
        }
        free(instructions);
    }

    if (out_count) *out_count = count;
    return blocks;
}

/* Free system blocks array */
void coding_runtime_mode_free_blocks(char **blocks, int count) {
    if (!blocks) return;
    for (int i = 0; i < count; i++) free(blocks[i]);
    free(blocks);
}

/* AG26: Port of Python agent/coding_context.py:compact_skill_categories() */
const char **coding_runtime_mode_compact_skill_categories(const coding_runtime_mode_t *mode) {
    if (!mode || !coding_runtime_mode_is_coding(mode) ||
        strcmp(mode->config_mode, "focus") != 0)
        return NULL;
    return mode->profile->compact_skill_cats;
}

/* ================================================================== */
/*  Main entry point (Port of Python resolve_runtime_mode)            */
/* ================================================================== */

/* AG26: Port of Python agent/coding_context.py:resolve_runtime_mode() */
coding_runtime_mode_t *coding_context_resolve_runtime_mode(
    const char *platform,
    const char *cwd,
    const hermes_config_t *config,
    const char *model) {

    return coding_runtime_mode_create(platform, cwd, config, model);
}

/* ================================================================== */
/*  Git / workspace probe (Port of Python _git / _parse_status /      */
/*  _read_small / _project_facts / build_coding_workspace_block)     */
/* ================================================================== */

#include <stdarg.h>
#include <signal.h>
#include <dirent.h>


/*
 * PoP: _git @ agent/coding_context.py:_git
 * Runs `git -C <cwd> <args...>` and returns the stripped stdout in out (or ""
 * on failure). Timeout-guarded via SIGALRM (mirrors Python subprocess timeout). */
int coding_context_run_git(const char *cwd, char *out, size_t out_size,
                           const char *fmt, ...)
{
    if (out && out_size) out[0] = '\0';
    if (!cwd || !cwd[0] || !fmt) return -1;

    va_list ap;
    va_start(ap, fmt);
    char args[1024];
    vsnprintf(args, sizeof(args), fmt, ap);
    va_end(ap);

    /* vsnprintf consumes %h/%s as float/str from varargs → use literal "git" binary
     * and pass args via shell so git receives the exact --pretty= format string. */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "git -C %s %s", cwd, args);

    /* timeout guard */
    void (*old)(int) = signal(SIGALRM, SIG_DFL);
    alarm((unsigned)GIT_TIMEOUT_SECONDS);
    FILE *fp = popen(cmd, "r");
    if (!fp) { alarm(0); signal(SIGALRM, old); return -1; }
    if (out && out_size) {
        size_t got = fread(out, 1, out_size - 1, fp);
        out[got] = '\0';
        /* strip trailing newline(s) */
        while (got > 0 && (out[got-1] == '\n' || out[got-1] == '\r')) out[--got] = '\0';
    }
    int rc = pclose(fp);
    alarm(0);
    signal(SIGALRM, old);
    return (rc == 0) ? 0 : -1;
}

/*
 * PoP: _parse_status @ agent/coding_context.py:_parse_status
 * Parses `git status --porcelain=2 --branch` into a malloc'd JSON object:
 *   { "branch": {"head":..,"upstream":..,"ahead":..,"behind":..},
 *     "counts": {"staged":n,"modified":n,"untracked":n,"conflicts":n} }
 * Caller frees the returned string (NULL on empty input). */
char *coding_context_parse_git_status(const char *porcelain)
{
    if (!porcelain || !*porcelain) return NULL;
    char *branch_head = NULL, *branch_upstream = NULL, *ahead = NULL, *behind = NULL;
    int staged = 0, modified = 0, untracked = 0, conflicts = 0;

    const char *p = porcelain;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t linelen = nl ? (size_t)(nl - p) : strlen(p);
        char line[1024];
        if (linelen >= sizeof(line)) linelen = sizeof(line) - 1;
        memcpy(line, p, linelen); line[linelen] = '\0';

        if (strncmp(line, "# branch.head ", 14) == 0) {
            branch_head = strdup(line + 14);
        } else if (strncmp(line, "# branch.upstream ", 18) == 0) {
            branch_upstream = strdup(line + 18);
        } else if (strncmp(line, "# branch.ab ", 12) == 0) {
            /* "# branch.ab +N -M" */
            const char *q = line + 12;
            while (*q == ' ') q++;
            if (*q == '+') {
                ahead = strdup(q + 1);
                char *sp = strchr(ahead, ' '); if (sp) *sp = '\0';
                while (*q && *q != ' ') q++;
            }
            while (*q == ' ') q++;
            if (*q == '-') {
                behind = strdup(q + 1);
                char *sp = strchr(behind, ' '); if (sp) *sp = '\0';
            }
        } else if (strncmp(line, "1 ", 2) == 0 || strncmp(line, "2 ", 2) == 0) {
            /* "1 XY ..." — xy is the 2nd field */
            const char *q = line;
            /* skip "1 " */
            q += 2;
            while (*q == ' ') q++;
            if (q[0] && q[0] != '.') staged++;
            if (q[1] && q[1] != '.') modified++;
        } else if (strncmp(line, "u ", 2) == 0) {
            conflicts++;
        } else if (strncmp(line, "? ", 2) == 0) {
            untracked++;
        }

        if (!nl) break;
        p = nl + 1;
    }

    json_t *b = json_object();
    json_set(b, "head", json_string(branch_head ? branch_head : ""));
    json_set(b, "upstream", json_string(branch_upstream ? branch_upstream : ""));
    json_set(b, "ahead", json_string(ahead ? ahead : "0"));
    json_set(b, "behind", json_string(behind ? behind : "0"));
    json_t *c = json_object();
    json_set(c, "staged", json_number((double)staged));
    json_set(c, "modified", json_number((double)modified));
    json_set(c, "untracked", json_number((double)untracked));
    json_set(c, "conflicts", json_number((double)conflicts));
    json_t *root = json_object();
    json_set(root, "branch", b);
    json_set(root, "counts", c);
    char *out = json_serialize(root);
    json_free(root);
    free(branch_head); free(branch_upstream); free(ahead); free(behind);
    return out;
}

/*
 * PoP: _read_small @ agent/coding_context.py:_read_small
 * Reads a small text file into out (or "" on any failure / oversize). */
int coding_context_read_small(const char *path, char *out, size_t out_size, size_t max_bytes)
{
    if (out && out_size) out[0] = '\0';
    if (!path || !path[0]) return -1;
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    if (!S_ISREG(st.st_mode)) return -1;
    if ((size_t)st.st_size > max_bytes) return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    size_t n = fread(out, 1, out_size - 1, fp);
    out[n] = '\0';
    fclose(fp);
    return 0;
}

/*
 * PoP: detect_project_facts @ agent/coding_context.py:detect_project_facts
 * PoP: _project_facts @ agent/coding_context.py:_project_facts
 * Port of Python agent/coding_context.py:detect_project_facts /
 * agent/coding_context.py:_project_facts
 * Detects manifests, package managers, verify commands, context files for a
 * root and renders them as workspace-snapshot lines into out (malloc'd, caller frees). */
char *coding_context_project_facts(const char *root)
{
    if (!root || !root[0]) return NULL;
    char line[2048];
    size_t cap = 1024;
    char *out = malloc(cap);
    out[0] = '\0';
    size_t len = 0;

    /* manifests (excluding context files) */
    int nm = 0;
    char manifests[256]; manifests[0] = '\0';
    for (int i = 0; PROJECT_MARKERS[i]; i++) {
        const char *m = PROJECT_MARKERS[i];
        int is_ctx = 0;
        for (int j = 0; CONTEXT_FILES[j]; j++) if (strcmp(m, CONTEXT_FILES[j]) == 0) { is_ctx = 1; break; }
        if (is_ctx) continue;
        char p[PATH_MAX]; snprintf(p, sizeof(p), "%s/%s", root, m);
        if (file_exists(p)) {
            if (nm) strncat(manifests, ", ", sizeof(manifests) - strlen(manifests) - 1);
            strncat(manifests, m, sizeof(manifests) - strlen(manifests) - 1);
            nm++;
        }
    }
    /* package managers */
    char pms[128]; pms[0] = '\0'; int npm = 0;
    struct { const char *lock; const char *pm; } lockfiles[] = {
        {"uv.lock","uv"},{"poetry.lock","poetry"},{"Pipfile.lock","pipenv"},
        {"pnpm-lock.yaml","pnpm"},{"bun.lockb","bun"},{"bun.lock","bun"},
        {"yarn.lock","yarn"},{"package-lock.json","npm"},{NULL,NULL}
    };
    char seen_pm[256]; seen_pm[0]='\0';
    for (int i = 0; lockfiles[i].lock; i++) {
        char p[PATH_MAX]; snprintf(p, sizeof(p), "%s/%s", root, lockfiles[i].lock);
        if (file_exists(p)) {
            if (strstr(seen_pm, lockfiles[i].pm)) continue;
            strncat(seen_pm, lockfiles[i].pm, sizeof(seen_pm)-1);
            strncat(seen_pm, "|", sizeof(seen_pm)-1);
            if (npm) strncat(pms, "/", sizeof(pms)-1);
            strncat(pms, lockfiles[i].pm, sizeof(pms)-1);
            npm++;
        }
    }

    if (nm) {
        len += (size_t)snprintf(line, sizeof(line), "- Project: %s%s%s",
            manifests, npm ? " (" : "", npm ? pms : "", npm ? ")" : "");
        size_t need = len + strlen(line) + 2;
        if (need >= cap) { cap = need + 64; char *nb = realloc(out, cap); if(!nb){free(out);return NULL;} out = nb; }
        strcat(out, line); strcat(out, "\n");
    }

    /* verify commands */
    char verify[1024]; verify[0]='\0';
    {
        char p[PATH_MAX]; snprintf(p, sizeof(p), "%s/scripts/run_tests.sh", root);
        if (file_exists(p)) strncat(verify, "scripts/run_tests.sh; ", sizeof(verify)-1);
        /* package.json scripts */
        char pkg[256*1024];
        char pjp[PATH_MAX]; snprintf(pjp, sizeof(pjp), "%s/package.json", root);
        if (coding_context_read_small(pjp, pkg, sizeof(pkg), MAX_FACT_FILE_BYTES) == 0) {
            json_t *pj = json_parse(pkg, NULL);
            if (pj && pj->type == JSON_OBJECT) {
                json_t *scripts = json_object_get(pj, "scripts");
                if (scripts && scripts->type == JSON_OBJECT) {
                    const char *js_pm = "npm";
                    for (int i = 0; lockfiles[i].lock; i++) {
                        char lp[PATH_MAX]; snprintf(lp, sizeof(lp), "%s/%s", root, lockfiles[i].lock);
                        if (file_exists(lp)) { js_pm = lockfiles[i].pm; break; }
                    }
                    size_t nk = json_object_size(scripts);
                    for (size_t i = 0; i < nk; i++) {
                        const char *name = json_object_get_key_at(scripts, i);
                        if (!name) continue;
                        for (int t = 0; VERIFY_TARGETS[t]; t++) {
                            if (strcmp(name, VERIFY_TARGETS[t]) == 0) {
                                strncat(verify, js_pm, sizeof(verify)-1);
                                strncat(verify, " run ", sizeof(verify)-1);
                                strncat(verify, name, sizeof(verify)-1);
                                strncat(verify, "; ", sizeof(verify)-1);
                                break;
                            }
                        }
                    }
                }
                json_free(pj);
            }
        }
        /* pyproject pytest */
        char pp[256*1024];
        char pyp[PATH_MAX]; snprintf(pyp, sizeof(pyp), "%s/pyproject.toml", root);
        if (coding_context_read_small(pyp, pp, sizeof(pp), MAX_FACT_FILE_BYTES) == 0) {
            if (strstr(pp, "pytest.ini") || strstr(pp, "[tool.pytest")) strncat(verify, "pytest; ", sizeof(verify)-1);
        }
        /* Makefile targets */
        char mk[256*1024];
        char mkp[PATH_MAX]; snprintf(mkp, sizeof(mkp), "%s/Makefile", root);
        if (coding_context_read_small(mkp, mk, sizeof(mk), MAX_FACT_FILE_BYTES) == 0) {
            for (int t = 0; VERIFY_TARGETS[t]; t++) {
                char pat[128]; snprintf(pat, sizeof(pat), "\n%s:", VERIFY_TARGETS[t]);
                if (strstr(mk, pat)) { strncat(verify, "make ", sizeof(verify)-1); strncat(verify, VERIFY_TARGETS[t], sizeof(verify)-1); strncat(verify, "; ", sizeof(verify)-1); }
            }
        }
    }
    if (verify[0]) {
        size_t vlen = strlen(verify);
        if (vlen >= 2 && verify[vlen-2]==';' && verify[vlen-1]==' ') verify[vlen-2]='\0';
        len = strlen(out);
        size_t need = len + strlen(verify) + 12;
        if (need >= cap) { cap = need + 64; char *nb = realloc(out, cap); if(!nb){free(out);return NULL;} out = nb; }
        snprintf(out + len, cap - len, "- Verify: %s\n", verify);
    }

    /* context files */
    int cf = 0; char ctx[256]; ctx[0]='\0';
    for (int j = 0; CONTEXT_FILES[j]; j++) {
        char p[PATH_MAX]; snprintf(p, sizeof(p), "%s/%s", root, CONTEXT_FILES[j]);
        if (file_exists(p)) {
            if (cf) strncat(ctx, ", ", sizeof(ctx)-1);
            strncat(ctx, CONTEXT_FILES[j], sizeof(ctx)-1);
            cf++;
        }
    }
    if (cf) {
        len = strlen(out);
        size_t need = len + strlen(ctx) + 20;
        if (need >= cap) { cap = need + 64; char *nb = realloc(out, cap); if(!nb){free(out);return NULL;} out = nb; }
        snprintf(out + len, cap - len, "- Context files: %s\n", ctx);
    }

    if (out[0] == '\0') { free(out); return strdup(""); }
    return out;
}

/*
 * PoP: build_coding_workspace_block @ agent/coding_context.py:build_coding_workspace_block
 * Full workspace snapshot (git state + project facts). Returns malloc'd string. */
char *coding_context_build_workspace_block(const char *cwd)
{
    char resolved[PATH_MAX];
    if (!cwd || !cwd[0]) { if (getcwd(resolved, sizeof(resolved)) == NULL) return strdup(""); }
    else snprintf(resolved, sizeof(resolved), "%s", cwd);

    char git_root[PATH_MAX];
    bool has_git = coding_context_find_git_root(resolved, git_root, sizeof(git_root));
    char root[PATH_MAX];
    if (has_git) snprintf(root, sizeof(root), "%s", git_root);
    else if (!coding_context_find_marker_root(resolved, root, sizeof(root))) return strdup("");

    size_t cap = 4096;
    char *out = malloc(cap);
    size_t len = 0;
    len += (size_t)snprintf(out + len, cap - len,
        "Workspace (snapshot at session start — re-check with `git` before acting on it):\n- Root: %s\n", root);

    if (has_git) {
            char buf[4096];
            /* Get branch info and status counts in one git call */
            if (coding_context_run_git(root, buf, sizeof(buf), "status --porcelain=2 --branch") == 0) {
                char *parsed = coding_context_parse_git_status(buf);
                if (parsed) {
                    json_t *p = json_parse(parsed, NULL);
                    free(parsed);
                    if (p && p->type == JSON_OBJECT) {
                        json_t *b = json_object_get(p, "branch");
                        json_t *c = json_object_get(p, "counts");
                        const char *head = b ? json_get_str(b, "head", "") : "";
                        if (head && head[0] && strcmp(head, "(detached)") != 0) {
                            size_t need = len + 256;
                            if (need >= cap) { cap = need+64; char *nb=realloc(out,cap); if(nb) out=nb; }
                            len += (size_t)snprintf(out + len, cap - len, "- Branch: %s", head);
                            const char *up = b ? json_get_str(b, "upstream", "") : "";
                            if (up && up[0]) {
                                const char *a = b ? json_get_str(b, "ahead", "0") : "0";
                                const char *be = b ? json_get_str(b, "behind", "0") : "0";
                                len += (size_t)snprintf(out + len, cap - len, " → %s (ahead %s, behind %s)", up, a, be);
                            }
                            len += (size_t)snprintf(out + len, cap - len, "\n");
                        } else if (head && strcmp(head, "(detached)") == 0) {
                            len += (size_t)snprintf(out + len, cap - len, "- Branch: (detached HEAD)\n");
                        }
                        /* status counts */
                        int s = c ? (int)json_get_num(c, "staged", 0) : 0;
                        int m = c ? (int)json_get_num(c, "modified", 0) : 0;
                        int u = c ? (int)json_get_num(c, "untracked", 0) : 0;
                        int cf2 = c ? (int)json_get_num(c, "conflicts", 0) : 0;
                        char dirty[256]; dirty[0]='\0';
                        struct { int n; const char *l; } ds[] = {{s,"staged"},{m,"modified"},{u,"untracked"},{cf2,"conflicts"},{0,NULL}};
                        int any=0;
                        for (int i=0; ds[i].l; i++) if (ds[i].n>0) { if(any)strcat(dirty,", "); char tmp[32]; snprintf(tmp,sizeof(tmp),"%d %s",ds[i].n,ds[i].l); strcat(dirty,tmp); any=1; }
                        len += (size_t)snprintf(out + len, cap - len, "- Status: %s\n", any?dirty:"clean");
                        json_free(p);
                    }
                }
            }
            /* worktree detection */
            char gdir[1024], cdir[1024];
            if (coding_context_run_git(root, gdir, sizeof(gdir), "rev-parse --git-dir") == 0 &&
                coding_context_run_git(root, cdir, sizeof(cdir), "rev-parse --git-common-dir") == 0) {
                char ga[PATH_MAX], cb[PATH_MAX];
                realpath(gdir, ga); realpath(cdir, cb);
                if (strcmp(ga, cb) != 0) {
                    len += (size_t)snprintf(out + len, cap - len, "- Worktree: linked (git state shared with primary tree)\n");
                }
            }
            /* recent commits */
            if (coding_context_run_git(root, buf, sizeof(buf), "log -3 --pretty='%%h %%s'") == 0 && buf[0]) {
                len += (size_t)snprintf(out + len, cap - len, "- Recent commits:\n");
                char *line = strtok(buf, "\n");
                while (line) {
                    len += (size_t)snprintf(out + len, cap - len, "    %s\n", line);
                    line = strtok(NULL, "\n");
                }
            }
        }

    /* project facts */
    char *facts = coding_context_project_facts(root);
    if (facts && facts[0]) {
        size_t need = len + strlen(facts) + 2;
        if (need >= cap) { cap = need+64; char *nb=realloc(out,cap); if(nb) out=nb; }
        strncat(out, facts, cap - len - 1);
    }
    free(facts);
    return out;
}

/* PoP: coding_selection @ agent/coding_context.py:coding_selection */
const char **coding_context_coding_selection(const char *platform, const char *cwd, const hermes_config_t *config)
{
    coding_runtime_mode_t *mode = coding_context_resolve_runtime_mode(platform, cwd, config, NULL);
    if (!mode) return NULL;
    const char **r = coding_runtime_mode_toolset_selection(mode, config);
    coding_runtime_mode_destroy(mode);
    return r;
}

/* PoP: system_prompt_parts @ agent/coding_context.py:system_prompt_parts */
/* PoP: coding_system_prompt_parts @ agent/coding_context.py:coding_system_prompt_parts */
/* PoP: coding_system_blocks @ agent/coding_context.py:coding_system_blocks */
char **coding_context_coding_system_blocks(const char *platform, const char *cwd, const hermes_config_t *config, const char *model, int *out_count)
{
    coding_runtime_mode_t *mode = coding_context_resolve_runtime_mode(platform, cwd, config, model);
    if (!mode) { if (out_count) *out_count = 0; return NULL; }
    char **r = coding_runtime_mode_system_blocks(mode, out_count);
    coding_runtime_mode_destroy(mode);
    return r;
}

/* PoP: coding_compact_skill_categories @ agent/coding_context.py:coding_compact_skill_categories */
const char **coding_context_coding_compact_skill_categories(const char *platform, const char *cwd, const hermes_config_t *config)
{
    coding_runtime_mode_t *mode = coding_context_resolve_runtime_mode(platform, cwd, config, NULL);
    if (!mode) return NULL;
    const char **r = coding_runtime_mode_compact_skill_categories(mode);
    coding_runtime_mode_destroy(mode);
    return r;
}

/*
 * PoP: _enabled_mcp_servers @ agent/coding_context.py:_enabled_mcp_servers
 * Returns a NULL-terminated malloc'd array of enabled MCP server names read
 * from config.yaml's `mcp_servers` map (a server is enabled unless it sets
 * enabled:false). Caller frees each string and the array. NULL on none/err.
 * Mirrors Python read_raw_config()["mcp_servers"] + _parse_enabled_flag. */
char **coding_context_enabled_mcp_servers(const hermes_config_t *config)
{
    if (!config) return NULL;
    /* Resolve config.yaml path from the loaded config. */
    char yaml_path[HERMES_PATH_MAX];
    if (config->config_path[0]) {
        snprintf(yaml_path, sizeof(yaml_path), "%s", config->config_path);
    } else {
        const char *home = getenv("HOME");
        if (!home) return NULL;
        snprintf(yaml_path, sizeof(yaml_path), "%s/.hermes/config.yaml", home);
    }

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(yaml_path, &err);
    if (err) free(err);
    if (!doc) return NULL;

    size_t nkeys = 0;
    char **keys = yaml_map_keys(doc, "mcp_servers", &nkeys);
    if (!keys || nkeys == 0) { if (keys) free(keys); yaml_free(doc); return NULL; }

    char **out = calloc(nkeys + 1, sizeof(char *));
    size_t n = 0;
    for (size_t i = 0; i < nkeys; i++) {
        char enkey[512];
        snprintf(enkey, sizeof(enkey), "mcp_servers.%s.enabled", keys[i]);
        if (yaml_get_bool(doc, enkey, true)) {
            out[n++] = strdup(keys[i]);
        }
        free(keys[i]);
    }
    free(keys);
    yaml_free(doc);
    if (n == 0) { free(out); return NULL; }
    out[n] = NULL;
    return out;
}

/*
 * PoP: _has_code_files @ agent/coding_context.py:_has_code_files
 * Cheap, bounded check for source files in a repo's top two levels. Scans root
 * and immediate subdirs only, capped at CODE_SCAN_MAX_ENTRIES stats. */
#define CODE_SCAN_MAX_ENTRIES 500
bool coding_context_has_code_files(const char *root)
{
    if (!root || !root[0]) return false;
    static const char *CODE_EXTENSIONS[] = {
        ".py",".pyi",".ipynb",".js",".jsx",".ts",".tsx",".mjs",".cjs",
        ".go",".rs",".java",".kt",".kts",".scala",".rb",".php",".c",".h",
        ".cc",".cpp",".hpp",".cs",".swift",".m",".mm",".dart",".ex",".exs",
        ".lua",".sh",".bash",".zsh",".sql",".vue",".svelte",".r",".jl",
        ".hs",".clj",".erl",".pl", NULL
    };
    static const char *SKIP_DIRS[] = {
        ".git","node_modules","venv",".venv","__pycache__","dist","build",
        "target",".next",".turbo","vendor", NULL
    };

    /* stack of (dir, is_root) — root + one level deep only */
    struct { char path[PATH_MAX]; int is_root; } stack[CODE_SCAN_MAX_ENTRIES];
    int sp = 0;
    snprintf(stack[sp].path, sizeof(stack[sp].path), "%s", root);
    stack[sp].is_root = 1; sp++;

    int seen = 0;
    while (sp > 0) {
        sp--;
        char dirpath[PATH_MAX];
        snprintf(dirpath, sizeof(dirpath), "%s", stack[sp].path);
        int is_root = stack[sp].is_root;
        DIR *d = opendir(dirpath);
        if (!d) continue;
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            seen++;
            if (seen > CODE_SCAN_MAX_ENTRIES) { closedir(d); return false; }
            char full[PATH_MAX];
            snprintf(full, sizeof(full), "%s/%s", dirpath, e->d_name);
            struct stat st;
            if (stat(full, &st) != 0) continue;
            if (S_ISREG(st.st_mode)) {
                const char *dot = strrchr(e->d_name, '.');
                if (dot) {
                    char ext[32]; size_t n = 0;
                    for (const char *p = dot; *p && n+1 < sizeof(ext); p++) ext[n++] = (char)tolower((unsigned char)*p);
                    ext[n] = '\0';
                    for (int i = 0; CODE_EXTENSIONS[i]; i++)
                        if (strcmp(ext, CODE_EXTENSIONS[i]) == 0) { closedir(d); return true; }
                }
            } else if (is_root && S_ISDIR(st.st_mode) && e->d_name[0] != '.') {
                int skip = 0;
                for (int i = 0; SKIP_DIRS[i]; i++) if (strcmp(e->d_name, SKIP_DIRS[i]) == 0) { skip = 1; break; }
                if (!skip && sp < CODE_SCAN_MAX_ENTRIES) {
                    snprintf(stack[sp].path, sizeof(stack[sp].path), "%s", full);
                    stack[sp].is_root = 0; sp++;
                }
            }
        }
        closedir(d);
    }
    return false;
}

/*
 * PoP: _coding_instructions @ agent/coding_context.py:_coding_instructions
 * Standing operator instructions for the coding posture, read from
 * `agent.coding_instructions` in config.yaml (string or list of strings).
 * Returns malloc'd string (caller frees), empty "" when unset. */
char *coding_context_coding_instructions(const hermes_config_t *config)
{
    char yaml_path[HERMES_PATH_MAX];
    if (config && config->config_path[0]) {
        snprintf(yaml_path, sizeof(yaml_path), "%s", config->config_path);
    } else {
        const char *home = getenv("HOME");
        if (!home) return strdup("");
        snprintf(yaml_path, sizeof(yaml_path), "%s/.hermes/config.yaml", home);
    }
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(yaml_path, &err);
    if (err) free(err);
    if (!doc) return strdup("");

    /* string form first */
    const char *s = yaml_get_string(doc, "agent.coding_instructions");
    if (s && s[0]) {
        /* strip surrounding whitespace */
        while (*s == ' ' || *s == '\t' || *s == '\n') s++;
        char *out = strdup(s);
        size_t n = strlen(out);
        while (n > 0 && (out[n-1] == ' ' || out[n-1] == '\t' || out[n-1] == '\n')) out[--n] = '\0';
        yaml_free(doc);
        return out;
    }
    /* list form: join non-empty stripped items with '\n' */
    size_t cnt = yaml_list_count(doc, "agent.coding_instructions");
    if (cnt > 0) {
        size_t cap = 1024; char *out = malloc(cap); out[0] = '\0'; size_t len = 0; int first = 1;
        for (size_t i = 0; i < cnt; i++) {
            const char *item = yaml_list_get(doc, "agent.coding_instructions", i);
            if (!item) continue;
            while (*item == ' ' || *item == '\t' || *item == '\n') item++;
            char tmp[1024]; snprintf(tmp, sizeof(tmp), "%s", item);
            size_t tn = strlen(tmp);
            while (tn > 0 && (tmp[tn-1]==' '||tmp[tn-1]=='\t'||tmp[tn-1]=='\n')) tmp[--tn]='\0';
            if (!tmp[0]) continue;
            size_t need = len + tn + 2;
            if (need >= cap) { cap = need + 256; char *nb = realloc(out, cap); if (!nb) break; out = nb; }
            if (!first) { strcat(out, "\n"); len++; }
            strcat(out, tmp); len += tn; first = 0;
        }
        yaml_free(doc);
        return out;
    }
    yaml_free(doc);
    return strdup("");
}

/* === AG26: remaining annotations (real ports added above) === */
/* Port of Python agent/coding_context.py:_git
 * Port of Python agent/coding_context.py:_parse_status
 * Port of Python agent/coding_context.py:_read_small
 * Port of Python agent/coding_context.py:_project_facts
 * Port of Python agent/coding_context.py:detect_project_facts
 * Port of Python agent/coding_context.py:build_coding_workspace_block
 * Port of Python agent/coding_context.py:coding_selection
 * Port of Python agent/coding_context.py:coding_system_blocks
 * Port of Python agent/coding_context.py:coding_compact_skill_categories
 */
