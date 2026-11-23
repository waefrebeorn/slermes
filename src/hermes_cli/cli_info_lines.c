/*
 * cli_info_lines.c — C port of upstream Hermes hermes_cli/cli_info_lines.py.
 *
 * PoP: w12_cliinfoli_04080c_box @ hermes_cli/cli_info_lines.py:_box
 * PoP: w12_cliinfoli_04080c_toolsets_lines @ hermes_cli/cli_info_lines.py:toolsets_lines
 * PoP: w12_cliinfoli_04080c_platforms_lines @ hermes_cli/cli_info_lines.py:platforms_lines
 *
 * Translation (_t) is stubbed to the identity key; the toolset catalog and the
 * gateway config are injected from the fixture.
 */
#include "noc_w12_cliinfoli_04080c.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINEBUF 8192

static json_t *jget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *jstr(const json_t *o, const char *k, const char *d) {
    return (o && o->type == JSON_OBJECT) ? json_get_str(o, k, d) : d;
}
static int jbool(const json_t *o, const char *k, int d) {
    return (o && o->type == JSON_OBJECT) ? (json_get_bool(o, k, d != 0) ? 1 : 0) : d;
}
static void push(json_t *arr, const char *s) { json_append(arr, json_string(s)); }
static void pushf(json_t *arr, const char *fmt, const char *a, const char *b) {
    char buf[LINEBUF];
    if (b) snprintf(buf, sizeof(buf), fmt, a, b);
    else if (a) snprintf(buf, sizeof(buf), fmt, a);
    else snprintf(buf, sizeof(buf), "%s", fmt);
    push(arr, buf);
}

/* Python f"{s:<width}" / f"{s:>width}" with ASCII padding. */
static void pad_right(const char *s, int width, char *out, size_t cap) {
    size_t n = strlen(s);
    size_t total = n < (size_t)width ? (size_t)width : n;
    size_t i = 0;
    for (; i < n && i + 1 < cap; i++) out[i] = s[i];
    for (; i < total && i + 1 < cap; i++) out[i] = ' ';
    out[i] = '\0';
}
static void pad_left(const char *s, int width, char *out, size_t cap) {
    size_t n = strlen(s);
    size_t pad = n < (size_t)width ? (size_t)width - n : 0;
    size_t i = 0;
    for (; i < pad && i + 1 < cap; i++) out[i] = ' ';
    for (size_t j = 0; j < n && i + 1 < cap; j++, i++) out[i] = s[j];
    out[i] = '\0';
}

static void box_lines(json_t *out, const char *title, int width) {
    if (width < 0) width = 0;
    char rule[LINEBUF], mid[LINEBUF];
    size_t i = 0;
    rule[i++] = '+';
    for (int k = 0; k < width && i + 2 < sizeof(rule); k++) rule[i++] = '-';
    rule[i++] = '+'; rule[i] = '\0';
    size_t pad = (size_t)width > strlen(title) ? (size_t)width - strlen(title) : 0;
    size_t left = pad / 2, right = pad - left;
    size_t j = 0;
    mid[j++] = '|';
    for (size_t k = 0; k < left && j + 2 < sizeof(mid); k++) mid[j++] = ' ';
    for (const char *p = title; *p && j + 2 < sizeof(mid); p++) mid[j++] = *p;
    for (size_t k = 0; k < right && j + 2 < sizeof(mid); k++) mid[j++] = ' ';
    mid[j++] = '|'; mid[j] = '\0';
    push(out, rule); push(out, mid); push(out, rule);
}

struct json_t *w12_cliinfoli_04080c_box(const char *title, int width) {
    json_t *out = json_array();
    box_lines(out, title ? title : "", width);
    return out;
}

static int cmp_str(const void *a, const void *b) {
    const char *const *pa = (const char *const *)a;
    const char *const *pb = (const char *const *)b;
    return strcmp(*pa, *pb);
}

struct json_t *w12_cliinfoli_04080c_toolsets_lines(const struct json_t *spec) {
    json_t *out = json_array();
    push(out, "");
    box_lines(out, "cli.toolsets.header", 58);
    push(out, "");

