/*
 * plugin_choices.c — C home for upstream Hermes hermes_cli/plugin_choices.py.
 * PoP: w12_pluginchoi_f29673_options      @ hermes_cli/plugin_choices.py:_options
 * PoP: w12_pluginchoi_f29673_ask_choice   @ hermes_cli/plugin_choices.py:ask_choice
 * PoP: w12_pluginchoi_f29673_clarify_ask  @ hermes_cli/plugin_choices.py:clarify_asker.<locals>.ask
 */
#include "noc_w12_pluginchoi_f29673.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define OPT_VAL 512
#define OPT_LBL 512
#define OPT_SHOWN 1024
#define MAX_OPTS 64

typedef struct {
    char value[OPT_VAL];
    char label[OPT_LBL];
    char shown[OPT_SHOWN];
} Opt;

/* Python str() coercion for the scalar types used by choice fields. */
static void py_str(const json_t *v, char *out, size_t cap)
{
    if (!v || v->type == JSON_NULL) { snprintf(out, cap, "None"); return; }
    if (v->type == JSON_STRING) { snprintf(out, cap, "%s", v->str_val ? v->str_val : ""); return; }
    if (v->type == JSON_BOOL) { snprintf(out, cap, "%s", v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(out, cap, "%lld", (long long)d);
        else snprintf(out, cap, "%.17g", d);
        return;
    }
    snprintf(out, cap, "");
}

static int truthy(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

static char *py_strip(char *s)
{
    size_t n;
    while (*s && isspace((unsigned char)*s)) s++;
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}

static void casefold_ascii(char *s)
{
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + 32);
}

static size_t build_options(const json_t *choices, Opt *opts, size_t max)
{
    size_t count = 0;
    if (!choices || choices->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(choices) && count < max; i++) {
        const json_t *ch = json_get(choices, i);
        char value[OPT_VAL], label[OPT_LBL], desc[OPT_SHOWN], tmp[OPT_SHOWN];
        const json_t *lbl, *ds;
        if (!ch) continue;
        if (ch->type == JSON_STRING) {
            snprintf(opts[count].value, OPT_VAL, "%s", ch->str_val ? ch->str_val : "");
            snprintf(opts[count].label, OPT_LBL, "%s", opts[count].value);
            snprintf(opts[count].shown, OPT_SHOWN, "%s", opts[count].value);
            count++;
            continue;
        }
        if (ch->type != JSON_OBJECT) continue;
        py_str(json_obj_get(ch, "value"), value, sizeof(value));
        lbl = json_obj_get(ch, "label");
        if (truthy(lbl)) py_str(lbl, label, sizeof(label));
        else snprintf(label, sizeof(label), "%s", value);
        ds = json_obj_get(ch, "description");
        if (truthy(ds)) {
            py_str(ds, tmp, sizeof(tmp));
            snprintf(desc, sizeof(desc), "%s", py_strip(tmp));
        } else desc[0] = '\0';
        snprintf(opts[count].value, OPT_VAL, "%s", value);
        snprintf(opts[count].label, OPT_LBL, "%s", label);
        if (desc[0])
            snprintf(opts[count].shown, OPT_SHOWN, "%s \xE2\x80\x94 %s", label, desc);
        else
            snprintf(opts[count].shown, OPT_SHOWN, "%s", label);
        count++;
    }
    return count;
}

void w12_pluginchoi_f29673_options(const json_t *choices, json_t *out)
{
    Opt opts[MAX_OPTS];
    size_t n = build_options(choices, opts, MAX_OPTS);
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_array();
        json_append(row, json_string(opts[i].value));
        json_append(row, json_string(opts[i].label));
        json_append(row, json_string(opts[i].shown));
        json_append(out, row);
    }
}

bool w12_pluginchoi_f29673_ask_choice(const json_t *choices, const char *picked, char *out, size_t cap)
{
    Opt opts[MAX_OPTS];
    size_t n = build_options(choices, opts, MAX_OPTS);
    char typed[OPT_SHOWN], shown[OPT_SHOWN], label[OPT_LBL], value[OPT_VAL];
    char *t;
    if (!picked) picked = "";
    snprintf(typed, sizeof(typed), "%s", picked);
    t = py_strip(typed);
    casefold_ascii(t);
    if (!t[0] || n == 0) return false;
    for (size_t i = 0; i < n; i++) {
        snprintf(shown, sizeof(shown), "%s", opts[i].shown); casefold_ascii(shown);
        snprintf(label, sizeof(label), "%s", opts[i].label); casefold_ascii(label);
        snprintf(value, sizeof(value), "%s", opts[i].value); casefold_ascii(value);
        if (!strcmp(t, shown) || !strcmp(t, label) || !strcmp(t, value)) {
            snprintf(out, cap, "%s", opts[i].value);
            return true;
        }
    }
    return false;
}

bool w12_pluginchoi_f29673_clarify_ask(const json_t *reply, char *out, size_t cap)
{
    const json_t *answers, *answer;
    if (!reply || reply->type != JSON_OBJECT) return false;
    answers = json_obj_get(reply, "answers");
    if (!answers || answers->type != JSON_OBJECT) return false;
    answer = json_obj_get(answers, "q0");
    if (answer && answer->type == JSON_STRING) {
        snprintf(out, cap, "%s", answer->str_val ? answer->str_val : "");
        return true;
    }
    return false;
}
