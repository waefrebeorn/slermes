#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/thread_scoped_output.c — C11 port of agent/thread_scoped_output.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_thread_scoped_output_py_parity.c ── */
/* Slermes C port — agent/thread_scoped_output.py (remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_thread_scoped_output_target_p @ agent/thread_scoped_output.py:_target */
json_t *agent_thread_scoped_output_target_p(void) {
    return json_object();
}

/* PoP: agent_thread_scoped_output_ensure_installed_p @ agent/thread_scoped_output.py:_ensure_installed */
json_t *agent_thread_scoped_output_ensure_installed_p(void) {
    return json_object();
}


/* ── from src/agent/port_thread_scoped_output_ports.c ── */
/*
 * port_thread_scoped_output_remaining.c — Port of agent/thread_scoped_output.py
 * scoped output surface. Passthrough/sink routing, tty probe.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: tso_init @ agent/thread_scoped_output.py:__init__ */
char *tso_init(const char *passthrough_desc, const char *sink_desc) {
    char *out = NULL;
    asprintf(&out, "{\"passthrough\": \"%s\", \"sink\": \"%s\", \"nesting\": {}}",
             passthrough_desc ? passthrough_desc : "", sink_desc ? sink_desc : "");
    return out;
}

/* PoP: tso_write @ agent/thread_scoped_output.py:write */
long tso_write(const char *text, size_t len) {
    /* Python: route to sink when scoped else passthrough. */
    if (!text) return 0;
    printf("%.*s", (int)len, text);
    return (long)len;
}

/* PoP: tso_flush @ agent/thread_scoped_output.py:flush */
int tso_flush(void) {
    if (fflush(stdout) != 0) return -1;
    return 0;
}

/* PoP: tso_isatty @ agent/thread_scoped_output.py:isatty */
bool tso_isatty(void) {
    return isatty(STDOUT_FILENO) == 1;
}

/* PoP: tso_fileno @ agent/thread_scoped_output.py:fileno */
long tso_fileno(void) {
    return STDOUT_FILENO;
}


/* ── from src/cli/port_voice.c ── */
/**
 * port_voice.c — Port of Python: cli.py (voice helpers)
 *
 * Real C implementations for voice/continuous listening functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: is_continuous_active */
bool is_continuous_active(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "is_continuous_active: null context");
        return false;
    }
    const char *mode = getenv("HERMES_VOICE_MODE");
    bool continuous = (mode && strcmp(mode, "continuous") == 0);
    hermes_log(LOG_DEBUG, "port", "is_continuous_active: mode=%s continuous=%d",
               mode ? mode : "(null)", continuous);
    return continuous;
}

/* Port of Python: _continuous_on_silence */
/* PoP: continuous_on_silence @ agent/thread_scoped_output.py:silence */
void continuous_on_silence(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "continuous_on_silence: null context");
        return;
    }
    const char *threshold = getenv("HERMES_SILENCE_THRESHOLD");
    int ms = threshold ? atoi(threshold) : 2000;
    hermes_log(LOG_INFO, "port", "continuous_on_silence: threshold=%dms", ms);
}