    json_t *enabled = jget(spec, "enabled");
    json_t *names = jget(spec, "all_names");
    size_t n = (names && names->type == JSON_ARRAY) ? json_len(names) : 0;
    char **sorted = (char **)calloc(n ? n : 1, sizeof(char *));
    if (!sorted) return out;
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(names, i);
        const char *s = (v && v->type == JSON_STRING) ? v->str_val : "";
        sorted[i] = strdup(s ? s : "");
    }
    qsort(sorted, n, sizeof(char *), cmp_str);
    json_t *infos = jget(spec, "toolsets");
    for (size_t i = 0; i < n; i++) {
        json_t *info = jget(infos, sorted[i]);
        if (!info || info->type == JSON_NULL) continue;
        int on = 0;
        if (enabled && enabled->type == JSON_ARRAY) {
            for (size_t e = 0; e < json_len(enabled); e++) {
                const char *en = json_string_value(json_get(enabled, e));
                if (en && !strcmp(en, sorted[i])) { on = 1; break; }
            }
        }
        char marker[8], nm[128], cnt[64], line[LINEBUF];
        snprintf(marker, sizeof(marker), "%s", on ? "(*)" : "   ");
        pad_right(sorted[i], 18, nm, sizeof(nm));
        json_t *c = jget(info, "tool_count");
        if (c && c->type == JSON_STRING) snprintf(cnt, sizeof(cnt), "%s", c->str_val ? c->str_val : "");
        else snprintf(cnt, sizeof(cnt), "%.0f", c ? c->num_val : 0.0);
        char cntpad[64];
        pad_left(cnt, 2, cntpad, sizeof(cntpad));
        snprintf(line, sizeof(line), "  %s %s [%s tools] - %s", marker, nm, cntpad,
                 jstr(info, "description", ""));
        push(out, line);
        free(sorted[i]);
    }
    free(sorted);
    push(out, "");
    push(out, "  cli.toolsets.currently_enabled_legend");
    push(out, "");
    push(out, "  cli.toolsets.tip_enable_all");
    push(out, "  cli.toolsets.example");
    push(out, "");
    return out;
}

struct json_t *w12_cliinfoli_04080c_platforms_lines(const struct json_t *spec) {
    json_t *out = json_array();
    push(out, "");
    box_lines(out, "cli.gateway_status.header", 60);
    push(out, "");
    const char *home = jstr(spec, "home", "");
    char config_file[LINEBUF];
    snprintf(config_file, sizeof(config_file), "%s/config.yaml", home);

    const char *load_error = jstr(spec, "load_error", "");
    if (*load_error) {
    push(out, "  cli.gateway_status.load_error");
    push(out, "");
    push(out, "  cli.gateway_status.to_configure");
    push(out, "    cli.gateway_status.step_env_vars");
    push(out, "       TELEGRAM_BOT_TOKEN=your_token");
    push(out, "       DISCORD_BOT_TOKEN=your_token");
    push(out, "    cli.gateway_status.step_config");
    push(out, "");
    return out;
    }

    push(out, "  cli.gateway_status.platform_config_header");
    char dashes[64];
    size_t k = 0;
    dashes[k++] = ' '; dashes[k++] = ' ';
    for (int i = 0; i < 55 && k + 1 < sizeof(dashes); i++) dashes[k++] = '-';
    dashes[k] = '\0';
    push(out, dashes);

    static const char *const keys[4] = { "telegram", "discord", "slack", "whatsapp" };
    static const char *const names[4] = { "Telegram", "Discord", "Slack", "WhatsApp" };
    json_t *cfg = jget(spec, "config");
    json_t *platforms = jget(cfg, "platforms");
    for (int i = 0; i < 4; i++) {
        json_t *pc = jget(platforms, keys[i]);
        char nm[64], line[LINEBUF];
        pad_right(names[i], 12, nm, sizeof(nm));
        if (pc && pc->type == JSON_OBJECT && jbool(pc, "enabled", 0)) {
            json_t *hn = jget(pc, "home");
            const char *chn = (hn && hn->type == JSON_STRING) ? (hn->str_val ? hn->str_val : "") : NULL;
            char homesuffix[LINEBUF];
            if (chn) snprintf(homesuffix, sizeof(homesuffix), " \xe2\x86\x92 %s", chn);
            else homesuffix[0] = '\0';
            snprintf(line, sizeof(line), "    \xe2\x9c\x93 %s cli.gateway_status.enabled%s", nm, homesuffix);
        } else {
            snprintf(line, sizeof(line), "    \xe2\x97\x8b %s cli.gateway_status.not_configured", nm);
        }
        push(out, line);
    }
    push(out, "");
    push(out, "  cli.gateway_status.conversations_persist");
    push(out, "");
    push(out, "  cli.gateway_status.to_start");
    push(out, "    python cli.py --gateway");
    push(out, "");
    char cf[LINEBUF];
    snprintf(cf, sizeof(cf), "  cli.gateway_status.config_file");
    push(out, cf);
    (void)config_file;
    push(out, "");
    return out;
}
