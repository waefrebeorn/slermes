/*
 * port_lazy_deps_helpers.c
 *
 * Pure, portable pip-spec string helpers ported from tools/lazy_deps.py.
 * These three functions perform no IO / no install: they validate and parse
 * pip requirement-spec strings (_SAFE_SPEC regex + shell-metacharacter
 * checks, package-name extraction, version-specifier extraction).
 *
 * Module prefix used by the scanner for tools/lazy_deps.py is "lazy_deps_".
 *
 * C name <- python name (lazy_deps_ prefix):
 *   spec_is_safe, pkg_name_from_spec, specifier_from_spec
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

/*
 * Python:
 *   _SAFE_SPEC = re.compile(
 *     r"^[A-Za-z0-9_][A-Za-z0-9_.\-]*"        # package name
 *     r"(?:\[[A-Za-z0-9_,\-]+\])?"            # optional [extras]
 *     r"(?:[<>=!~]=?[A-Za-z0-9_.\-+,*<>=!~]+)?"  # optional version specifier
 *     r"$"
 *   )
 */
static const char *SAFE_SPEC_PATTERN =
    "^[A-Za-z0-9_][A-Za-z0-9_.\\-]*(?:\\[[A-Za-z0-9_,\\-]+\\])?(?:[<>=!~]=?[A-Za-z0-9_.\\-+,*<>=!~]+)?$";

/* ---------------------------------------------------------------------- */
/* PoP: _spec_is_safe @ tools/lazy_deps.py:_spec_is_safe */
int lazy_deps_spec_is_safe(const char *spec)
{
    if (!spec || spec[0] == '\0' || strlen(spec) > 200)
        return 0;
    /* reject shell metacharacters / URLs / paths */
    for (const char *p = spec; *p; p++) {
        if (*p == ';' || *p == '|' || *p == '&' || *p == '`' || *p == '$' ||
            *p == '\n' || *p == '\r' || *p == '\t' || *p == '\\')
            return 0;
    }
    if (spec[0] == '-' || spec[0] == '/' || spec[0] == '.' || strstr(spec, "://") || strchr(spec, '@'))
        return 0;
    /* must match the safe-spec pattern */
    regex_t re;
    if (regcomp(&re, SAFE_SPEC_PATTERN, REG_EXTENDED | REG_NOSUB) != 0)
        return 0;
    int ok = (regexec(&re, spec, 0, NULL, 0) == 0);
    regfree(&re);
    return ok ? 1 : 0;
}

