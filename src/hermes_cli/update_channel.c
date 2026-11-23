/*
 * update_channel.c — C home for upstream Hermes hermes_cli/update_channel.py.
 *
 * PoP: w12_updatechan_790d05_is_canary_tag @ hermes_cli/update_channel.py:is_canary_tag
 * PoP: w12_updatechan_790d05_canary_timestamp @ hermes_cli/update_channel.py:canary_timestamp
 * PoP: w12_updatechan_790d05_canary_tag_for_date @ hermes_cli/update_channel.py:canary_tag_for_date
 * PoP: w12_updatechan_790d05_install_records @ hermes_cli/update_channel.py:_install_records
 * PoP: w12_updatechan_790d05_package_channel @ hermes_cli/update_channel.py:_package_channel
 *
 * The pure tag/navigation layers are ported verbatim.  ValueError on an
 * invalid version/timestamp is represented as JSON null (the oracle maps the
 * live exception to None).
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w12_updatechan_790d05.h"

/* Python str.strip(): ASCII whitespace both ends. buf must hold strlen(s)+1. */
static void py_strip(const char *s, char *buf, size_t cap) {
    size_t n = 0;
    if (s) { while (s[n] && n + 1 < cap) { buf[n] = s[n]; n++; } }
    buf[n] = '\0';
    size_t b = 0, e = n;
    while (b < e && isspace((unsigned char)buf[b])) b++;
    while (e > b && isspace((unsigned char)buf[e - 1])) e--;
    if (b > 0) memmove(buf, buf + b, e - b);
    buf[e - b] = '\0';  /* NUL at the NEW length after shortening */
}

/* v<major>.<minor>.<patch> with Python's digit rules. */
static int parse_stable_core(const char *s, const char **end_out) {
    const char *p = s;
    if (*p != 'v') return 0;
    p++;
    /* (?:0|[1-9]\d{0,2}) */
    if (*p == '0') p++;
    else if (*p >= '1' && *p <= '9') {
        int n = 1; p++;
        while (isdigit((unsigned char)*p) && n < 3) { p++; n++; }
    } else return 0;
    if (*p != '.') return 0;
    p++;
    /* (?:0|[1-9]\d*) */
    if (*p == '0') p++;
    else if (*p >= '1' && *p <= '9') { p++; while (isdigit((unsigned char)*p)) p++; }
    else return 0;
    if (*p != '.') return 0;
    p++;
    if (*p == '0') p++;
    else if (*p >= '1' && *p <= '9') { p++; while (isdigit((unsigned char)*p)) p++; }
    else return 0;
    *end_out = p;
    return 1;
}

static int fullmatch_stable(const char *s) {
    const char *e = NULL;
    return parse_stable_core(s, &e) && *e == '\0';
}

static int take_digits(const char *p, int n, const char **end_out) {
    for (int i = 0; i < n; i++) if (!isdigit((unsigned char)p[i])) return 0;
    *end_out = p + n;
    return 1;
}

static int fullmatch_canary(const char *s) {
    const char *e = NULL;
    if (!parse_stable_core(s, &e)) return 0;
    if (strncmp(e, "+canary.20", 10) != 0) return 0;
    e += 10;
    if (!take_digits(e, 6, &e)) return 0;
    if (*e != 'T') return 0;
    e++;
    if (!take_digits(e, 6, &e)) return 0;
    if (*e != 'Z') return 0;
    e++;
    return *e == '\0';
}

/* 20\d{6}T\d{6}Z */
static int fullmatch_date(const char *s) {
    const char *e = s;
    if (e[0] != '2' || e[1] != '0') return 0;
    e += 2;
    if (!take_digits(e, 6, &e)) return 0;
    if (*e != 'T') return 0;
    e++;
    if (!take_digits(e, 6, &e)) return 0;
    if (*e != 'Z') return 0;
    e++;
    return *e == '\0';
}

json_t *w12_updatechan_790d05_is_canary_tag(const char *tag) {
    char buf[4096];
    py_strip(tag, buf, sizeof(buf));
    return json_bool(fullmatch_canary(buf) ? true : false);
}

json_t *w12_updatechan_790d05_canary_timestamp(const char *tag) {
    char buf[4096];
    py_strip(tag, buf, sizeof(buf));
    if (!fullmatch_canary(buf)) return json_null();
    const char *p = strstr(buf, "+canary.");
    if (!p) return json_null();
    return json_string(p + 8);
}

json_t *w12_updatechan_790d05_canary_tag_for_date(const char *version, const char *date_utc) {
    const char *v = version ? version : "";
    const char *core = v;
    if (*core == 'v') core++;                 /* str.removeprefix("v") */
    size_t need = strlen(core) + strlen(date_utc ? date_utc : "") + 64;
    char *stable = (char *)malloc(need);
    if (!stable) return json_null();
    snprintf(stable, need, "v%s", core);
    int ok = fullmatch_stable(stable) && fullmatch_date(date_utc ? date_utc : "");
    free(stable);
    if (!ok) return json_null();
    char *out = (char *)malloc(need);
    if (!out) return json_null();
    snprintf(out, need, "v%s+canary.%s", core, date_utc ? date_utc : "");
    json_t *res = json_string(out);
    free(out);
    return res;
}

json_t *w12_updatechan_790d05_install_records(const json_t *config) {
    json_t *out = json_object();
    if (!config || config->type != JSON_OBJECT) return out;
    json_t *update = json_obj_get(config, "update");
    if (!update || update->type != JSON_OBJECT) return out;
    json_t *installs = json_obj_get(update, "installs");
    if (!installs || installs->type != JSON_OBJECT) return out;
    return json_copy(installs);
}

json_t *w12_updatechan_790d05_package_channel(const json_t *stamp) {
    const char *payload = json_get_str(stamp, "payload", NULL);
    if (payload && (!strcmp(payload, "bundled") || !strcmp(payload, "light") ||
                    !strcmp(payload, "runtime"))) return json_bool(true);
    const char *mech = json_get_str(stamp, "updateMechanism", NULL);
    if (mech && (!strcmp(mech, "electron-updater") || !strcmp(mech, "app-installer") ||
                 !strcmp(mech, "microsoft-store"))) return json_bool(true);
    return json_bool(false);
}

void hermes_cli_update_channel_port_pending(void) { (void)0; }
