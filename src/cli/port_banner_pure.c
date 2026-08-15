/* banner_pure.c — Port of hermes_cli/banner.py pure helpers.
 * Deterministic string/URL/format functions (no git, no subprocess, no TTY).
 */
#define _GNU_SOURCE
#include "banner_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* extern helpers from config.py ports */
extern const char *detect_install_method(const char *project_root);
extern void config_py_recommended_update_command_for_method(const char *method, char *out, size_t out_size);
extern char *cfg_get_managed_update_command(const char *managed_system);
extern char *dashboard_procs_hermes_home_dir(void);

/* Python: _OFFICIAL_REPO_CANONICAL = "github.com/nousresearch/hermes-agent". */
#define BANNER_OFFICIAL_REPO_CANONICAL "github.com/nousresearch/hermes-agent"

/* PoP: banner_canonical_github_remote @ hermes_cli/banner.py:_canonical_github_remote */
char *banner_canonical_github_remote(const char *url) {
    if (!url) return strdup("");
    /* value = url.strip() */
    const char *s = url;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t slen = strlen(s);
    while (slen > 0 && (s[slen-1] == ' ' || s[slen-1] == '\t' || s[slen-1] == '\n' || s[slen-1] == '\r')) slen--;

    /* Rebuild into a growable buffer. */
    size_t cap = slen + 16;
    char *value = malloc(cap + 1);
    if (!value) return strdup("");

    const char *git_scp = "git@github.com:";
    const char *ssh_url = "ssh://git@github.com/";
    if (strncmp(s, git_scp, strlen(git_scp)) == 0) {
        snprintf(value, cap + 1, "github.com/%s", s + strlen(git_scp));
    } else if (strncmp(s, ssh_url, strlen(ssh_url)) == 0) {
        snprintf(value, cap + 1, "github.com/%s", s + strlen(ssh_url));
    } else {
        /* urlparse(value): extract netloc + path */
        const char *scheme_end = strstr(s, "://");
        const char *p = scheme_end ? scheme_end + 3 : s;
        const char *path_start = p;
        const char *slash = strchr(path_start, '/');
        const char *netloc_end = slash ? slash : path_start + strlen(path_start);
        /* netloc may have userinfo@ or :port — Python urlparse keeps them in
         * netloc; the Python code uses f"{netloc}{path}". We replicate that
         * literally (urlparse includes userinfo and port in netloc). */
        const char *path = slash ? slash : "";
        size_t nl = (size_t)(netloc_end - path_start);
        if (nl == 0 && !*path) {
            /* no netloc and no path: value = whole input */
            snprintf(value, cap + 1, "%s", s);
        } else {
            size_t tl = nl + strlen(path);
            if (tl > cap) { cap = tl + 1; char *n = realloc(value, cap + 1); if (!n) { free(value); return strdup(""); } value = n; }
            memcpy(value, path_start, nl);
            memcpy(value + nl, path, strlen(path));
            value[nl + strlen(path)] = '\0';
        }
    }

    /* value = value.strip().rstrip("/") */
    char *v = value;
    size_t vl = strlen(v);
    while (vl > 0 && (v[vl-1] == ' ' || v[vl-1] == '\t' || v[vl-1] == '\n' || v[vl-1] == '\r')) v[--vl] = '\0';
    while (vl > 0 && v[vl-1] == '/') v[--vl] = '\0';
    if (vl >= 4 && strcmp(v + vl - 4, ".git") == 0) v[vl-4] = '\0';
    /* .lower() */
    for (char *q = v; *q; q++) *q = (char)tolower((unsigned char)*q);
    return value;
}

