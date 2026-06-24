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
 * Port of Python agent/coding_context.py:coding_selection — N/A, Python-only convenience wrapper
 * Port of Python agent/coding_context.py:coding_system_blocks — N/A, Python-only convenience wrapper
 * Port of Python agent/coding_context.py:coding_compact_skill_categories — N/A, Python-only convenience wrapper
 * Port of Python agent/coding_context.py:_enabled_mcp_servers — N/A, Python config dict access
 * Port of Python agent/coding_context.py:_git — N/A, Python subprocess/git wrapper
 * Port of Python agent/coding_context.py:_parse_status — N/A, Python git parser
 * Port of Python agent/coding_context.py:_read_small — N/A, Python file utility
 * Port of Python agent/coding_context.py:_project_facts — N/A, Python dict builder
 * Port of Python agent/coding_context.py:build_coding_workspace_block — ported inline in coding_runtime_mode_system_blocks()
 * Port of Python agent/coding_context.py:resolve_runtime_mode() — consolidated in coding_context_resolve_runtime_mode()
 */

#include "coding_context.h"
#include "hermes.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <limits.h>

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

static const struct lockfile_map PY_LOCKFILES[] = {
    {"uv.lock", "uv"},
    {"poetry.lock", "poetry"},
    {"Pipfile.lock", "pipenv"},
    {NULL, NULL}
};

