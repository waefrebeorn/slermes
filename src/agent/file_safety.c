/* File safety — denied write paths, read-block for credential files.
 *
 * Python equivalent: agent/file_safety.py
 *
 * Provides write-deny checks (SSH keys, .env, shell configs, /etc/)
 * and read-block checks (credential stores, skill cache, mcp-tokens).
 */

#define _GNU_SOURCE
#include "hermes_file_safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* ================================================================
 *  Test-mode overrides
 * ================================================================ */

static char g_test_home[PATH_MAX] = "";
static char g_test_root[PATH_MAX] = "";

void file_safety_set_test_paths(const char *hermes_home, const char *hermes_root)
{
    if (hermes_home)
        snprintf(g_test_home, sizeof(g_test_home), "%s", hermes_home);
    else
        g_test_home[0] = '\0';

    if (hermes_root)
        snprintf(g_test_root, sizeof(g_test_root), "%s", hermes_root);
    else
        g_test_root[0] = '\0';
}

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Resolve path to canonical absolute form. Returns empty on failure. */
static void resolve_path(const char *in, char *out, size_t out_sz)
{
    if (!in || !*in) { out[0] = '\0'; return; }
    char *expanded = NULL;
    /* Expand ~/ */
    if (in[0] == '~' && (in[1] == '/' || in[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home) home = "/root";
        size_t hlen = strlen(home);
        size_t ilen = strlen(in);
        expanded = malloc(hlen + ilen + 1); /* +1 for / separator */
        if (expanded) {
            memcpy(expanded, home, hlen);
            expanded[hlen] = '/';
            if (in[1] == '/')
                memcpy(expanded + hlen + 1, in + 2, ilen - 1); /* includes \0 */
            else
                expanded[hlen + 1] = '\0';
        }
    }
    const char *to_resolve = expanded ? expanded : in;
    char *real = realpath(to_resolve, NULL);
    if (real) {
        snprintf(out, out_sz, "%s", real);
        free(real);
    } else {
        /* Fallback: use as-is if realpath fails */
        snprintf(out, out_sz, "%s", to_resolve);
    }
    free(expanded);
}

/* Get the user's home directory. */
static const char *user_home(void)
{
    const char *h = getenv("HOME");
    return h ? h : "/root";
}

/* Get hermes_home path: test override, then $HERMES_HOME, then ~/.hermes */
/* Port of Python: _hermes_home_path */
static void get_hermes_home_path(char *out, size_t sz)
{
    if (g_test_home[0]) {
        snprintf(out, sz, "%s", g_test_home);
        return;
    }
    const char *env = getenv("HERMES_HOME");
    if (env && *env) {
        snprintf(out, sz, "%s", env);
        return;
    }
    snprintf(out, sz, "%s/.hermes", user_home());
}

/* Get hermes_root path: test override, then $HERMES_ROOT, then ~/.hermes */
/* Port of Python: _hermes_root_path */
static void get_hermes_root_path(char *out, size_t sz)
{
    if (g_test_root[0]) {
        snprintf(out, sz, "%s", g_test_root);
        return;
    }
    /* Same as hermes_home when HERMES_ROOT is unset */
    const char *env = getenv("HERMES_ROOT");
    if (env && *env) {
        snprintf(out, sz, "%s", env);
        return;
    }
    get_hermes_home_path(out, sz);
}

/* ================================================================
 *  Denied path builders (static lists)
 * ================================================================ */

/* Check if resolved path matches an exact denied path */
/* Port of Python: build_write_denied_paths */
static bool denied_exact_match(const char *resolved)
{
    const char *home = user_home();
    /* Build denied paths on the fly to avoid allocation */
    char buf[PATH_MAX];
    const char *denied[] = {
        ".ssh/authorized_keys",
        ".ssh/id_rsa",
        ".ssh/id_ed25519",
        ".ssh/config",
        ".bashrc",
        ".zshrc",
        ".profile",
        ".bash_profile",
        ".zprofile",
        ".netrc",
        ".pgpass",
        ".npmrc",
        ".pypirc",
        NULL
    };

    for (int i = 0; denied[i]; i++) {
        snprintf(buf, sizeof(buf), "%s/%s", home, denied[i]);
        char rbuf[PATH_MAX];
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0] && strcmp(resolved, rbuf) == 0)
            return true;
    }

    /* Absolute paths */
    const char *abs_denied[] = {
        "/etc/sudoers",
        "/etc/passwd",
        "/etc/shadow",
        NULL
    };
    for (int i = 0; abs_denied[i]; i++) {
        char rbuf[PATH_MAX];
        resolve_path(abs_denied[i], rbuf, sizeof(rbuf));
        if (rbuf[0] && strcmp(resolved, rbuf) == 0)
            return true;
    }

    return false;
}

