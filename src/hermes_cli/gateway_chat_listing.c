/*
 * gateway_chat_listing.c -- real C11 port of `--list-tools` / `--list-toolsets`
 * on the gateway path of upstream Hermes hermes_cli/gateway_chat_listing.py.
 *
 * PoP: w21_gatewaycha_3d5133_render @ hermes_cli/gateway_chat_listing.py:print_tool_listing
 *
 * The registry (`model_tools`), the toolset table (`toolsets`), this profile's
 * config and the i18n catalog are config/registry seams and arrive as values;
 * the selection and every printed byte are the port's own work.
 */
#include "hermes_json.h"
#include "noc_w21_gatewaycha_3d5133.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GA_MAX_NAMES 64
#define GA_NAME_LEN 128
#define GA_DESC_LEN 512

static char *ga_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int ga_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static void ga_strip_copy(const char *value, char *out, size_t cap) {
    size_t len, start = 0, j = 0;
    if (!value) value = "";
    len = strlen(value);
    while (start < len && isspace((unsigned char)value[start])) start++;
    while (len > start && isspace((unsigned char)value[len - 1])) len--;
    for (; start < len && j + 1 < cap; start++) out[j++] = value[start];
    out[j] = '\0';
}

static int ga_cmp_str(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

/* `sorted({name.strip() for name in text.split(",") if name.strip()})` */
static int ga_split_set(const char *text, char names[][GA_NAME_LEN], int cap) {
    int n = 0, i;
    const char *p = text ? text : "";
    while (*p || n == 0) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        char item[GA_NAME_LEN];
        if (len >= sizeof(item)) len = sizeof(item) - 1;
        memcpy(item, p, len);
        item[len] = '\0';
        ga_strip_copy(item, item, sizeof(item));
        if (item[0] && n < cap) {
            int dup = 0;
            for (i = 0; i < n; i++) {
                if (!strcmp(names[i], item)) { dup = 1; break; }
            }
            if (!dup) snprintf(names[n++], GA_NAME_LEN, "%s", item);
        }
        if (!comma) break;
        p = comma + 1;
    }
    qsort(names, (size_t)n, GA_NAME_LEN, ga_cmp_str);
    return n;
}

/* `",".join(map(str, value))` for the array shape of args.toolsets. */
static void ga_join_array(const json_t *arr, char *out, size_t cap) {
    size_t i, used = 0;
    out[0] = '\0';
    if (!arr || arr->type != JSON_ARRAY) return;
    for (i = 0; i < arr->c.count; i++) {
        json_t *v = arr->c.items[i];
        char buf[GA_NAME_LEN];
        const char *text;
        if (v && v->type == JSON_STRING) text = v->str_val ? v->str_val : "";
        else if (v && v->type == JSON_BOOL) text = v->bool_val ? "True" : "False";
        else if (v && v->type == JSON_NUMBER) {
            snprintf(buf, sizeof(buf), "%g", v->num_val);
            text = buf;
        } else text = "";
        if (used + strlen(text) + 2 >= cap) break;
        if (used) { out[used++] = ','; }
        memcpy(out + used, text, strlen(text));
        used += strlen(text);
        out[used] = '\0';
    }
}

static void ga_add_line(json_t *lines, const char *fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    json_append(lines, json_string(buf));
}

/* `t(key, **kw)` from the fixture's message catalog. */
static const char *ga_msg(const json_t *i18n, const char *key, const char *fallback) {
    json_t *v = (i18n && i18n->type == JSON_OBJECT) ? json_obj_get(i18n, key) : NULL;
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return fallback ? fallback : key;
}