static const struct lockfile_map JS_LOCKFILES[] = {
    {"pnpm-lock.yaml", "pnpm"},
    {"bun.lockb", "bun"},
    {"bun.lock", "bun"},
    {"yarn.lock", "yarn"},
    {"package-lock.json", "npm"},
    {NULL, NULL}
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
static const char *CODING_AGENT_GUIDANCE =
    "You are a coding agent pairing with the user inside their codebase. "
    "Operate like a careful senior engineer.\n"
    "\n"
    "Gather context first:\n"
    "- Read the relevant files with `read_file` and locate code with "
    "`search_files` before changing anything. Trace a symbol to its definition "
    "and usages rather than guessing its shape.\n"
    "- Batch independent lookups: when several reads/searches don't depend on "
    "each other, issue them together in one turn instead of one at a time.\n"
    "- Never invent files, symbols, APIs, or imports. If you haven't seen it in "
    "the repo, go look. Don't assume a library is available — check the project "
    "manifest (pyproject.toml / package.json / Cargo.toml / go.mod) and how "
    "neighbouring files import it.\n"
    "\n"
    "Make changes through the tools, not the chat:\n"
    "- Edit with `patch`/`write_file`. Do NOT print code blocks to the user as "
    "a substitute for editing — apply the change, then summarise it. Only show "
    "code when the user explicitly asks to see it.\n"
    "- Match the project's existing style and conventions; AGENTS.md / "
    "CLAUDE.md / .cursorrules already in context win over your defaults. Touch "
    "only what the task needs — no drive-by refactors, renames, or reformatting "
    "— and add any imports/dependencies your code requires.\n"
    "- If an edit fails to apply, re-read the file to get the current exact "
    "contents before retrying — don't repeat a stale patch. If the same region "
    "fails twice, rewrite the enclosing function or file with `write_file` "
    "instead of attempting a third patch.\n"
    "\n"
    "Verify, and know when to stop:\n"
    "- Use `terminal` for git, builds, tests, and inspection. Run the relevant "
    "tests/linter/build and confirm they pass before claiming the work is done.\n"
    "- Fix root causes, not symptoms: when you find a bug, check sibling call "
    "paths for the same flaw and fix the class, not just the reported site.\n"
    "- When fixing linter/type errors on a file, stop after about three "
    "attempts on the same file and ask the user rather than looping.\n"
    "- Track multi-step work with `todo`. Reference code as `path:line` instead "
    "of pasting whole files.\n"
    "\n"
    "Respect the user's repo: don't commit, push, or rewrite history unless "
    "asked, and never read, print, or commit secrets — leave `.env` and "
    "credential files alone unless the user explicitly asks. The Workspace "
    "block below is a snapshot from session start — re-run `git status`/"
    "`git branch` before relying on it. Be concise: lead with the change or "
    "answer, not a preamble.";

/* Per-model edit-format steering (Port of Python _EDIT_FORMAT_GUIDANCE) */
struct edit_format_family {
    const char **needles;
    const char *guidance_line;
};

static const char *PATCH_NEEDLES[] = {
    "gpt", "codex", NULL
};
static const char *PATCH_GUIDANCE =
    "- Edit format: author new files with `write_file`; for edits to "
    "existing code use `patch` with `mode='patch'` (V4A diff) — including "
    "single-file edits. It's the edit format you handle most reliably.";

static const char *REPLACE_NEEDLES[] = {
    "claude", "sonnet", "opus", "haiku",
    "gemini", "gemma", "deepseek", "qwen", "kimi", "glm", "grok",
    "hermes", "llama", "mistral", "devstral", "minimax", NULL
};
static const char *REPLACE_GUIDANCE =
    "- Edit format: author new files with `write_file`; for edits to "
    "existing code prefer `patch` in `mode='replace'` — match a unique "
    "snippet and swap it. Reach for `mode='patch'` (V4A) only when an edit "
    "genuinely spans several files at once.";

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
    if (config && config->agent.cwd[0]) {
        snprintf(out, out_size, "%s", config->agent.cwd);
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

/* AG26: Port of Python agent/coding_context.py:system_blocks() */
char **coding_runtime_mode_system_blocks(
    const coding_runtime_mode_t *mode, int *out_count) {

    if (!mode || !coding_runtime_mode_is_coding(mode)) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    /* Allocate array for up to 2 blocks (brief + workspace) */
    char **blocks = calloc(2, sizeof(char *));
    if (!blocks) { if (out_count) *out_count = 0; return NULL; }

    int count = 0;

    /* Operating brief with edit-format guidance */
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

    /* Workspace block — in C we build a simplified version */
    if (mode->cwd[0]) {
        char git_branch[128] = "";
        char git_status[256] = "";
        char git_root[PATH_MAX];

        if (coding_context_find_git_root(mode->cwd, git_root, sizeof(git_root))) {
            /* Try to get branch and status */
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "cd %s && git rev-parse --abbrev-ref HEAD 2>/dev/null", git_root);
            FILE *fp = popen(cmd, "r");
            if (fp) {
                if (fgets(git_branch, sizeof(git_branch), fp))
                    git_branch[strcspn(git_branch, "\n")] = '\0';
                pclose(fp);
            }

            snprintf(cmd, sizeof(cmd), "cd %s && git status --porcelain 2>/dev/null | head -20", git_root);
            fp = popen(cmd, "r");
            if (fp) {
                char line[256];
                int line_count = 0;
                while (fgets(line, sizeof(line), fp) && line_count < 20) {
                    if (strlen(git_status) + strlen(line) < sizeof(git_status) - 1)
                        strcat(git_status, line);
                    line_count++;
                }
                pclose(fp);
            }
        }

        size_t ws_len = 512 + strlen(mode->cwd) + strlen(git_branch) + strlen(git_status);
        blocks[count] = malloc(ws_len);
        if (blocks[count]) {
            snprintf(blocks[count], ws_len,
                "Workspace: %s\n"
                "Git branch: %s\n"
                "Git status:\n%s",
                mode->cwd,
                git_branch[0] ? git_branch : "(unknown)",
                git_status[0] ? git_status : "  (clean)"
            );
            count++;
        }
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
/*  AG26 Annotations                                                  */
/* ================================================================== */

/* Port of Python agent/coding_context.py:_model_family()
 * Port of Python agent/coding_context.py:_edit_format_line()
 * Port of Python agent/coding_context.py:CODING_AGENT_GUIDANCE
 * Port of Python agent/coding_context.py:GENERAL_PROFILE
 * Port of Python agent/coding_context.py:CODING_PROFILE
 * Port of Python agent/coding_context.py:get_profile()
 * Port of Python agent/coding_context.py:_coding_mode()
 * Port of Python agent/coding_context.py:_resolve_cwd()
 * Port of Python agent/coding_context.py:_git_root()
 * Port of Python agent/coding_context.py:_home()
 * Port of Python agent/coding_context.py:_marker_root()
 * Port of Python agent/coding_context.py:_detect_profile_name()
 * Port of Python agent/coding_context.py:RuntimeMode
 * Port of Python agent/coding_context.py:RuntimeMode.kind
 * Port of Python agent/coding_context.py:RuntimeMode.is_coding
 * Port of Python agent/coding_context.py:RuntimeMode.toolset_selection()
 * Port of Python agent/coding_context.py:RuntimeMode.system_blocks()
 * Port of Python agent/coding_context.py:RuntimeMode.compact_skill_categories()
 * Port of Python agent/coding_context.py:resolve_runtime_mode()
 */