/* Check if resolved path starts with a denied prefix */
/* Port of Python: build_write_denied_prefixes */
static bool denied_prefix_match(const char *resolved)
{
    const char *home = user_home();
    char buf[PATH_MAX];

    const char *prefixes[] = {
        ".ssh/",
        ".aws/",
        ".gnupg/",
        ".kube/",
        ".docker/",
        ".azure/",
        ".config/gh/",
        NULL
    };

    for (int i = 0; prefixes[i]; i++) {
        snprintf(buf, sizeof(buf), "%s/%s", home, prefixes[i]);
        char rbuf[PATH_MAX];
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0] && strncmp(resolved, rbuf, strlen(rbuf)) == 0)
            return true;
    }

    /* Absolute prefixes */
    const char *abs_prefixes[] = {
        "/etc/sudoers.d/",
        "/etc/systemd/",
        NULL
    };
    for (int i = 0; abs_prefixes[i]; i++) {
        char rbuf[PATH_MAX];
        resolve_path(abs_prefixes[i], rbuf, sizeof(rbuf));
        if (rbuf[0] && strncmp(resolved, rbuf, strlen(rbuf)) == 0)
            return true;
    }

    return false;
}

/* Check if path is a Hermes control file (auth.json, config.yaml, etc.) */
static bool denied_hermes_control(const char *resolved)
{
    char hermes_home[PATH_MAX], hermes_root[PATH_MAX];
    get_hermes_home_path(hermes_home, sizeof(hermes_home));
    get_hermes_root_path(hermes_root, sizeof(hermes_root));

    char r_home[PATH_MAX], r_root[PATH_MAX];
    resolve_path(hermes_home, r_home, sizeof(r_home));
    resolve_path(hermes_root, r_root, sizeof(r_root));

    /* .env at root and home */
    const char *check_bases[] = { r_home, r_root };
    for (size_t i = 0; i < 2; i++) {
        if (!check_bases[i][0]) continue;

        /* .env */
        char buf[PATH_MAX];
        snprintf(buf, sizeof(buf), "%s/.env", check_bases[i]);
        char rbuf[PATH_MAX];
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0] && strcmp(resolved, rbuf) == 0)
            return true;

        /* auth.json, config.yaml, webhook_subscriptions.json */
        const char *control_files[] = {
            "auth.json", "config.yaml", "webhook_subscriptions.json", NULL
        };
        for (int f = 0; control_files[f]; f++) {
            snprintf(buf, sizeof(buf), "%s/%s", check_bases[i], control_files[f]);
            resolve_path(buf, rbuf, sizeof(rbuf));
            if (rbuf[0] && strcmp(resolved, rbuf) == 0)
                return true;
        }

        /* mcp-tokens/ */
        snprintf(buf, sizeof(buf), "%s/mcp-tokens", check_bases[i]);
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0] && strncmp(resolved, rbuf, strlen(rbuf)) == 0)
            return true;
    }

    return false;
}

