/* port_hermes_cli_toolset_validation.c
 *
 * Faithful C port of hermes_cli/toolset_validation.py.
 *
 * Pure, side-effect-free helpers: validate the `platform_toolsets` config
 * section and surface unknown / empty toolset mappings as human-readable
 * warnings (mirrors the decoupled-helper pattern used in Python).
 */

/* PoP: toolset_validation_is_known_toolset @ hermes_cli/toolset_validation.py:validate_toolset */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* The set of toolset names the C registry knows about. Mirrors the names
 * registered via registry_set_toolset(...) across src/. Used as the default
 * `is_valid_toolset` predicate (the Python equivalent is toolsets.validate_toolset
 * against get_all_toolsets()). */
static const char *KNOWN_TOOLSETS[] = {
    "browser", "computer_use", "cron", "delegate", "homeassistant",
    "image_gen", "kanban", "memory", "send_message", "video_gen",
    "vision", "voice", NULL
};

/* Default predicate: is `name` a known toolset? */
bool toolset_validation_is_known_toolset(const char *name)
{
    if (!name || !name[0]) return false;
    for (int i = 0; KNOWN_TOOLSETS[i]; i++) {
        if (strcmp(name, KNOWN_TOOLSETS[i]) == 0) return true;
    }
    return false;
}

/* ── internal: append a warning string to a growable array ── */
static void push_warning(char ***out, size_t *n, size_t *cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (*n >= *cap) {
        size_t ncap = (*cap == 0) ? 8 : (*cap * 2);
        char **tmp = realloc(*out, ncap * sizeof(char *));
        if (!tmp) return;
        *out = tmp;
        *cap = ncap;
    }
    (*out)[(*n)++] = strdup(buf);
}

/* PoP: toolset_validation_validate_platform_toolsets @ hermes_cli/toolset_validation.py:validate_platform_toolsets */
/*
 * Port of Python validate_platform_toolsets(platform_toolsets, is_valid_toolset).
 *
 * `platform_toolsets` is a JSON object {"platform": [names] or name, ...}.
 * `is_valid` is the injected predicate returning true for a known toolset name.
 *
 * Returns a heap-allocated array of malloc'd warning strings (count in *out_n);
 * the caller frees each entry and the array. Returns NULL/0 when nothing to
 * validate (non-object / empty input).
 */
void toolset_validation_validate_platform_toolsets(
    const json_t *platform_toolsets,
    bool (*is_valid)(const char *),
    char ***out_warnings, size_t *out_n)
{
    static const char *ZERO_TOOLS =
        "platform_toolsets resolves to zero valid toolsets — the agent will "
        "have no tools. Run `hermes tools` to reconfigure.";

    *out_warnings = NULL;
    *out_n = 0;
    if (!out_warnings || !out_n) return;

    /* Only dict values carry toolset entries; anything else yields no warnings. */
    if (!platform_toolsets || platform_toolsets->type != JSON_OBJECT) return;
    if (platform_toolsets->c.count == 0) return;

    char **warnings = NULL;
    size_t n = 0, cap = 0;
    size_t valid_count = 0;

    for (size_t i = 0; i < platform_toolsets->c.count; i++) {
        const char *platform = platform_toolsets->c.keys[i];
        json_t *raw = platform_toolsets->c.items[i];

        /* Normalize to a list of names. */
        const char *names[64];
        size_t ncount = 0;
        if (raw && raw->type == JSON_ARRAY) {
            for (size_t j = 0; j < raw->c.count && ncount < 64; j++) {
                json_t *e = raw->c.items[j];
                if (e && e->type == JSON_STRING) names[ncount++] = e->str_val;
            }
        } else if (raw && raw->type == JSON_STRING) {
            names[ncount++] = raw->str_val;
        }

        for (size_t k = 0; k < ncount; k++) {
            const char *name = names[k];
            if (!name || !name[0]) continue;
            if (is_valid && is_valid(name)) {
                valid_count++;
                continue;
            }
            /* Unknown toolset — offer "hermes-<platform>" as a suggestion if valid. */
            char suggestion[256];
            snprintf(suggestion, sizeof(suggestion), "hermes-%s", platform);
            const char *hint = (is_valid && is_valid(suggestion))
                ? " — did you mean '" : "";
            if (hint[0]) {
                push_warning(&warnings, &n, &cap,
                    "platform '%s' references unknown toolset '%s' — did you mean '%s'?",
                    platform, name, suggestion);
            } else {
                push_warning(&warnings, &n, &cap,
                    "platform '%s' references unknown toolset '%s'",
                    platform, name);
            }
        }
    }

    if (valid_count == 0) {
        push_warning(&warnings, &n, &cap, "%s", ZERO_TOOLS);
    }

    *out_warnings = warnings;
    *out_n = n;
}