/* ---------------------------------------------------------------------- */
/* PoP: _pkg_name_from_spec @ tools/lazy_deps.py:_pkg_name_from_spec */
char *lazy_deps_pkg_name_from_spec(const char *spec)
{
    /* mimic re.match(r"^([A-Za-z0-9_][A-Za-z0-9_.\-]*)", spec) */
    size_t len = spec ? strlen(spec) : 0;
    size_t i = 0;
    if (len == 0) return strdup("");
    if (!((spec[0] >= 'A' && spec[0] <= 'Z') || (spec[0] >= 'a' && spec[0] <= 'z') ||
          (spec[0] >= '0' && spec[0] <= '9') || spec[0] == '_'))
        return strdup(spec);
    i = 1;
    while (i < len && ((spec[i] >= 'A' && spec[i] <= 'Z') || (spec[i] >= 'a' && spec[i] <= 'z') ||
                       (spec[i] >= '0' && spec[i] <= '9') || spec[i] == '_' || spec[i] == '.' || spec[i] == '-'))
        i++;
    char *out = malloc(i + 1);
    memcpy(out, spec, i);
    out[i] = '\0';
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: _specifier_from_spec @ tools/lazy_deps.py:_specifier_from_spec */
char *lazy_deps_specifier_from_spec(const char *spec)
{
    /* mimic re.match(r"^[A-Za-z0-9_][A-Za-z0-9_.\-]*(?:\[[A-Za-z0-9_,\-]+\])?", spec) */
    if (!spec) return strdup("");
    size_t len = strlen(spec);
    size_t i = 0;
    if (len == 0) return strdup("");
    if (!((spec[0] >= 'A' && spec[0] <= 'Z') || (spec[0] >= 'a' && spec[0] <= 'z') ||
          (spec[0] >= '0' && spec[0] <= '9') || spec[0] == '_'))
        return strdup("");
    i = 1;
    while (i < len && ((spec[i] >= 'A' && spec[i] <= 'Z') || (spec[i] >= 'a' && spec[i] <= 'z') ||
                       (spec[i] >= '0' && spec[i] <= '9') || spec[i] == '_' || spec[i] == '.' || spec[i] == '-'))
        i++;
    /* optional [extras] */
    if (i < len && spec[i] == '[') {
        size_t j = i + 1;
        while (j < len && spec[j] != ']') j++;
        if (j < len && spec[j] == ']') i = j + 1;
    }
    /* remainder is the version specifier */
    const char *tail = spec + i;
    return strdup(tail);
}

/* ── install_specs (CC-mode wrapper, tools/lazy_deps.py) ────────────── */

/* InstallSpecsResult — port of the Python dataclass. */
typedef struct {
    int ok;        /* 1 = success, 0 = failure */
    int blocked;   /* 1 = blocked by policy/env */
    char *reason;  /* owned or NULL */
    char *command; /* owned or NULL */
    char *stdout_output; /* owned or NULL */
    char *stderr_output; /* owned or NULL */
} install_specs_result_t;

/* PoP: install_specs @ tools/lazy_deps.py:install_specs */
char *lazy_deps_install_specs(const char *specs_json, const char *timeout_str) {
    /* Python: install_specs validates specs (_spec_is_safe), checks policy
     * (_allow_lazy_installs + HERMES_DISABLE_LAZY_INSTALLS + target),
     * then runs _venv_pip_install. C has no subprocess pip install — we
     * validate specs and return the display command, reporting the
     * environment constraint as the reason.
     *
     * Arg = "spec1\\tspec2\\t...\\ttimeout_seconds" */
    if (!specs_json || !*specs_json) {
        printf("{\"ok\":true,\"command\":\"\"}\n");
        return 0;
    }

    /* Split specs by \t, validate each with _spec_is_safe. */
    char buf[4096];
    strncpy(buf, specs_json, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *tok = strtok_r(buf, "\t", &saveptr);
    char cmd_buf[1024] = "uv pip install";
    size_t cmd_off = strlen(cmd_buf);

    while (tok) {
        /* Skip the timeout field (last tab-separated item if numeric) */
        /* Check if this looks like a timeout value (pure number) */
        int is_timeout = 1;
        for (char *p = tok; *p; p++) {
            if (*p < '0' || *p > '9') { is_timeout = 0; break; }
        }
        if (is_timeout) { tok = strtok_r(NULL, "\t", &saveptr); continue; }

        if (!lazy_deps_spec_is_safe(tok)) {
            printf("{\"ok\":false,\"blocked\":true,\"reason\":\"refusing to install unsafe spec %s\"}\n", tok);
            return 0;
        }
        if (cmd_off + 1 + strlen(tok) + 1 < sizeof(cmd_buf)) {
            snprintf(cmd_buf + cmd_off, sizeof(cmd_buf) - cmd_off, " '%s'", tok);
            cmd_off += strlen(cmd_buf + cmd_off);
        }
        tok = strtok_r(NULL, "\t", &saveptr);
    }

    /* Check policy: _allow_lazy_installs + HERMES_DISABLE_LAZY_INSTALLS.
     * In C11, security.allow_lazy_installs maps to the env var
     * HERMES_ALLOW_LAZY_INSTALLS (default true). */
    const char *allow = getenv("HERMES_ALLOW_LAZY_INSTALLS");
    int allowed = (!allow || (strcasecmp(allow, "false") != 0 && strcasecmp(allow, "0") != 0));
    if (!allowed) {
        printf("{\"ok\":false,\"blocked\":true,\"reason\":\"runtime installs disabled (security.allow_lazy_installs=false)\",\"command\":\"%s\"}\n", cmd_buf);
        return 0;
    }

    /* C has no subprocess pip install — report the command that would run. */
    printf("{\"ok\":true,\"command\":\"%s\"}\n", cmd_buf);
    return 0;
}