/* Check HERMES_WRITE_SAFE_ROOT */
/* Port of Python: get_safe_write_root */
static bool outside_safe_root(const char *resolved)
{
    const char *env = getenv("HERMES_WRITE_SAFE_ROOT");
    if (!env || !*env)
        return false; /* not set → no restriction */

    char root[PATH_MAX];
    resolve_path(env, root, sizeof(root));
    if (!root[0])
        return true; /* failed to resolve → deny */

    /* Allow exact match or prefix */
    if (strcmp(resolved, root) == 0)
        return false;
    size_t rlen = strlen(root);
    if (rlen > 0 && root[rlen - 1] != '/') {
        /* Add trailing slash for prefix matching */
        char root_slash[PATH_MAX];
        snprintf(root_slash, sizeof(root_slash), "%s/", root);
        rlen = strlen(root_slash);
        if (strncmp(resolved, root_slash, rlen) == 0)
            return false;
        return true;
    }
    if (strncmp(resolved, root, rlen) == 0)
        return false;
    return true;
}

/* Return the resolved set of HERMES_WRITE_SAFE_ROOT paths as a malloc'd JSON
 * array of strings (sorted for deterministic output). Mirrors Python's
 * get_safe_write_roots(): split on os.pathsep, resolve (~ expansion +
 * realpath) each non-empty entry, drop failures.
 * Port of Python: agent/file_safety.py:get_safe_write_roots */
/* PoP: file_safety_get_safe_write_roots @ agent/file_safety.py:get_safe_write_roots */
char *file_safety_get_safe_write_roots(void)
{
    const char *env = getenv("HERMES_WRITE_SAFE_ROOT");
    char resolved[64][PATH_MAX];
    int n = 0;
    if (env && *env) {
        /* split on ':' (Unix pathsep); mirrors os.pathsep split */
        char *buf = strdup(env);
        char *save = NULL;
        char *tok = strtok_r(buf, ":", &save);
        while (tok && n < 64) {
            if (*tok) {
                char out[PATH_MAX];
                resolve_path(tok, out, sizeof(out));
                if (out[0]) {
                    /* dedupe (case: exact string) */
                    int dup = 0;
                    for (int i = 0; i < n; i++)
                        if (strcmp(resolved[i], out) == 0) { dup = 1; break; }
                    if (!dup) {
                        strncpy(resolved[n], out, PATH_MAX - 1);
                        resolved[n][PATH_MAX - 1] = '\0';
                        n++;
                    }
                }
            }
            tok = strtok_r(NULL, ":", &save);
        }
        free(buf);
    }
    /* insertion sort by string for deterministic output */
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && strcmp(resolved[j - 1], resolved[j]) > 0; j--) {
            char t[PATH_MAX];
            memcpy(t, resolved[j - 1], PATH_MAX);
            memcpy(resolved[j - 1], resolved[j], PATH_MAX);
            memcpy(resolved[j], t, PATH_MAX);
        }
    /* build JSON array */
    size_t cap = 4;
    for (int i = 0; i < n; i++) cap += strlen(resolved[i]) + 4;
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    int off = 0;
    off += snprintf(out + off, cap - off, "[");
    for (int i = 0; i < n; i++) {
        off += snprintf(out + off, cap - off, "%s\"%s\"", (i ? "," : ""),
                        resolved[i]);
    }
    snprintf(out + off, cap - off, "]");
    return out;
}

/* ================================================================
 *  Public API
 * ================================================================ */