/* `t('cli.tools.total', count=str(len(tools)))` with a `{count}` template. */
static void ga_total_line(char *out, size_t cap, const char *template_text, size_t count) {
    char number[32];
    const char *mark = NULL, *p;
    size_t used = 0;
    snprintf(number, sizeof(number), "%llu", (unsigned long long)count);
    mark = strstr(template_text, "{count}");
    if (!mark) {
        snprintf(out, cap, "%s", template_text);
        return;
    }
    for (p = template_text; *p && used + 1 < cap; p++) {
        if (p == mark) {
            size_t k;
            for (k = 0; number[k] && used + 1 < cap; k++) out[used++] = number[k];
            p += strlen("{count}") - 1;
        } else {
            out[used++] = *p;
        }
    }
    out[used] = '\0';
}

typedef struct {
    char name[GA_NAME_LEN];
    char desc[GA_DESC_LEN];
    char toolset[GA_NAME_LEN];      /* "unknown" when the registry has no home */
} ga_tool;

static int ga_cmp_tool(const void *a, const void *b) {
    const ga_tool *x = (const ga_tool *)a;
    const ga_tool *y = (const ga_tool *)b;
    return strcmp(x->name, y->name);
}

json_t *w21_gatewaycha_3d5133_render(const json_t *case_) {
    json_t *lines = json_array();
    json_t *out = json_object();
    json_t *requested = json_obj_get(case_, "toolsets");
    json_t *i18n = json_obj_get(case_, "i18n");
    char enabled[GA_MAX_NAMES][GA_NAME_LEN];
    int nenabled = 0, took_explicit = 0;

    if (!case_ || case_->type != JSON_OBJECT) case_ = json_object();

    /* --- _selection --------------------------------------------------- */
    if (requested && requested->type == JSON_ARRAY) {
        char joined[2048];
        ga_join_array(requested, joined, sizeof(joined));
        if (joined[0] && strspn(joined, " \t\r\n") != strlen(joined)) {
            nenabled = ga_split_set(joined, enabled, GA_MAX_NAMES);
            took_explicit = nenabled > 0;
        }
    } else if (requested && requested->type == JSON_STRING && ga_truthy(requested)) {
        nenabled = ga_split_set(requested->str_val, enabled, GA_MAX_NAMES);
        took_explicit = 1;
    }
    if (!took_explicit) {
        json_t *config = json_obj_get(case_, "config");
        json_t *platform_tools = json_obj_get(config, "platform_tools");
        size_t i;
        nenabled = 0;
        if (platform_tools && platform_tools->type == JSON_ARRAY) {
            for (i = 0; i < platform_tools->c.count && nenabled < GA_MAX_NAMES; i++) {
                json_t *v = platform_tools->c.items[i];
                if (v && v->type == JSON_STRING) {
                    snprintf(enabled[nenabled++], GA_NAME_LEN, "%s", v->str_val ? v->str_val : "");
                }
            }
            qsort(enabled, (size_t)nenabled, GA_NAME_LEN, ga_cmp_str);
        }
    }

    if (ga_truthy(json_obj_get(case_, "list_tools"))) {
        json_t *tools = json_obj_get(case_, "tools");
        ga_tool rows[GA_MAX_NAMES];
        int nrows = 0;
        size_t i;

        if (tools && tools->type == JSON_ARRAY) {
            for (i = 0; i < tools->c.count && nrows < GA_MAX_NAMES; i++) {
                json_t *tool = tools->c.items[i];
                const char *name = json_get_str(tool, "name", "");
                const char *raw = json_get_str(tool, "description", "");
                char *first;
                const char *cut;
                ga_tool *row = &rows[nrows++];
                snprintf(row->name, sizeof(row->name), "%s", name ? name : "");
                first = ga_dup(raw);
                if (first) {
                    char *nl = strchr(first, '\n');
                    if (nl) *nl = '\0';
                }
                snprintf(row->desc, sizeof(row->desc), "%s", first ? first : "");
                free(first);
                cut = strstr(row->desc, ". ");
                if (cut) {
                    size_t keep = (size_t)(cut - row->desc) + 1;   /* [:.index(". ")+1] */
                    row->desc[keep] = '\0';
                }
                {
                    json_t *home = json_obj_get(tool, "toolset");
                    if (home && home->type == JSON_STRING && ga_truthy(home)) {
                        snprintf(row->toolset, sizeof(row->toolset), "%s", home->str_val);
                    } else {
                        snprintf(row->toolset, sizeof(row->toolset), "unknown");
                    }
                }
            }
        }
        if (nrows > 1) qsort(rows, (size_t)nrows, sizeof(rows[0]), ga_cmp_tool);

        if (nrows == 0) {
            ga_add_line(lines, "%s", ga_msg(i18n, "cli.tools.none_available",
                                            "cli.tools.none_available"));
            json_set(out, "lines", lines);
            json_set(out, "rc", json_number(0));
            return out;
        }

        ga_add_line(lines, "%s", ga_msg(i18n, "cli.tools.header", "cli.tools.header"));
        /* grouped: sorted toolset names, tools in name order inside each group */
        {
            char groups[GA_MAX_NAMES][GA_NAME_LEN];
            int ngroups = 0, g, r;
            for (r = 0; r < nrows; r++) {
                int seen = 0;
                for (g = 0; g < ngroups; g++) {
                    if (!strcmp(groups[g], rows[r].toolset)) { seen = 1; break; }
                }
                if (!seen && ngroups < GA_MAX_NAMES) {
                    snprintf(groups[ngroups++], GA_NAME_LEN, "%s", rows[r].toolset);
                }
            }
            qsort(groups, (size_t)ngroups, GA_NAME_LEN, ga_cmp_str);
            for (g = 0; g < ngroups; g++) {
                ga_add_line(lines, "");
                ga_add_line(lines, "  [%s]", groups[g]);
                for (r = 0; r < nrows; r++) {
                    if (strcmp(rows[r].toolset, groups[g])) continue;
                    ga_add_line(lines, "    * %-20s - %s", rows[r].name, rows[r].desc);
                }
            }
        }
        {
            char total[1024];
            ga_total_line(total, sizeof(total),
                          ga_msg(i18n, "cli.tools.total", "cli.tools.total"), (size_t)nrows);
            ga_add_line(lines, "");
            ga_add_line(lines, "  %s", total);
        }
    } else {
        json_t *all = json_obj_get(case_, "all_toolsets");
        json_t *info = json_obj_get(case_, "toolsets_info");
        char names[GA_MAX_NAMES][GA_NAME_LEN];
        int nnames = 0, k;
        size_t i;

        if (all && all->type == JSON_ARRAY) {
            for (i = 0; i < all->c.count && nnames < GA_MAX_NAMES; i++) {
                json_t *v = all->c.items[i];
                if (v && v->type == JSON_STRING) {
                    snprintf(names[nnames++], GA_NAME_LEN, "%s", v->str_val ? v->str_val : "");
                }
            }
        }
        qsort(names, (size_t)nnames, GA_NAME_LEN, ga_cmp_str);
        ga_add_line(lines, "%s", ga_msg(i18n, "cli.toolsets.header", "cli.toolsets.header"));
        for (k = 0; k < nnames; k++) {
            json_t *entry = (info && info->type == JSON_OBJECT) ? json_obj_get(info, names[k]) : NULL;
            const char *marker;
            int on = 0, e;
            if (!entry || entry->type != JSON_OBJECT) continue;
            for (e = 0; e < nenabled; e++) {
                if (!strcmp(enabled[e], names[k])) { on = 1; break; }
            }
            marker = on ? "(*)" : "   ";
            ga_add_line(lines, "  %s %-18s [%2d tools] - %s", marker, names[k],
                        (int)json_get_num(entry, "tool_count", 0.0),
                        json_get_str(entry, "description", ""));
        }
        ga_add_line(lines, "");
        ga_add_line(lines, "  %s", ga_msg(i18n, "cli.toolsets.currently_enabled_legend",
                                          "cli.toolsets.currently_enabled_legend"));
    }

    json_set(out, "lines", lines);
    json_set(out, "rc", json_number(0));
    return out;
}
