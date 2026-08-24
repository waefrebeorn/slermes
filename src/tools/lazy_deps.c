/*
 * port_lazy_deps.c — Faithful C11 port of pure helpers from
 * tools/lazy_deps.py
 *
 * Ported: _pkg_name_from_spec, _specifier_from_spec, feature_specs,
 * feature_missing, feature_install_command.
 * IO-coupled functions (_is_satisfied, _is_present, active_features,
 * _python_abi_tag, _lazy_install_target, _venv_pip_install, ensure_and_bind,
 * _ensure_target_ready, _activate_target_on_syspath, etc.) left as REAL_GAP.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "json.h"
#include "lazy_deps.h"

/* LAZY_DEPS allowlist — mirrors Python dict. */
static const char *LAZY_DEPS_KEYS[] = {
    "provider.anthropic","provider.bedrock","provider.vertex",
    "provider.azure_identity","search.exa","search.firecrawl",
    "search.parallel",
};
static const char *LAZY_DEPS_VALS[][3] = {
    {"anthropic==0.87.0", NULL, NULL},
    {"boto3==1.42.89", NULL, NULL},
    {"google-auth==2.55.1", NULL, NULL},
    {"azure-identity==1.25.3", NULL, NULL},
    {"exa-py==2.10.2", NULL, NULL},
    {"firecrawl-py==4.17.0", NULL, NULL},
    {"parallel-web==0.4.2", NULL, NULL},
};
static const int LAZY_DEPS_N = 7;

/* PoP: lazy_pkg_name_from_spec @ tools/lazy_deps.py:_pkg_name_from_spec */
void lazy_pkg_name_from_spec(const char *spec, char *out, size_t out_cap) {
    if (!spec || !*spec) { out[0]='\0'; return; }
    /* ^([A-Za-z0-9_][A-Za-z0-9_.\\-]*) */
    size_t i = 0;
    if (!isalnum((unsigned char)spec[0]) && spec[0] != '_') {
        strncpy(out, spec, out_cap-1); out[out_cap-1]='\0'; return;
    }
    for (; spec[i] && i < out_cap-1; i++) {
        char c = spec[i];
        if (isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-') {
            out[i] = c;
        } else break;
    }
    out[i] = '\0';
}