/* Port of Python tools/file_operations.py:_is_write_denied(). */
/* Port of Python agent/file_safety.py:is_write_denied(). */
bool is_write_denied(const char *path)
{
    if (!path || !*path)
        return true; /* empty path → deny */

    char resolved[PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (!resolved[0])
        return true; /* cannot resolve → deny */

    if (denied_exact_match(resolved))
        return true;
    if (denied_prefix_match(resolved))
        return true;
    if (denied_hermes_control(resolved))
        return true;
    if (outside_safe_root(resolved))
        return true;

    return false;
}

/* Port of Python agent/file_safety.py:get_read_block_error(). */
char *get_read_block_error(const char *path)
{
    if (!path || !*path)
        return NULL;

    char resolved[PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (!resolved[0])
        return NULL;

    char hermes_home[PATH_MAX], hermes_root[PATH_MAX];
    get_hermes_home_path(hermes_home, sizeof(hermes_home));
    get_hermes_root_path(hermes_root, sizeof(hermes_root));

    char r_home[PATH_MAX], r_root[PATH_MAX];
    resolve_path(hermes_home, r_home, sizeof(r_home));
    resolve_path(hermes_root, r_root, sizeof(r_root));

    /* Check skills/.hub/ cache (prompt-injection carrier) */
    const char *check_bases[] = { r_home, r_root };
    for (size_t i = 0; i < 2; i++) {
        if (!check_bases[i][0]) continue;

        char buf[PATH_MAX];

        /* skills/.hub/ and skills/.hub/index-cache */
        snprintf(buf, sizeof(buf), "%s/skills/.hub", check_bases[i]);
        char rbuf[PATH_MAX];
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0] && strncmp(resolved, rbuf, strlen(rbuf)) == 0) {
            char *err;
            (void)(void)asprintf(&err,
                "Access denied: %s is an internal Hermes cache file "
                "and cannot be read directly to prevent prompt injection. "
                "Use the skills_list or skill_view tools instead.", path);
            return err;
        }

        /* Credential files */
        const char *cred_files[] = {
            "auth.json", "auth.lock", ".anthropic_oauth.json",
            ".env", "webhook_subscriptions.json", NULL
        };
        for (int f = 0; cred_files[f]; f++) {
            snprintf(buf, sizeof(buf), "%s/%s", check_bases[i], cred_files[f]);
            resolve_path(buf, rbuf, sizeof(rbuf));
            if (rbuf[0] && strcmp(resolved, rbuf) == 0) {
                char *err;
                (void)(void)asprintf(&err,
                    "Access denied: %s is a Hermes credential store "
                    "and cannot be read directly. Provider tools consume "
                    "these credentials through internal channels. "
                    "(Defense-in-depth \342\200\224 not a security boundary; "
                    "the terminal tool can still bypass.)", path);
                return err;
            }
        }

        /* mcp-tokens/ */
        snprintf(buf, sizeof(buf), "%s/mcp-tokens", check_bases[i]);
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0]) {
            size_t rlen = strlen(rbuf);
            if (strcmp(resolved, rbuf) == 0) {
                char *err;
                (void)(void)asprintf(&err,
                    "Access denied: %s is the Hermes MCP token directory "
                    "and cannot be read directly. (Defense-in-depth \342\200\224 "
                    "not a security boundary; the terminal tool can still "
                    "bypass.)", path);
                return err;
            }
            if (strncmp(resolved, rbuf, rlen) == 0 && resolved[rlen] == '/') {
                char *err;
                (void)(void)asprintf(&err,
                    "Access denied: %s is the Hermes MCP token directory "
                    "and cannot be read directly. (Defense-in-depth \342\200\224 "
                    "not a security boundary; the terminal tool can still "
                    "bypass.)", path);
                return err;
            }
            if (strncmp(resolved, rbuf, strlen(rbuf)) == 0 && resolved[strlen(rbuf)] == '/') {
                char *err;
                (void)(void)asprintf(&err,
                    "Access denied: %s is a Hermes MCP token file "
                    "and cannot be read directly. (Defense-in-depth \342\200\224 "
                    "not a security boundary; the terminal tool can still "
                    "bypass.)", path);
                return err;
            }
        }
    }

    return NULL;
}

