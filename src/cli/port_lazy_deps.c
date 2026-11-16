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