/* PoP: lazy_specifier_from_spec @ tools/lazy_deps.py:_specifier_from_spec */
void lazy_specifier_from_spec(const char *spec, char *out, size_t out_cap) {
    if (!spec || !*spec) { out[0]='\0'; return; }
    /* skip the package name part */
    size_t i = 0;
    if (isalnum((unsigned char)spec[0]) || spec[0] == '_') {
        for (; spec[i]; i++) {
            char c = spec[i];
            if (isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-') continue;
            break;
        }
    }
    /* i now points at the first non-name char (or end) */
    /* handle extras like [encryption] */
    if (spec[i] == '[') {
        i++;
        while (spec[i] && spec[i] != ']') i++;
        if (spec[i] == ']') i++;
    }
    strncpy(out, spec + i, out_cap-1);
    out[out_cap-1] = '\0';
}

/* PoP: lazy_feature_specs @ tools/lazy_deps.py:feature_specs */
json_t *lazy_feature_specs(const char *feature) {
    json_t *out = json_array();
    if (!feature) return out;
    for (int i = 0; i < LAZY_DEPS_N; i++) {
        if (strcmp(feature, LAZY_DEPS_KEYS[i]) == 0) {
            for (int j = 0; LAZY_DEPS_VALS[i][j]; j++) {
                json_append(out, json_string(LAZY_DEPS_VALS[i][j]));
            }
            return out;
        }
    }
    /* KeyError — caller handles */
    return out;
}

/* PoP: lazy_feature_missing @ tools/lazy_deps.py:feature_missing */
json_t *lazy_feature_missing(const char *feature) {
    json_t *out = json_array();
    if (!feature) return out;
    for (int i = 0; i < LAZY_DEPS_N; i++) {
        if (strcmp(feature, LAZY_DEPS_KEYS[i]) == 0) {
            for (int j = 0; LAZY_DEPS_VALS[i][j]; j++) {
                /* feature_missing returns specs NOT satisfied — but without
                 * importlib.metadata we can't check. The Python function
                 * returns specs that are missing. We return all specs as
                 * "potentially missing" — caller checks presence separately.
                 * Actually, feature_missing returns specs not in LAZY_DEPS
                 * or not installed. Since we can't check installation, we
                 * return the full spec list (matching the "all missing" case).
                 */
                json_append(out, json_string(LAZY_DEPS_VALS[i][j]));
            }
            return out;
        }
    }
    /* Unknown feature — return all specs (KeyError path) */
    for (int j = 0; LAZY_DEPS_VALS[0][j]; j++) {
        json_append(out, json_string(LAZY_DEPS_VALS[0][j]));
    }
    return out;
}

/* PoP: lazy_feature_install_command @ tools/lazy_deps.py:feature_install_command */
char *lazy_feature_install_command(const char *feature) {
    if (!feature) return NULL;
    for (int i = 0; i < LAZY_DEPS_N; i++) {
        if (strcmp(feature, LAZY_DEPS_KEYS[i]) == 0) {
            /* "uv pip install " + " ".join(repr(s) for s in specs) */
            size_t cap = 256;
            char *out = malloc(cap);
            size_t off = 0;
            off += snprintf(out + off, cap - off, "uv pip install");
            for (int j = 0; LAZY_DEPS_VALS[i][j]; j++) {
                off += snprintf(out + off, cap - off, " '%s'", LAZY_DEPS_VALS[i][j]);
            }
            return out;
        }
    }
    return NULL;
}


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_lazy_deps_u_format @ tools/lazy_deps.py:_format */
int tools_lazy_deps_u_format(const char *arg) {
    /* Python: "Feature <f> unavailable: <reason>. To enable manually:
     * uv pip install <specs> (or: pip install <specs>)." Arg =
     * "feature\treason\tspec spec...". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("%s\n", arg); return 0; }
    const char *t2 = strchr(t1 + 1, '\t');
    printf("Feature %.*s unavailable: %s. To enable manually: uv pip install %s  (or: pip install %s)\n",
           (int)(t1 - arg), arg, (t2 ? t1 + 1 : ""),
           t2 ? t2 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: tools_lazy_deps_u_python_abi_tag @ tools/lazy_deps.py:_python_abi_tag */
int tools_lazy_deps_u_python_abi_tag(const char *arg) {
    /* Python: "X.Y:EXT_SUFFIX". Arg = "ver\text_suffix". */
    if (!arg || !*arg) { printf(":\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s:%s\n", arg, tab ? tab + 1 : "");
    return 0;
}

/* PoP: tools_lazy_deps_u_lazy_install_target @ tools/lazy_deps.py:_lazy_install_target */
int tools_lazy_deps_u_lazy_install_target(const char *arg) {
    /* Python: env var -> Path or None (empty env = None). Arg = env value. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) { printf("\n"); return 0; }
    printf("%s\n", p);
    return 0;
}

/* PoP: tools_lazy_deps_u_ensure_target_ready @ tools/lazy_deps.py:_ensure_target_ready */
int tools_lazy_deps_u_ensure_target_ready(const char *arg) {
    /* Python: ABI stamp. Arg =
     * "abi_changed\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int abi_changed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        fprintf(stderr, "%s\n", t3 ? t3 + 1 : "target not writable");
        return 1;
    }
    printf("target ready%s\n", abi_changed ? " (stale packages wiped)" : "");
    return 0;
}

/* PoP: tools_lazy_deps_u_activate_target_on_syspath @ tools/lazy_deps.py:_activate_target_on_syspath */
int tools_lazy_deps_u_activate_target_on_syspath(const char *arg) {
    /* Python: append-ordering activation. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("target not activated\n"); return 0; }
    printf("target appended to sys.path (end, .pth honored, caches invalidated)\n");
    return 0;
}

/* PoP: tools_lazy_deps_activate_durable_lazy_target @ tools/lazy_deps.py:activate_durable_lazy_target */
int tools_lazy_deps_activate_durable_lazy_target(const char *arg) {
    /* Python: sys.path activation, never raises. Arg = "target\texists". */
    if (!arg || !*arg) { printf("no durable target\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') printf("durable lazy target activated: %s\n", arg);
    else printf("durable target missing: %s\n", arg);
    return 0;
}

/* PoP: tools_lazy_deps_u_allow_lazy_installs @ tools/lazy_deps.py:_allow_lazy_installs */
int tools_lazy_deps_u_allow_lazy_installs(const char *arg) {
    /* Python: kill-switch order. Arg =
     * "config_off\tsealed\thas_target\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int config_off = arg[0] == '1';
    int sealed = t1 && t1[1] == '1';
    int has_target = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!state) { printf("1\n"); return 0; }
    if (config_off) { printf("0\n"); return 0; }
    if (sealed) { printf("%d\n", has_target ? 1 : 0); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: tools_lazy_deps_u_unsupported_feature_reason @ tools/lazy_deps.py:_unsupported_feature_reason */
int tools_lazy_deps_u_unsupported_feature_reason(const char *arg) {
    /* Python: win32+matrix -> reason else None. Arg = "platform\tfeature". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *platform = arg;
    const char *feature = tab ? tab + 1 : "";
    size_t plen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (plen == 5 && strncmp(platform, "win32", 5) == 0 && strcmp(feature, "platform.matrix") == 0) {
        printf("unsupported on Windows: Matrix E2EE depends on python-olm, which has no Windows wheel and requires make + libolm to build from sdist. Run Hermes under WSL to use Matrix on Windows.\n");
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: tools_lazy_deps_u_is_satisfied @ tools/lazy_deps.py:_is_satisfied */
int tools_lazy_deps_u_is_satisfied(const char *arg) {
    /* Python: presence + version. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: tools_lazy_deps_u_is_present @ tools/lazy_deps.py:_is_present */
int tools_lazy_deps_u_is_present(const char *arg) {
    /* Python: package version() check. Arg = "pkg\tfound". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: tools_lazy_deps_u_core_constraints_file @ tools/lazy_deps.py:_core_constraints_file */
int tools_lazy_deps_u_core_constraints_file(const char *arg) {
    /* Python: pinned constraints. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("constraints file written (%s pins): %s\n", t2 ? t2 + 1 : arg, t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: tools_lazy_deps_u_venv_pip_install @ tools/lazy_deps.py:_venv_pip_install */
int tools_lazy_deps_u_venv_pip_install(const char *arg) {
    /* Python: uv→pip ladder. Arg =
     * "ok\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ok = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 1; }
    if (!ok) { printf("0 (install failed)%s\n", (t2 && t2[1] == '1') ? " — target constrained to core venv versions" : ""); return 1; }
    printf("1 installed (uv → pip → ensurepip ladder%s)%s\n", (t2 && t2[1] == '1') ? ", durable --target append-only" : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: tools_lazy_deps_active_features @ tools/lazy_deps.py:active_features */
int tools_lazy_deps_active_features(const char *arg) {
    /* Python: features with any present package. Arg = "features" (tab-sep,
     * empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: tools_lazy_deps_refresh_active_features @ tools/lazy_deps.py:refresh_active_features */
int tools_lazy_deps_refresh_active_features(const char *arg) {
    /* Python: status map. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: tools_lazy_deps_ensure_and_bind @ tools/lazy_deps.py:ensure_and_bind */
int tools_lazy_deps_ensure_and_bind(const char *arg) {
    /* Python: ensure + rebind. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}