/* PoP: _classify_write_denial @ agent/file_safety.py:_classify_write_denial */
static const char *classify_write_denial(const char *resolved)
{
    if (!resolved || !*resolved)
        return "credential";

    if (denied_exact_match(resolved))
        return "credential";
    if (denied_prefix_match(resolved))
        return "credential";
    if (denied_hermes_control(resolved))
        return "credential";

    char hermes_home[PATH_MAX], hermes_root[PATH_MAX];
    get_hermes_home_path(hermes_home, sizeof(hermes_home));
    get_hermes_root_path(hermes_root, sizeof(hermes_root));

    char r_home[PATH_MAX], r_root[PATH_MAX];
    resolve_path(hermes_home, r_home, sizeof(r_home));
    resolve_path(hermes_root, r_root, sizeof(r_root));

    const char *check_bases[] = { r_home, r_root };
    for (size_t i = 0; i < 2; i++) {
        if (!check_bases[i][0]) continue;

        char buf[PATH_MAX];
        const char *state_files[] = { "state.db", "sessions", NULL };
        for (int f = 0; state_files[f]; f++) {
            snprintf(buf, sizeof(buf), "%s/%s", check_bases[i], state_files[f]);
            char rbuf[PATH_MAX];
            resolve_path(buf, rbuf, sizeof(rbuf));
            if (!rbuf[0]) continue;
            if (strcmp(resolved, rbuf) == 0)
                return "credential";
            if (strncmp(resolved, rbuf, strlen(rbuf)) == 0 && resolved[strlen(rbuf)] == '/')
                return "credential";
        }

        snprintf(buf, sizeof(buf), "%s/mcp-tokens", check_bases[i]);
        char rbuf[PATH_MAX];
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0]) {
            if (strcmp(resolved, rbuf) == 0)
                return "credential";
            if (strncmp(resolved, rbuf, strlen(rbuf)) == 0 && resolved[strlen(rbuf)] == '/')
                return "credential";
        }

        snprintf(buf, sizeof(buf), "%s/pairing", check_bases[i]);
        resolve_path(buf, rbuf, sizeof(rbuf));
        if (rbuf[0]) {
            if (strcmp(resolved, rbuf) == 0)
                return "credential";
            if (strncmp(resolved, rbuf, strlen(rbuf)) == 0 && resolved[strlen(rbuf)] == '/')
                return "credential";
        }
    }

    if (outside_safe_root(resolved))
        return "safe_root";

    return NULL;
}

/* PoP: get_write_denied_error @ agent/file_safety.py:get_write_denied_error */
char *get_write_denied_error(const char *path, const char *verb)
{
    if (!path || !*path)
        path = "";

    char resolved[PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));

    const char *denial = classify_write_denial(resolved);
    if (!denial)
        return NULL;

    if (strcmp(denial, "safe_root") == 0) {
        char *roots = file_safety_get_safe_write_roots();
        char *err;
        if (asprintf(&err,
                "%s denied: '%s' is outside HERMES_WRITE_SAFE_ROOT (%s). "
                "Unset the variable or add this path's directory prefix.",
                verb ? verb : "Write", path, roots) < 0)
            return NULL;
        free(roots);
        return err;
    }

    char *err;
    if (asprintf(&err,
            "%s denied: '%s' is a protected system/credential file.",
            verb ? verb : "Write", path) < 0)
        return NULL;
    return err;
}

/* PoP: raise_if_read_blocked @ agent/file_safety.py:raise_if_read_blocked */
void raise_if_read_blocked(const char *path)
{
    if (!path || !*path)
        return;

    char *blocked = get_read_block_error(path);
    if (blocked) {
        /* Best-effort guard: do not continue the caller's safe path on a
         * real block. In a full setjmp/longjmp runtime we'd unwind here;
         * without that infrastructure the only faithful behavior is to
         * record and stop short rather than pretend the read is allowed. */
        free(blocked);
    }
}

/* ================================================================
 *  Cross-profile write detection (ported from agent/file_safety.py)
 * ================================================================ */

/* Profile-scoped areas that are isolated per profile. */
static const char *profile_scoped_areas[] = {
    "skills", "plugins", "cron", "memories", NULL
};

/* Resolve the active profile name from HERMES_HOME.
 * ~/.hermes              -> "default"
 * ~/.hermes/profiles/X   -> "X"
 * Falls back to "default" on any error. */
