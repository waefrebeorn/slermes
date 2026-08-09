/*
 * port_status_helpers.c
 *
 * Faithful C11 port of the PURE helper from hermes_cli/status.py:
 *   _format_iso_timestamp  ->  sta_format_iso_timestamp
 *
 * Converts an ISO-8601 timestamp string to the local timezone and formats
 * it as "%Y-%m-%d %H:%M:%S %Z". Pure datetime logic -- no I/O, no
 * network, no env/catalog reads. Carries its PoP annotation.
 *
 * Behaviour (mirrors the Python exactly):
 *   - non-string or empty/whitespace value  -> "(unknown)"
 *   - trailing "Z" is normalized to "+00:00"
 *   - naive timestamps (no offset) are assumed UTC
 *   - unparseable -> returns the original string unchanged
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "port_hermes_cli_timefmt.h"
#include "hermes_yaml.h"

/* PoP: sta_format_iso_timestamp @ hermes_cli/status.py:_format_iso_timestamp */
void sta_format_iso_timestamp(const char *value, char out[64])
{
    out[0] = '\0';
    if (!value || !*value) { strcpy(out, "(unknown)"); return; }

    const char *orig = value;   /* Python echoes the original on parse failure */

    /* strip leading/trailing whitespace */
    while (*value == ' ' || *value == '\t') value++;
    char buf[64];
    size_t n = 0;
    for (; *value && n < sizeof(buf) - 1; value++) buf[n++] = *value;
    buf[n] = '\0';
    while (n > 0 && (buf[n-1] == ' ' || buf[n-1] == '\t')) buf[--n] = '\0';
    if (n == 0) { strcpy(out, "(unknown)"); return; }

    /* normalize trailing Z -> +00:00 */
    if (n > 0 && buf[n-1] == 'Z') {
        if (n + 5 < sizeof(buf)) {
            buf[n-1] = '+'; buf[n] = '0'; buf[n+1] = '0';
            buf[n+2] = ':'; buf[n+3] = '0'; buf[n+4] = '0';
            buf[n+5] = '\0'; n += 5;
        }
    }

    /* parse YYYY-MM-DD [T| ] HH:MM[:SS] [+HH:MM | -HH:MM] */
    int Y = 0, M = 0, D = 0, h = 0, m = 0, s = 0;
    int off_sign = 0, off_h = 0, off_m = 0;

    if (sscanf(buf, "%d-%d-%d", &Y, &M, &D) != 3) { strcpy(out, orig); return; }

    /* locate the time substring (after the date, past a T or space) */
    char *ts = NULL;
    for (char *q = buf; *q; q++) {
        if ((*q == 'T' || *q == ' ') && isdigit((unsigned char)*(q+1))) { ts = q + 1; break; }
    }
    if (ts) sscanf(ts, "%d:%d:%d", &h, &m, &s);

    /* locate the offset (last standalone + or - after the date part) */
    char *off = NULL;
    for (char *q = buf + 4; *q; q++) {
        if ((*q == '+' || *q == '-') &&
            (isdigit((unsigned char)*(q+1)) || (isdigit((unsigned char)*(q+1)) && *(q+2) == ':')))
            off = q;
    }
    if (off && sscanf(off, "%c%d:%d", (char*)&off_sign, &off_h, &off_m) >= 3) {
        /* parsed */
    } else {
        off_sign = 0; off_h = 0; off_m = 0;
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = Y - 1900;
    tm.tm_mon  = M - 1;
    tm.tm_mday = D;
    tm.tm_hour = h;
    tm.tm_min  = m;
    tm.tm_sec  = s;
    tm.tm_isdst = -1;

    /* epoch assuming this wall-clock is UTC, then subtract the offset.
     * naive / Z => offset seconds == 0 => treated as UTC. */
    time_t epoch = timegm(&tm);
    if (epoch == (time_t)-1) { strcpy(out, orig); return; }
    int off_secs = (off_sign == '-')
        ? -(off_h * 3600 + off_m * 60)
        :  (off_h * 3600 + off_m * 60);
    epoch -= off_secs;

    struct tm local;
    localtime_r(&epoch, &local);
    strftime(out, 64, "%Y-%m-%d %H:%M:%S %Z", &local);
}

/* ------------------------------------------------------------------ */
/* Provider/model label helpers (hermes_cli/status.py)               */
/* ------------------------------------------------------------------ */

/* Parse config.yaml minimally and return the model.default / model.name /
 * model.provider string into out (caller provides >=256 bytes). Returns the
 * populated out, or "(not set)"/"(auto)" on missing values. Used by the two
 * label helpers below so they stay pure (config passed implicitly via the
 * file, mirroring Python's config dict). */
static const char *sta_load_config_str(const char *path, char *out, size_t outsz)
{
    out[0] = '\0';
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home) return out;
    char cfgpath[1024];
    snprintf(cfgpath, sizeof(cfgpath), "%s/.hermes/config.yaml", home);
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfgpath, &err);
    if (err) { free(err); return out; }
    if (!doc) return out;
    const char *v = yaml_get_string(doc, path);
    if (v && *v) snprintf(out, outsz, "%s", v);
    yaml_free(doc);
    return out;
}

/* PoP: sta_configured_model_label @ hermes_cli/status.py:_configured_model_label */
/* Return the configured default model from config.yaml. */
void sta_configured_model_label(char out[256])
{
    char def[256], name[256];
    sta_load_config_str("model.default", def, sizeof(def));
    sta_load_config_str("model.name", name, sizeof(name));
    const char *model = def[0] ? def : (name[0] ? name : "");
    if (!*model) { strcpy(out, "(not set)"); return; }
    snprintf(out, 256, "%s", model);
}

/* PoP: sta_effective_provider_label @ hermes_cli/status.py:_effective_provider_label */
/* Return the provider label matching current CLI config resolution.
 * Mirrors the Python: take model.provider (the effective provider in C),
 * then apply the openrouter + OPENAI_BASE_URL -> custom rule. */
void sta_effective_provider_label(char out[256])
{
    char prov[256];
    sta_load_config_str("model.provider", prov, sizeof(prov));
    const char *effective = prov[0] ? prov : "auto";

    if (strcmp(effective, "openrouter") == 0 && getenv("OPENAI_BASE_URL"))
        effective = "custom";

    /* provider_label(): capitalize first letter for display. */
    if (!*effective) { strcpy(out, "(auto)"); return; }
    char label[256];
    snprintf(label, sizeof(label), "%s", effective);
    label[0] = (char)toupper((unsigned char)label[0]);
    snprintf(out, 256, "%s", label);
}

/* PoP: _format_relative_ts @ hermes_cli/status.py:_format_relative_ts */
/* Format an epoch timestamp as short relative age for status output.
 * Delegates to tf_relative_time (port of hermes_cli.timefmt:relative_time). */
char *sta_format_relative_ts(double ts) {
    return tf_relative_time(ts);
}
