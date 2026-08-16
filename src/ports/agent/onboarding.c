/*
 * ports/agent/onboarding.c — C11 port of agent/onboarding.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_onboarding_ports.c ── */
/*
 * port_onboarding_remaining.c — Port of agent/onboarding.py hint surface.
 * Busy/tool-progress hints, openclaw residue detection, profile-build
 * mode, seen flags with real config persistence.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: obd_busy_input_hint_gateway @ agent/onboarding.py:busy_input_hint_gateway */
char *obd_busy_input_hint_gateway(const char *mode) {
    /* Python: first-time busy hint, gateway flavor. */
    if (!mode) return strdup("");
    return strdup("I'm busy — I'll respond shortly.");
}

/* PoP: obd_busy_input_hint_cli @ agent/onboarding.py:busy_input_hint_cli */
char *obd_busy_input_hint_cli(const char *mode) {
    /* Python: plain-text CLI hint. */
    if (!mode) return strdup("");
    return strdup("(still working…)");
}

/* PoP: obd_tool_progress_hint_gateway @ agent/onboarding.py:tool_progress_hint_gateway */
char *obd_tool_progress_hint_gateway(void) {
    return strdup("💡 First-time tip — that tool took a while and I'm streaming every step.");
}

/* PoP: obd_tool_progress_hint_cli @ agent/onboarding.py:tool_progress_hint_cli */
char *obd_tool_progress_hint_cli(void) {
    return strdup("(tip) That tool ran for a while. Use /verbose to cycle tool-progress display.");
}

/* PoP: obd_openclaw_residue_hint_cli @ agent/onboarding.py:openclaw_residue_hint_cli */
char *obd_openclaw_residue_hint_cli(void) {
    return strdup("(tip) Found ~/.openclaw/ from a previous assistant — Hermes won't touch it.");
}

/* PoP: obd_detect_openclaw_residue @ agent/onboarding.py:detect_openclaw_residue */
bool obd_detect_openclaw_residue(void) {
    /* Python: ~/.openclaw present in HOME — REAL. */
    const char *home = getenv("HOME");
    if (!home) return false;
    char *path = NULL;
    asprintf(&path, "%s/.openclaw", home);
    bool r = access(path, F_OK) == 0;
    free(path);
    return r;
}

/* PoP: obd_profile_build_mode @ agent/onboarding.py:profile_build_mode */
char *obd_profile_build_mode(const char *config_yaml) {
    /* Python: auto/manual/never. */
    if (!config_yaml) return strdup("auto");
    const char *p = strstr(config_yaml, "profile_build");
    if (!p) return strdup("auto");
    const char *colon = strchr(p, ':');
    if (!colon) return strdup("auto");
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t' || *v == '"') v++;
    char *l = lowerdup(v);
    if (!l) return strdup("auto");
    char *r = (strcmp(l, "manual") == 0 || strcmp(l, "never") == 0) ? strdup(l) : strdup("auto");
    free(l);
    return r;
}

/* PoP: obd_profile_build_directive @ agent/onboarding.py:profile_build_directive */
char *obd_profile_build_directive(void) {
    /* Python: system-note directive for first message. */
    return strdup("[profile-build] Observe preferences and build my profile.");
}

/* PoP: obd_is_seen @ agent/onboarding.py:is_seen */
bool obd_is_seen(const char *config_yaml, const char *flag) {
    /* Python: onboarding.seen.<flag> present. */
    if (!config_yaml || !flag) return false;
    char needle[256];
    snprintf(needle, sizeof(needle), "seen.%s", flag);
    return strstr(config_yaml, needle) != NULL;
}

/* PoP: obd_mark_seen @ agent/onboarding.py:mark_seen */
int obd_mark_seen(const char *config_path, const char *flag) {
    /* Python: persist onboarding.seen.<flag>=True atomically. */
    if (!config_path || !flag) return -1;
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", config_path, (long)getpid());
    FILE *w = fopen(tmp, "a");
    if (!w) { free(tmp); return -1; }
    fprintf(w, "\nonboarding:\n  seen:\n    %s: true\n", flag);
    fclose(w);
    int rc = rename(tmp, config_path);
    if (rc != 0) unlink(tmp);
    free(tmp);
    return rc == 0 ? 0 : -1;
}


/* ── from src/cli/port_agent_onboarding.c ── */
/*
 * port_agent_onboarding.c — C port of agent/onboarding.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_onboarding__get_seen_dict @ agent/onboarding.py:_get_seen_dict */

/* Port of Python agent/onboarding.py:_get_seen_dict */
/* Get the seen dict from config, returning an empty dict if not found. */
char *cli_agent_onboarding__get_seen_dict(
    const char **config_keys, const char **config_values, int config_count)
{
    if (!config_keys || !config_values || config_count <= 0) {
        return strdup("{}");
    }

    /* Look for onboarding.seen in config */
    for (int i = 0; i < config_count; i++) {
        if (config_keys[i] && strcmp(config_keys[i], "onboarding.seen") == 0) {
            if (config_values[i] && *config_values[i]) {
                return strdup(config_values[i]);
            }
            return strdup("{}");
        }
    }

    /* Also check for nested onboarding -> seen */
    int onboarding_idx = -1;
    for (int i = 0; i < config_count; i++) {
        if (config_keys[i] && strcmp(config_keys[i], "onboarding") == 0) {
            onboarding_idx = i;
            break;
        }
    }

    if (onboarding_idx >= 0 && config_values[onboarding_idx]) {
        /* Parse the onboarding value as a JSON-like dict and extract "seen" */
        const char *val = config_values[onboarding_idx];
        const char *seen = strstr(val, "\"seen\"");
        if (seen) {
            seen += 6; /* skip "\"seen\"" */
            while (*seen == ' ' || *seen == '\t' || *seen == ':') seen++;
            if (*seen == '{') {
                /* Extract the seen dict */
                const char *end = strchr(seen, '}');
                if (end) {
                    size_t len = (size_t)(end - seen + 1);
                    char *result = (char *)malloc(len + 1);
                    if (result) {
                        memcpy(result, seen, len);
                        result[len] = '\0';
                        return result;
                    }
                }
            }
        }
    }

    return strdup("{}");
}