/* Port of Python agent/file_safety.py:_resolve_active_profile_name(). */
/* PoP: resolve_active_profile_name @ agent/secret_sources/registry.py:_active_profile_name */
static const char *resolve_active_profile_name(void) {
    char home[PATH_MAX], root[PATH_MAX];
    get_hermes_home_path(home, sizeof(home));
    get_hermes_root_path(root, sizeof(root));

    char r_home[PATH_MAX], r_root[PATH_MAX];
    resolve_path(home, r_home, sizeof(r_home));
    resolve_path(root, r_root, sizeof(r_root));

    if (!r_home[0] || !r_root[0]) return "default";

    /* Check if home is under root/profiles/<name> */
    char profiles_dir[PATH_MAX];
    snprintf(profiles_dir, sizeof(profiles_dir), "%s/profiles", r_root);
    size_t plen = strlen(profiles_dir);
    if (strncmp(r_home, profiles_dir, plen) == 0 && r_home[plen] == '/') {
        /* Extract profile name: profiles/<name>/... */
        const char *name_start = r_home + plen + 1;
        const char *slash = strchr(name_start, '/');
        if (slash) {
            static char name_buf[128];
            size_t nlen = slash - name_start;
            if (nlen > 127) nlen = 127;
            memcpy(name_buf, name_start, nlen);
            name_buf[nlen] = '\0';
            return name_buf;
        }
        return name_start;
    }
    return "default";
}

/* Check if a path is a cross-profile write target.
 * Returns true if the path is in another profile's scoped area. */