/* PoP: banner_is_ssh_remote @ hermes_cli/banner.py:_is_ssh_remote */
bool banner_is_ssh_remote(const char *url) {
    if (!url) return false;
    const char *v = url;
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    size_t vl = strlen(v);
    while (vl > 0 && (v[vl-1] == ' ' || v[vl-1] == '\t' || v[vl-1] == '\n' || v[vl-1] == '\r')) vl--;
    if (vl == 0) return false;
    char *low = malloc(vl + 1);
    if (!low) return false;
    for (size_t i = 0; i < vl; i++) low[i] = (char)tolower((unsigned char)v[i]);
    low[vl] = '\0';
    bool res = (strncmp(low, "git@", 4) == 0) || (strncmp(low, "ssh://", 6) == 0);
    free(low);
    return res;
}

/* PoP: banner_is_official_ssh_remote @ hermes_cli/banner.py:_is_official_ssh_remote */
bool banner_is_official_ssh_remote(const char *url) {
    if (!banner_is_ssh_remote(url)) return false;
    char *canon = banner_canonical_github_remote(url);
    bool res = strcmp(canon, BANNER_OFFICIAL_REPO_CANONICAL) == 0;
    free(canon);
    return res;
}

/* PoP: banner_format_context_length @ hermes_cli/banner.py:_format_context_length */
char *banner_format_context_length(long long tokens) {
    char *out = NULL;
    if (tokens >= 1000000) {
        double val = tokens / 1000000.0;
        double rounded = round(val);
        if (fabs(val - rounded) < 0.05) {
            asprintf(&out, "%.0fM", rounded);
        } else {
            asprintf(&out, "%.1fM", val);
        }
    } else if (tokens >= 1000) {
        double val = tokens / 1000.0;
        double rounded = round(val);
        if (fabs(val - rounded) < 0.05) {
            asprintf(&out, "%.0fK", rounded);
        } else {
            asprintf(&out, "%.1fK", val);
        }
    } else {
        asprintf(&out, "%lld", tokens);
    }
    return out ? out : strdup("");
}

/* PoP: banner_display_toolset_name @ hermes_cli/banner.py:_display_toolset_name */
char *banner_display_toolset_name(const char *toolset_name) {
    /* Python: `if not toolset_name: return "unknown"` — empty string is falsy. */
    if (!toolset_name || !*toolset_name) return strdup("unknown");
    size_t l = strlen(toolset_name);
    if (l >= 6 && strcmp(toolset_name + l - 6, "_tools") == 0) {
        char *out = strndup(toolset_name, l - 6);
        return out ? out : strdup(toolset_name);
    }
    return strdup(toolset_name);
}

/* PoP: banner_format_update_notice @ hermes_cli/banner.py:_format_update_notice */
/* Render the update warning line for a non-zero behind result. Caller frees. */
char *banner_format_update_notice(int behind) {
    if (behind > 0) {
        const char *commits_word = (behind == 1) ? "commit" : "commits";
        /* Python: recommended_update_command() -> detect_install_method() +
         * recommended_update_command_for_method(). */
        const char *method = detect_install_method(NULL);
        char cmd_buf[256];
        config_py_recommended_update_command_for_method(method, cmd_buf, sizeof(cmd_buf));
        char *out = NULL;
        asprintf(&out,
            "[bold yellow]⚠ %d %s behind[/][dim yellow] — run [bold]%s[/bold] to update[/]",
            behind, commits_word, cmd_buf);
        return out;
    }
    char *managed_cmd = cfg_get_managed_update_command(NULL);
    char *line = strdup("[bold yellow]⚠ update available[/]");
    if (managed_cmd && *managed_cmd) {
        char *extra = NULL;
        asprintf(&extra, "%s[dim yellow] — run [bold]%s[/bold][/]", line, managed_cmd);
        free(line); free(managed_cmd);
        return extra;
    }
    free(managed_cmd);
    return line;
}

/* PoP: banner_snapshot_path @ hermes_cli/banner.py:_banner_snapshot_path */
/* Return path to banner snapshot JSON. Caller frees. */
char *banner_snapshot_path(void) {
    char *home = dashboard_procs_hermes_home_dir();
    if (!home) return strdup("~/.hermes/cache/banner_snapshot.json");
    char *out = NULL;
    asprintf(&out, "%s/cache/banner_snapshot.json", home);
    free(home);
    return out;
}