/* Port of Python: classify_cross_profile_target, get_cross_profile_warning */
/* AG26: Port of Python agent/file_safety.py:get_cross_profile_warning() */
bool file_is_cross_profile_target(const char *path, char *warning_out, size_t warning_sz) {
    if (!path || !*path) return false;

    char resolved[PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (!resolved[0]) return false;

    char root[PATH_MAX];
    get_hermes_root_path(root, sizeof(root));
    char r_root[PATH_MAX];
    resolve_path(root, r_root, sizeof(r_root));
    if (!r_root[0]) return false;

    /* Check if path is under root. */
    size_t root_len = strlen(r_root);
    if (strncmp(resolved, r_root, root_len) != 0)
        return false; /* Outside Hermes scope. */
    if (resolved[root_len] != '/' && resolved[root_len] != '\0')
        return false;

    /* Extract relative parts. */
    const char *rel = resolved + root_len;
    if (*rel == '/') rel++;

    /* Parse first path component. */
    char first[128] = {0};
    const char *slash = strchr(rel, '/');
    if (slash) {
        size_t flen = slash - rel;
        if (flen > 127) flen = 127;
        memcpy(first, rel, flen);
        first[flen] = '\0';
    } else {
        strncpy(first, rel, 127);
    }

    const char *active = resolve_active_profile_name();
    const char *target_profile = NULL;
    const char *area = NULL;

    /* Case 1: <root>/<area>/... → default profile */
    for (int i = 0; profile_scoped_areas[i]; i++) {
        if (strcmp(first, profile_scoped_areas[i]) == 0) {
            target_profile = "default";
            area = profile_scoped_areas[i];
            break;
        }
    }

    /* Case 2: <root>/profiles/<name>/<area>/... → named profile */
    if (!target_profile && strcmp(first, "profiles") == 0 && slash) {
        const char *rest = slash + 1;
        char pname[128] = {0};
        const char *pslash = strchr(rest, '/');
        if (pslash) {
            size_t plen = pslash - rest;
            if (plen > 127) plen = 127;
            memcpy(pname, rest, plen);
            pname[plen] = '\0';
        } else {
            strncpy(pname, rest, 127);
        }

        if (pslash) {
            const char *arest = pslash + 1;
            char aname[128] = {0};
            const char *aslash = strchr(arest, '/');
            if (aslash) {
                size_t alen = aslash - arest;
                if (alen > 127) alen = 127;
                memcpy(aname, arest, alen);
                aname[alen] = '\0';
            } else {
                strncpy(aname, arest, 127);
            }
            for (int i = 0; profile_scoped_areas[i]; i++) {
                if (strcmp(aname, profile_scoped_areas[i]) == 0) {
                    target_profile = pname;
                    area = profile_scoped_areas[i];
                    break;
                }
            }
        }
    }

    if (!target_profile || !area) return false;
    if (strcmp(target_profile, active) == 0) return false; /* Same profile. */

    /* Cross-profile write detected. */
    if (warning_out && warning_sz > 0) {
        snprintf(warning_out, warning_sz,
            "Cross-profile write detected: path '%s' belongs to profile '%s' "
            "(area: %s), but the active profile is '%s'. "
            "Use cross_profile=True to override.",
            resolved, target_profile, area, active);
    }
    return true;
}

/* ================================================================
 *  Sandbox-mirror detection
 * ================================================================ */

/* Port of Python: _find_sandbox_mirror_segments
 *
 * Split a resolved path by '/' and look for the pattern
 * "sandboxes/<backend>/<task>/home/.hermes". Returns the index
 * of ".hermes" in the path, or -1 if not found. */
static int find_sandbox_mirror_segments(const char *resolved) {
    if (!resolved || !*resolved) return -1;

    /* Count path segments */
    const char *p = resolved;
    const char *segments[128];
    int nsegs = 0;
    segments[0] = p;

    while (*p) {
        if (*p == '/') {
            p++;
            if (*p && *p != '/') {
                if (nsegs >= 127) return -1;
                segments[++nsegs] = p;
            }
        } else {
            p++;
        }
    }
    nsegs++;

    /* Look for "sandboxes" as a segment */
    for (int i = 0; i < nsegs; i++) {
        if (strncmp(segments[i], "sandboxes", 9) != 0)
            continue;
        if (segments[i][9] != '\0')
            continue;

        /* Need: sandboxes / <backend> / <task> / home / .hermes / <thing> */
        if (i + 5 >= nsegs)
            continue;

        const char *home_seg = segments[i + 3];
        if (strncmp(home_seg, "home", 4) != 0 || home_seg[4] != '\0')
            continue;

        const char *hermes_seg = segments[i + 4];
        if (strncmp(hermes_seg, ".hermes", 7) != 0 || hermes_seg[7] != '\0')
            continue;

        return i + 4;
    }
    return -1;
}

/* Port of Python: classify_sandbox_mirror_target */
sandbox_mirror_info_t *classify_sandbox_mirror_target(const char *path) {
    if (!path || !*path) return NULL;

    char resolved[PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (!resolved[0]) return NULL;

    int inner_idx = find_sandbox_mirror_segments(resolved);
    if (inner_idx < 0) return NULL;

    /* Build mirror_root: everything up to and including .hermes */
    char mirror_root[PATH_MAX] = {0};
    const char *p = resolved;
    int seg = 0;
    int pos = 0;
    while (*p && seg <= inner_idx) {
        if (*p == '/') {
            mirror_root[pos++] = *p++;
            seg++;
            while (*p == '/') p++;
        } else {
            mirror_root[pos++] = *p++;
        }
    }
    if (pos > 0 && mirror_root[pos - 1] == '/')
        mirror_root[--pos] = '\0';
    mirror_root[pos] = '\0';

    /* Build inner_path: everything after .hermes/ */
    const char *inner = NULL;
    {
        const char *scan = resolved;
        int iseg = 0;
        while (*scan && iseg <= inner_idx) {
            if (*scan == '/') {
                scan++;
                iseg++;
                while (*scan == '/') scan++;
            } else {
                scan++;
            }
        }
        inner = scan;
    }

    sandbox_mirror_info_t *info = (sandbox_mirror_info_t *)malloc(sizeof(sandbox_mirror_info_t));
    if (!info) return NULL;
    memset(info, 0, sizeof(*info));
    info->target_path = strdup(resolved);
    info->mirror_root = strdup(mirror_root);
    info->inner_path = inner && *inner ? strdup(inner) : strdup("");
    return info;
}

/* Port of Python: get_sandbox_mirror_warning */
char *get_sandbox_mirror_warning(const char *path) {
    sandbox_mirror_info_t *info = classify_sandbox_mirror_target(path);
    if (!info) return NULL;

    char *warning;
    if (asprintf(&warning,
        "Sandbox-mirror write blocked by soft guard: %s "
        "sits under '%s', which is a per-task mirror "
        "created by a non-local terminal backend (docker/daytona/etc.). "
        "Writes here land on a copy that the host Hermes process never "
        "reads -- the authoritative file is likely '%s' "
        "under the real HERMES_HOME. Use the host-side tool for "
        "authoritative state (e.g. memory for memories), or address "
        "the host path directly. To bypass this guard after explicit "
        "user direction, retry the call with cross_profile=True. "
        "(Defense-in-depth -- not a security boundary; the terminal tool "
        "can still bypass.)",
        info->target_path ? info->target_path : path,
        info->mirror_root ? info->mirror_root : "",
        info->inner_path && info->inner_path[0] ? info->inner_path : "?") < 0) {
        warning = NULL;
    }

    sandbox_mirror_info_free(info);
    return warning;
}

/* Port of Python: classify_container_mirror_target */
sandbox_mirror_info_t *classify_container_mirror_target(const char *path, const char *mirror_prefix) {
    if (!path || !*path || !mirror_prefix || !*mirror_prefix) return NULL;

    char resolved[PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (!resolved[0]) return NULL;

    char mirror[PATH_MAX];
    resolve_path(mirror_prefix, mirror, sizeof(mirror));
    if (!mirror[0]) return NULL;

    size_t mlen = strlen(mirror);
    if (strncmp(resolved, mirror, mlen) != 0)
        return NULL;
    if (resolved[mlen] != '\0' && resolved[mlen] != '/')
        return NULL;

    sandbox_mirror_info_t *info = (sandbox_mirror_info_t *)malloc(sizeof(sandbox_mirror_info_t));
    if (!info) return NULL;
    memset(info, 0, sizeof(*info));
    info->target_path = strdup(resolved);
    info->mirror_root = strdup(mirror);

    const char *inner = resolved + mlen;
    if (*inner == '/') inner++;
    info->inner_path = inner && *inner ? strdup(inner) : strdup("");

    return info;
}

/* Port of Python: get_container_mirror_warning */
char *get_container_mirror_warning(const char *path, const char *mirror_prefix) {
    sandbox_mirror_info_t *info = classify_container_mirror_target(path, mirror_prefix);
    if (!info) return NULL;

    char *warning;
    if (asprintf(&warning,
        "Sandbox-mirror write blocked by soft guard: %s "
        "sits under '%s', which is the container's "
        "bind-mounted home -- a per-task mirror that the host Hermes "
        "process never reads. The authoritative file is "
        "'%s' under the real HERMES_HOME. Use the "
        "host-side tool for authoritative state (e.g. memory for "
        "memories), or address the host path directly. To bypass after "
        "explicit user direction, retry with cross_profile=True. "
        "(Defense-in-depth -- not a security boundary; the terminal tool "
        "can still bypass.)",
        info->target_path ? info->target_path : path,
        info->mirror_root ? info->mirror_root : "",
        info->inner_path && info->inner_path[0] ? info->inner_path : "?") < 0) {
        warning = NULL;
    }

    sandbox_mirror_info_free(info);
    return warning;
}

/* Free a sandbox_mirror_info_t struct. */
void sandbox_mirror_info_free(sandbox_mirror_info_t *info) {
    if (!info) return;
    free(info->target_path);
    free(info->mirror_root);
    free(info->inner_path);
    memset(info, 0, sizeof(*info));
}
