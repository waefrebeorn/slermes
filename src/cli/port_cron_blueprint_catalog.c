/*
 * port_cron_blueprint_catalog.c — C port of cron/blueprint_catalog.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: cli_cron_blueprint_catalog___post_init__ @ cron/blueprint_catalog.py:__post_init__ */
/* PoP: cli_cron_blueprint_catalog_get_blueprint @ cron/blueprint_catalog.py:get_blueprint */
/* PoP: cli_cron_blueprint_catalog_blueprint_form_schema @ cron/blueprint_catalog.py:blueprint_form_schema */
/* PoP: cli_cron_blueprint_catalog_blueprint_slash_command @ cron/blueprint_catalog.py:blueprint_slash_command */
/* PoP: cli_cron_blueprint_catalog_blueprint_deeplink @ cron/blueprint_catalog.py:blueprint_deeplink */
/* PoP: cli_cron_blueprint_catalog_blueprint_catalog_entry @ cron/blueprint_catalog.py:blueprint_catalog_entry */
/* PoP: cli_cron_blueprint_catalog__resolve_schedule @ cron/blueprint_catalog.py:_resolve_schedule */
/* PoP: cli_cron_blueprint_catalog_fill_blueprint @ cron/blueprint_catalog.py:fill_blueprint */

/*
 * Data structures matching Python dataclasses
 */

/* BlueprintSlot: a single fillable field */
typedef struct {
    char   name[64];
    char   type[16];     /* "time", "enum", "text", "weekdays" */
    char   label[128];
    char   default_val[256];
    int    options_count;
    char   options[8][128];
    bool   optional;
    bool   strict;
    char   help[256];
} blueprint_slot_t;

/* AutomationBlueprint: a parameterized automation template */
typedef struct {
    char              key[64];
    char              title[128];
    char              description[512];
    char              category[32];
    char              schedule_template[256];
    char              prompt_template[1024];
    int               slots_count;
    blueprint_slot_t   slots[16];
    char              deliver_default[32];
} automation_blueprint_t;

/* WEEKDAY_PRESETS mapping */
static const struct { const char *name; const char *dow; } weekday_presets[] = {
    {"everyday", "*"},
    {"weekdays", "1-5"},
    {"weekends", "0,6"},
    {NULL, NULL}
};

/* _DAY_TO_DOW mapping */
static const struct { const char *name; const char *dow; } day_to_dow[] = {
    {"sunday", "0"}, {"monday", "1"}, {"tuesday", "2"}, {"wednesday", "3"},
    {"thursday", "4"}, {"friday", "5"}, {"saturday", "6"},
    {NULL, NULL}
};

/*
 * Static blueprint catalog — mirrors Python CATALOG list.
 * Each entry is a pre-built automation_blueprint_t.
 */

/* Slot helper macros for readability */
#define SLOT_TIME(_name, _label, _default) \
    { .name = _name, .type = "time", .label = _label, .default_val = _default, \
      .options_count = 0, .optional = false, .strict = true, .help = "24h local time, e.g. 08:00" }

#define SLOT_DELIVER() \
    { .name = "deliver", .type = "enum", .label = "Where to deliver?", \
      .default_val = "origin", .options_count = 5, \
      .options = {"origin", "local", "telegram", "discord", "email"}, \
      .optional = false, .strict = false, \
      .help = "origin = the chat you set this up from" }

#define SLOT_ENUM(_name, _label, _default, _nopts, ...) \
    { .name = _name, .type = "enum", .label = _label, .default_val = _default, \
      .options_count = _nopts, .optional = false, .strict = true, .help = "" }

#define SLOT_TEXT(_name, _label, _default) \
    { .name = _name, .type = "text", .label = _label, .default_val = _default, \
      .options_count = 0, .optional = false, .strict = true, .help = "" }

#define SLOT_WEEKDAYS(_name, _label, _default) \
    { .name = _name, .type = "weekdays", .label = _label, .default_val = _default, \
      .options_count = 3, .options = {"everyday", "weekdays", "weekends"}, \
      .optional = false, .strict = true, .help = "" }

/* Forward declaration */
static const automation_blueprint_t *find_blueprint(const char *key);

/*
 * __post_init__: Validate slot type is one of the allowed types.
 *
 * Python: if self.type not in _SLOT_TYPES: raise ValueError
 * In C: p1 = pointer to blueprint_slot_t, returns 0 on success, -1 on error.
 */
void* cli_cron_blueprint_catalog___post_init__(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;

    blueprint_slot_t *slot = (blueprint_slot_t *)p1;
    if (!slot) return (void *)(intptr_t)(-1);

    static const char *valid_types[] = {"time", "enum", "text", "weekdays", NULL};
    for (int i = 0; valid_types[i]; i++) {
        if (strcmp(slot->type, valid_types[i]) == 0) {
            hermes_log(LOG_DEBUG, "port",
                       "post_init: slot '%s' type '%s' OK", slot->name, slot->type);
            return (void *)(intptr_t)0;
        }
    }

    hermes_log(LOG_ERROR, "port",
               "post_init: unknown slot type '%s' (slot '%s')",
               slot->type, slot->name);
    return (void *)(intptr_t)(-1);
}

/*
 * get_blueprint: Look up a blueprint by key.
 *
 * Python: return _CATALOG_BY_KEY.get(key)
 * In C: p1 = key string, returns pointer to blueprint or NULL.
 */
void* cli_cron_blueprint_catalog_get_blueprint(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;

    const char *key = (const char *)p1;
    if (!key || !key[0]) return NULL;

    const automation_blueprint_t *bp = find_blueprint(key);
    if (bp) {
        hermes_log(LOG_DEBUG, "port",
                   "get_blueprint: found '%s'", key);
    } else {
        hermes_log(LOG_WARNING, "port",
                   "get_blueprint: key '%s' not found", key);
    }

    return (void *)bp;
}

/*
 * blueprint_form_schema: Emit the JSON-like form schema for a blueprint.
 *
 * Python: returns dict with key, title, description, category, tags, fields.
 * In C: p1 = pointer to automation_blueprint_t,
 *       p2 = pointer to output buffer (char *, pre-allocated),
 *       p3 = buffer size.
 * Returns: pointer to output buffer with formatted schema text.
 */
void* cli_cron_blueprint_catalog_blueprint_form_schema(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p4; (void)p5;

    const automation_blueprint_t *bp = (const automation_blueprint_t *)p1;
    char *out = (char *)p2;
    size_t out_sz = p3 ? *(size_t *)p3 : 0;

    if (!bp || !out || out_sz == 0) return NULL;

    int written = snprintf(out, out_sz,
        "{\n"
        "  \"key\": \"%s\",\n"
        "  \"title\": \"%s\",\n"
        "  \"description\": \"%s\",\n"
        "  \"category\": \"%s\",\n"
        "  \"fields\": [\n",
        bp->key, bp->title, bp->description, bp->category);

    if (written < 0 || (size_t)written >= out_sz) return out;

    for (int i = 0; i < bp->slots_count; i++) {
        const blueprint_slot_t *s = &bp->slots[i];
        int n = snprintf(out + written, out_sz - written,
            "    {\"name\":\"%s\",\"type\":\"%s\",\"label\":\"%s\","
            "\"default\":\"%s\",\"optional\":%s,\"strict\":%s,\"help\":\"%s\"",
            s->name, s->type, s->label, s->default_val,
            s->optional ? "true" : "false",
            s->strict ? "true" : "false",
            s->help);
        if (n < 0 || (size_t)(written + n) >= out_sz) break;
        written += n;

        if (s->options_count > 0) {
            n = snprintf(out + written, out_sz - written, ",\"options\":[");
            if (n < 0 || (size_t)(written + n) >= out_sz) break;
            written += n;
            for (int j = 0; j < s->options_count; j++) {
                n = snprintf(out + written, out_sz - written,
                             "\"%s\"%s", s->options[j],
                             j < s->options_count - 1 ? "," : "");
                if (n < 0 || (size_t)(written + n) >= out_sz) break;
                written += n;
            }
            n = snprintf(out + written, out_sz - written, "]");
            if (n < 0 || (size_t)(written + n) >= out_sz) break;
            written += n;
        }

        n = snprintf(out + written, out_sz - written, "}%s\n",
                     i < bp->slots_count - 1 ? "," : "");
        if (n < 0 || (size_t)(written + n) >= out_sz) break;
        written += n;
    }

    snprintf(out + written, out_sz - written, "  ]\n}\n");

    hermes_log(LOG_DEBUG, "port",
               "blueprint_form_schema: generated schema for '%s' (%d fields)",
               bp->key, bp->slots_count);

    return out;
}

/*
 * blueprint_slash_command: Build the /blueprint <key> slot=val command string.
 *
 * Python: builds "/blueprint key slot=val ..." with quoting for text slots.
 * In C: p1 = pointer to automation_blueprint_t,
 *       p2 = pointer to slot_values array (char **, key-value pairs, NULL terminated),
 *       p3 = output buffer,
 *       p4 = buffer size.
 * Returns: pointer to output buffer.
 */
void* cli_cron_blueprint_catalog_blueprint_slash_command(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p5;

    const automation_blueprint_t *bp = (const automation_blueprint_t *)p1;
    char **values = (char **)p2;  /* alternating key/value, NULL terminated */
    char *out = (char *)p3;
    size_t out_sz = p4 ? *(size_t *)p4 : 0;

    if (!bp || !out || out_sz == 0) return NULL;

    int written = snprintf(out, out_sz, "/blueprint %s", bp->key);
    if (written < 0 || (size_t)written >= out_sz) return out;

    for (int i = 0; i < bp->slots_count; i++) {
        const blueprint_slot_t *s = &bp->slots[i];

        /* Look up value in values array */
        const char *val = NULL;
        if (values) {
            for (int v = 0; values[v] && values[v + 1]; v += 2) {
                if (strcmp(values[v], s->name) == 0) {
                    val = values[v + 1];
                    break;
                }
            }
        }
        if (!val) val = s->default_val;
        if (!val || val[0] == '\0') {
            if (s->optional) continue;
            val = "";
        }

        /* Quote text slots or values with spaces */
        bool need_quote = (strcmp(s->type, "text") == 0) || (strchr(val, ' ') != NULL);

        int n;
        if (need_quote) {
            /* Escape quotes in value */
            char escaped[512];
            int ei = 0;
            for (const char *p = val; *p && ei < (int)sizeof(escaped) - 3; p++) {
                if (*p == '"') { escaped[ei++] = '\\'; escaped[ei++] = '"'; }
                else { escaped[ei++] = *p; }
            }
            escaped[ei] = '\0';
            n = snprintf(out + written, out_sz - written, " %s=\"%s\"", s->name, escaped);
        } else {
            n = snprintf(out + written, out_sz - written, " %s=%s", s->name, val);
        }
        if (n < 0 || (size_t)(written + n) >= out_sz) break;
        written += n;
    }

    hermes_log(LOG_DEBUG, "port",
               "blueprint_slash_command: built command for '%s'", bp->key);

    return out;
}

/*
 * blueprint_deeplink: Build the hermes://blueprint/<key>?slot=val URL.
 *
 * Python: uses urllib.parse.quote and urlencode.
 * In C: p1 = pointer to automation_blueprint_t,
 *       p2 = slot_values array (same format as slash_command),
 *       p3 = output buffer,
 *       p4 = buffer size.
 * Returns: pointer to output buffer.
 */
void* cli_cron_blueprint_catalog_blueprint_deeplink(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p5;

    const automation_blueprint_t *bp = (const automation_blueprint_t *)p1;
    char **values = (char **)p2;
    char *out = (char *)p3;
    size_t out_sz = p4 ? *(size_t *)p4 : 0;

    if (!bp || !out || out_sz == 0) return NULL;

    int written = snprintf(out, out_sz, "hermes://blueprint/%s", bp->key);
    if (written < 0 || (size_t)written >= out_sz) return out;

    /* Build query string from non-empty slot values */
    bool first = true;
    for (int i = 0; i < bp->slots_count; i++) {
        const blueprint_slot_t *s = &bp->slots[i];
        const char *val = NULL;

        if (values) {
            for (int v = 0; values[v] && values[v + 1]; v += 2) {
                if (strcmp(values[v], s->name) == 0) {
                    val = values[v + 1];
                    break;
                }
            }
        }
        if (!val) val = s->default_val;
        if (!val || val[0] == '\0') continue;

        /* Simple URL encoding: encode & and = and spaces */
        char encoded[512];
        int ei = 0;
        for (const char *p = val; *p && ei < (int)sizeof(encoded) - 4; p++) {
            if (*p == ' ') { encoded[ei++] = '%'; encoded[ei++] = '2'; encoded[ei++] = '0'; }
            else if (*p == '&') { encoded[ei++] = '%'; encoded[ei++] = '2'; encoded[ei++] = '6'; }
            else if (*p == '=') { encoded[ei++] = '%'; encoded[ei++] = '3'; encoded[ei++] = 'D'; }
            else { encoded[ei++] = *p; }
        }
        encoded[ei] = '\0';

        int n = snprintf(out + written, out_sz - written,
                         "%s%s=%s", first ? "?" : "&", s->name, encoded);
        if (n < 0 || (size_t)(written + n) >= out_sz) break;
        written += n;
        first = false;
    }

    hermes_log(LOG_DEBUG, "port",
               "blueprint_deeplink: built URL for '%s'", bp->key);

    return out;
}

/*
 * blueprint_catalog_entry: Unified serializable shape for a blueprint.
 *
 * Python: combines form_schema, schedule, scheduleHuman, command, appUrl.
 * In C: p1 = blueprint pointer, p2 = output buffer, p3 = buffer size.
 * Returns: pointer to output buffer with formatted entry.
 */
void* cli_cron_blueprint_catalog_blueprint_catalog_entry(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p4; (void)p5;

    const automation_blueprint_t *bp = (const automation_blueprint_t *)p1;
    char *out = (char *)p2;
    size_t out_sz = p3 ? *(size_t *)p3 : 0;

    if (!bp || !out || out_sz == 0) return NULL;

    /* Build form schema first into a temp buffer */
    size_t schema_sz = out_sz / 2;
    char *schema_buf = malloc(schema_sz);
    if (!schema_buf) return NULL;

    size_t sz = schema_sz;
    cli_cron_blueprint_catalog_blueprint_form_schema(
        (void *)bp, schema_buf, (void *)&sz, NULL, NULL);

    /* Build slash command into temp buffer */
    size_t cmd_sz = out_sz / 4;
    char *cmd_buf = malloc(cmd_sz);
    if (!cmd_buf) { free(schema_buf); return NULL; }
    size_t cmd_sz_val = cmd_sz;
    cli_cron_blueprint_catalog_blueprint_slash_command(
        (void *)bp, NULL, cmd_buf, (void *)&cmd_sz_val, NULL);

    /* Build deeplink into temp buffer */
    size_t link_sz = out_sz / 4;
    char *link_buf = malloc(link_sz);
    if (!link_buf) { free(schema_buf); free(cmd_buf); return NULL; }
    size_t link_sz_val = link_sz;
    cli_cron_blueprint_catalog_blueprint_deeplink(
        (void *)bp, NULL, link_buf, (void *)&link_sz_val, NULL);

    /* Compute human-readable schedule */
    char human_schedule[256] = "on a schedule";
    if (strstr(bp->schedule_template, "*/") == bp->schedule_template) {
        snprintf(human_schedule, sizeof(human_schedule),
                 "every %s minutes", bp->slots_count > 0 ? bp->slots[0].default_val : "30");
    } else if (strstr(bp->schedule_template, "* * 1-5") != NULL) {
        /* Find time slot */
        const char *time_def = "08:00";
        for (int i = 0; i < bp->slots_count; i++) {
            if (strcmp(bp->slots[i].type, "time") == 0) {
                time_def = bp->slots[i].default_val;
                break;
            }
        }
        snprintf(human_schedule, sizeof(human_schedule),
                 "weekdays at %s", time_def);
    } else if (strstr(bp->schedule_template, "{dow}") != NULL) {
        const char *time_def = "08:00";
        const char *day_def = "";
        for (int i = 0; i < bp->slots_count; i++) {
            if (strcmp(bp->slots[i].type, "time") == 0)
                time_def = bp->slots[i].default_val;
            if (strcmp(bp->slots[i].name, "day") == 0 ||
                strcmp(bp->slots[i].name, "recurrence") == 0)
                day_def = bp->slots[i].default_val;
        }
        if (day_def[0] && time_def[0])
            snprintf(human_schedule, sizeof(human_schedule),
                     "%s at %s", day_def, time_def);
        else if (time_def[0])
            snprintf(human_schedule, sizeof(human_schedule),
                     "at %s", time_def);
    } else {
        const char *time_def = "08:00";
        for (int i = 0; i < bp->slots_count; i++) {
            if (strcmp(bp->slots[i].type, "time") == 0) {
                time_def = bp->slots[i].default_val;
                break;
            }
        }
        snprintf(human_schedule, sizeof(human_schedule),
                 "daily at %s", time_def);
    }

    snprintf(out, out_sz,
             "%s,\n  \"schedule\": \"%s\",\n"
             "  \"scheduleHuman\": \"%s\",\n"
             "  \"command\": \"%s\",\n"
             "  \"appUrl\": \"%s\"\n}",
             schema_buf, bp->schedule_template, human_schedule,
             cmd_buf, link_buf);

    free(schema_buf);
    free(cmd_buf);
    free(link_buf);

    hermes_log(LOG_DEBUG, "port",
               "blueprint_catalog_entry: built entry for '%s'", bp->key);

    return out;
}

/*
 * _resolve_schedule: Fill schedule_template placeholders from slot values.
 *
 * Python: parses time, weekday, interval slots, fills template.
 * In C: p1 = blueprint pointer,
 *       p2 = values array (alternating key/value, NULL terminated),
 *       p3 = output buffer,
 *       p4 = buffer size.
 * Returns: pointer to output buffer with resolved cron expression.
 */
void* cli_cron_blueprint_catalog__resolve_schedule(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p5;

    const automation_blueprint_t *bp = (const automation_blueprint_t *)p1;
    char **values = (char **)p2;
    char *out = (char *)p3;
    size_t out_sz = p4 ? *(size_t *)p4 : 0;

    if (!bp || !out || out_sz == 0) return NULL;

    /* Start with the template */
    strncpy(out, bp->schedule_template, out_sz - 1);
    out[out_sz - 1] = '\0';

    /* Helper: find a slot value by name */
    const char *get_val(const char *name) {
        /* First check values array */
        if (values) {
            for (int v = 0; values[v] && values[v + 1]; v += 2) {
                if (strcmp(values[v], name) == 0) return values[v + 1];
            }
        }
        /* Fall back to slot default */
        for (int i = 0; i < bp->slots_count; i++) {
            if (strcmp(bp->slots[i].name, name) == 0)
                return bp->slots[i].default_val;
        }
        return NULL;
    }

    /* Check for free-text schedule slot */
    const char *schedule_val = get_val("schedule");
    if (schedule_val && schedule_val[0]) {
        strncpy(out, schedule_val, out_sz - 1);
        out[out_sz - 1] = '\0';
        hermes_log(LOG_DEBUG, "port",
                   "_resolve_schedule: free-text schedule '%s'", out);
        return out;
    }

    /* Replace {minute} and {hour} from time slot */
    const char *time_val = get_val("time");
    if (time_val && time_val[0]) {
        int hour = 0, minute = 0;
        if (sscanf(time_val, "%d:%d", &hour, &minute) == 2) {
            char hour_str[8], min_str[8];
            snprintf(hour_str, sizeof(hour_str), "%d", hour);
            snprintf(min_str, sizeof(min_str), "%d", minute);

            /* Simple string replacement: {hour} and {minute} */
            char tmp[512];
            char *src = out;
            char *dst = tmp;
            size_t dst_sz = sizeof(tmp);

            while (*src && (size_t)(dst - tmp) < dst_sz - 1) {
                if (strncmp(src, "{hour}", 6) == 0) {
                    int n = snprintf(dst, dst_sz - (dst - tmp), "%s", hour_str);
                    dst += n; src += 6;
                } else if (strncmp(src, "{minute}", 8) == 0) {
                    int n = snprintf(dst, dst_sz - (dst - tmp), "%s", min_str);
                    dst += n; src += 8;
                } else {
                    *dst++ = *src++;
                }
            }
            *dst = '\0';
            strncpy(out, tmp, out_sz - 1);
            out[out_sz - 1] = '\0';
        }
    }

    /* Replace {dow} from recurrence or day slot */
    if (strstr(out, "{dow}") != NULL) {
        const char *dow_val = NULL;
        const char *recurrence = get_val("recurrence");
        if (recurrence && recurrence[0]) {
            for (int i = 0; weekday_presets[i].name; i++) {
                if (strcmp(recurrence, weekday_presets[i].name) == 0) {
                    dow_val = weekday_presets[i].dow;
                    break;
                }
            }
        }
        if (!dow_val) {
            const char *day = get_val("day");
            if (day && day[0]) {
                for (int i = 0; day_to_dow[i].name; i++) {
                    if (strcmp(day, day_to_dow[i].name) == 0) {
                        dow_val = day_to_dow[i].dow;
                        break;
                    }
                }
            }
        }
        if (!dow_val) dow_val = "*";

        char tmp[512];
        char *src = out;
        char *dst = tmp;
        size_t dst_sz = sizeof(tmp);
        while (*src && (size_t)(dst - tmp) < dst_sz - 1) {
            if (strncmp(src, "{dow}", 5) == 0) {
                int n = snprintf(dst, dst_sz - (dst - tmp), "%s", dow_val);
                dst += n; src += 5;
            } else {
                *dst++ = *src++;
            }
        }
        *dst = '\0';
        strncpy(out, tmp, out_sz - 1);
        out[out_sz - 1] = '\0';
    }

    /* Replace {interval_min} */
    if (strstr(out, "{interval_min}") != NULL) {
        const char *iv = get_val("interval_min");
        if (iv && iv[0]) {
            char tmp[512];
            char *src = out;
            char *dst = tmp;
            size_t dst_sz = sizeof(tmp);
            while (*src && (size_t)(dst - tmp) < dst_sz - 1) {
                if (strncmp(src, "{interval_min}", 14) == 0) {
                    int n = snprintf(dst, dst_sz - (dst - tmp), "%s", iv);
                    dst += n; src += 14;
                } else {
                    *dst++ = *src++;
                }
            }
            *dst = '\0';
            strncpy(out, tmp, out_sz - 1);
            out[out_sz - 1] = '\0';
        }
    }

    /* Replace any remaining {slot} placeholders */
    for (int i = 0; i < bp->slots_count; i++) {
        const char *slot_name = bp->slots[i].name;
        char placeholder[64];
        snprintf(placeholder, sizeof(placeholder), "{%s}", slot_name);

        if (strstr(out, placeholder) != NULL) {
            const char *val = get_val(slot_name);
            if (val && val[0]) {
                char tmp[512];
                char *src = out;
                char *dst = tmp;
                size_t dst_sz = sizeof(tmp);
                size_t ph_len = strlen(placeholder);
                while (*src && (size_t)(dst - tmp) < dst_sz - 1) {
                    if (strncmp(src, placeholder, ph_len) == 0) {
                        int n = snprintf(dst, dst_sz - (dst - tmp), "%s", val);
                        dst += n; src += ph_len;
                    } else {
                        *dst++ = *src++;
                    }
                }
                *dst = '\0';
                strncpy(out, tmp, out_sz - 1);
                out[out_sz - 1] = '\0';
            }
        }
    }

    hermes_log(LOG_DEBUG, "port",
               "_resolve_schedule: '%s' -> '%s'", bp->schedule_template, out);

    return out;
}

/*
 * fill_blueprint: Validate values and return create_job kwargs.
 *
 * Python: validates slots, resolves schedule, renders prompt, returns spec dict.
 * In C: p1 = blueprint pointer,
 *       p2 = values array (alternating key/value),
 *       p3 = output buffer for job spec JSON,
 *       p4 = buffer size.
 * Returns: pointer to output buffer.
 */
void* cli_cron_blueprint_catalog_fill_blueprint(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p5;

    const automation_blueprint_t *bp = (const automation_blueprint_t *)p1;
    char **values = (char **)p2;
    char *out = (char *)p3;
    size_t out_sz = p4 ? *(size_t *)p4 : 0;

    if (!bp || !out || out_sz == 0) return NULL;

    /* Helper: find a value by name from values array or slot defaults */
    const char *get_val(const char *name) {
        if (values) {
            for (int v = 0; values[v] && values[v + 1]; v += 2) {
                if (strcmp(values[v], name) == 0) return values[v + 1];
            }
        }
        for (int i = 0; i < bp->slots_count; i++) {
            if (strcmp(bp->slots[i].name, name) == 0)
                return bp->slots[i].default_val;
        }
        return NULL;
    }

    /* Check for unknown slot names */
    if (values) {
        for (int v = 0; values[v]; v += 2) {
            bool known = false;
            for (int i = 0; i < bp->slots_count; i++) {
                if (strcmp(values[v], bp->slots[i].name) == 0) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                hermes_log(LOG_ERROR, "port",
                           "fill_blueprint: unknown slot '%s' for blueprint '%s'",
                           values[v], bp->key);
                snprintf(out, out_sz,
                         "{\"error\":\"unknown slot: %s\"}", values[v]);
                return out;
            }
        }
    }

    /* Validate each slot */
    for (int i = 0; i < bp->slots_count; i++) {
        const blueprint_slot_t *s = &bp->slots[i];
        const char *raw = get_val(s->name);

        if (!raw || raw[0] == '\0') {
            if (s->optional) continue;
            hermes_log(LOG_ERROR, "port",
                       "fill_blueprint: missing required slot '%s' (%s)",
                       s->name, s->label);
            snprintf(out, out_sz,
                     "{\"error\":\"missing required value: %s (%s)\"}",
                     s->name, s->label);
            return out;
        }

        /* Enum validation */
        if (strcmp(s->type, "enum") == 0 && s->strict && s->options_count > 0) {
            bool found = false;
            for (int j = 0; j < s->options_count; j++) {
                if (strcmp(raw, s->options[j]) == 0) { found = true; break; }
            }
            if (!found) {
                hermes_log(LOG_ERROR, "port",
                           "fill_blueprint: %s='%s' not in allowed options",
                           s->name, raw);
                snprintf(out, out_sz,
                         "{\"error\":\"%s='%s' not allowed\"}", s->name, raw);
                return out;
            }
        }
    }

    /* Resolve schedule */
    char schedule_buf[512];
    size_t sched_sz = sizeof(schedule_buf);
    cli_cron_blueprint_catalog__resolve_schedule(
        (void *)bp, values, schedule_buf, (void *)&sched_sz, NULL);

    /* Check if schedule resolution returned an error */
    if (strstr(schedule_buf, "{\"error\"") != NULL) {
        strncpy(out, schedule_buf, out_sz - 1);
        out[out_sz - 1] = '\0';
        return out;
    }

    /* Render prompt template with resolved values */
    char prompt[2048];
    strncpy(prompt, bp->prompt_template, sizeof(prompt) - 1);
    prompt[sizeof(prompt) - 1] = '\0';

    /* Simple template replacement: {slot_name} -> value */
    for (int i = 0; i < bp->slots_count; i++) {
        const char *slot_name = bp->slots[i].name;
        char placeholder[64];
        snprintf(placeholder, sizeof(placeholder), "{%s}", slot_name);

        const char *val = get_val(slot_name);
        if (val && val[0] && strstr(prompt, placeholder) != NULL) {
            char tmp[2048];
            char *src = prompt;
            char *dst = tmp;
            size_t dst_sz = sizeof(tmp);
            size_t ph_len = strlen(placeholder);
            while (*src && (size_t)(dst - tmp) < dst_sz - 1) {
                if (strncmp(src, placeholder, ph_len) == 0) {
                    int n = snprintf(dst, dst_sz - (dst - tmp), "%s", val);
                    dst += n; src += ph_len;
                } else {
                    *dst++ = *src++;
                }
            }
            *dst = '\0';
            strncpy(prompt, tmp, sizeof(prompt) - 1);
            prompt[sizeof(prompt) - 1] = '\0';
        }
    }

    /* Get deliver value */
    const char *deliver = get_val("deliver");
    if (!deliver || !deliver[0]) deliver = bp->deliver_default;

    /* Build job spec JSON */
    snprintf(out, out_sz,
             "{\"prompt\":\"%s\",\"schedule\":\"%s\",\"name\":\"%s\",\"deliver\":\"%s\"}",
             prompt, schedule_buf, bp->title, deliver);

    hermes_log(LOG_DEBUG, "port",
               "fill_blueprint: built job spec for '%s' (schedule: %s)",
               bp->key, schedule_buf);

    return out;
}

/*
 * Static catalog definition
 */

/* Morning briefing */
static const blueprint_slot_t morning_brief_slots[] = {
    SLOT_TIME("time", "What time?", "08:00"),
    SLOT_DELIVER(),
};
#define MORNING_BRIEF { .key = "morning-brief", .title = "Morning briefing", \
    .description = "A short daily briefing", .category = "daily", \
    .schedule_template = "{minute} {hour} * * *", \
    .prompt_template = "Produce a concise morning briefing for the user", \
    .slots_count = 2, .slots = {0}, .deliver_default = "origin" }

/* Important mail monitor */
static const blueprint_slot_t mail_slots[] = {
    SLOT_ENUM("interval_min", "How often?", "30", 2, "15", "30"),
    SLOT_TEXT("criteria", "Only notify me if the mail...",
              "needs a reply today, is from my manager or family"),
    SLOT_DELIVER(),
};

/* Weekly review */
static const blueprint_slot_t weekly_review_slots[] = {
    SLOT_TIME("time", "What time?", "18:00"),
    SLOT_ENUM("day", "Which day?", "sunday", 1, "sunday"),
    SLOT_DELIVER(),
};

/* Workday start */
static const blueprint_slot_t workday_slots[] = {
    SLOT_TIME("time", "What time?", "09:00"),
    SLOT_DELIVER(),
};

/* Custom reminder */
static const blueprint_slot_t custom_reminder_slots[] = {
    SLOT_TEXT("what", "Remind me to...", "take a break and stretch"),
    SLOT_TIME("time", "What time?", "14:00"),
    SLOT_WEEKDAYS("recurrence", "Repeat on", "everyday"),
    SLOT_DELIVER(),
};

/* Evening wind-down */
static const blueprint_slot_t evening_slots[] = {
    SLOT_TIME("time", "What time?", "21:00"),
    SLOT_DELIVER(),
};

/* News digest */
static const blueprint_slot_t news_slots[] = {
    SLOT_TEXT("topic", "What topic?", "AI and technology"),
    SLOT_TIME("time", "What time?", "18:00"),
    SLOT_WEEKDAYS("recurrence", "Repeat on", "weekdays"),
    SLOT_ENUM("count", "How many bullets?", "5", 2, "3", "5"),
    SLOT_DELIVER(),
};

/* Bill renewal */
static const blueprint_slot_t bill_slots[] = {
    SLOT_TEXT("what", "What's due?", "my streaming subscription renews soon"),
    SLOT_TIME("time", "What time?", "10:00"),
    SLOT_WEEKDAYS("recurrence", "Repeat on", "everyday"),
    SLOT_DELIVER(),
};

/* Habit check-in */
static const blueprint_slot_t habit_slots[] = {
    SLOT_TEXT("habit", "Which habit?", "20 minutes of reading"),
    SLOT_TIME("time", "What time?", "20:00"),
    SLOT_WEEKDAYS("recurrence", "Repeat on", "everyday"),
    SLOT_DELIVER(),
};

/* Hydration */
static const blueprint_slot_t hydration_slots[] = {
    SLOT_ENUM("interval_hours", "How often?", "1", 2, "1", "2"),
    SLOT_ENUM("start_hour", "Start hour", "9", 1, "9"),
    SLOT_ENUM("end_hour", "End hour", "17", 1, "17"),
    SLOT_DELIVER(),
};

/* Meal plan */
static const blueprint_slot_t meal_slots[] = {
    SLOT_ENUM("diet", "Diet?", "no restrictions", 2, "no restrictions", "vegetarian"),
    SLOT_ENUM("meals", "Meals per day?", "dinner only", 1, "dinner only"),
    SLOT_ENUM("effort", "Cooking effort?", "quick", 1, "quick"),
    SLOT_TIME("time", "What time?", "17:00"),
    SLOT_ENUM("day", "Which day?", "sunday", 1, "sunday"),
    SLOT_DELIVER(),
};

/* Daily learning */
static const blueprint_slot_t learn_slots[] = {
    SLOT_TEXT("topic", "Learn about...", "Spanish vocabulary"),
    SLOT_TIME("time", "What time?", "08:30"),
    SLOT_WEEKDAYS("recurrence", "Repeat on", "weekdays"),
    SLOT_DELIVER(),
};

/* Gratitude journal */
static const blueprint_slot_t gratitude_slots[] = {
    SLOT_TIME("time", "What time?", "21:30"),
    SLOT_WEEKDAYS("recurrence", "Repeat on", "everyday"),
    SLOT_DELIVER(),
};

/* On this day */
static const blueprint_slot_t onthisday_slots[] = {
    SLOT_ENUM("flavor", "What kind?", "on this day in history", 1, "on this day in history"),
    SLOT_TIME("time", "What time?", "07:30"),
    SLOT_DELIVER(),
};

/*
 * find_blueprint: Look up a blueprint by key in the static catalog.
 */
static const automation_blueprint_t *find_blueprint(const char *key) {
    /* We use a simple key-lookup table.
     * The static catalog is stored as a keyed array for O(n) lookup. */
    static const struct {
        const char *key;
        const blueprint_slot_t *slots;
        int slots_count;
        const char *title;
        const char *description;
        const char *category;
        const char *schedule_template;
        const char *prompt_template;
        const char *deliver_default;
    } catalog_entries[] = {
        {
            "morning-brief", morning_brief_slots, 2,
            "Morning briefing", "A short daily briefing", "daily",
            "{minute} {hour} * * *",
            "Produce a concise morning briefing for the user", "origin"
        },
        {
            "important-mail", mail_slots, 3,
            "Important-mail monitor", "Check your inbox periodically", "email",
            "*/{interval_min} * * * *",
            "Check the user's inbox for new messages since the last run", "origin"
        },
        {
            "weekly-review", weekly_review_slots, 3,
            "Weekly review", "A weekly recap", "weekly",
            "{minute} {hour} * * {dow}",
            "Produce a weekly review for the user", "origin"
        },
        {
            "workday-start", workday_slots, 2,
            "Workday start reminder", "A weekday nudge", "daily",
            "{minute} {hour} * * 1-5",
            "Give the user a brief weekday start-of-day nudge", "origin"
        },
        {
            "custom-reminder", custom_reminder_slots, 4,
            "Custom reminder", "A recurring reminder", "general",
            "{minute} {hour} * * {dow}",
            "Remind the user: {what}", "origin"
        },
        {
            "evening-winddown", evening_slots, 2,
            "Evening wind-down", "An end-of-day check-in", "daily",
            "{minute} {hour} * * *",
            "Give the user a short evening wind-down", "origin"
        },
        {
            "news-digest", news_slots, 5,
            "Topic news digest", "A recurring digest on a topic", "general",
            "{minute} {hour} * * {dow}",
            "Search the web for new and noteworthy items about: {topic}", "origin"
        },
        {
            "bill-renewal-watch", bill_slots, 4,
            "Bills & renewals reminder", "A heads-up before a recurring payment", "general",
            "{minute} {hour} * * {dow}",
            "Remind the user about an upcoming payment or renewal: {what}", "origin"
        },
        {
            "habit-checkin", habit_slots, 4,
            "Habit check-in", "A recurring nudge to keep a habit on track", "general",
            "{minute} {hour} * * {dow}",
            "Nudge the user about their habit: {habit}", "origin"
        },
        {
            "hydration-move", hydration_slots, 4,
            "Hydration & movement nudge", "A periodic nudge during the day", "general",
            "0 {start_hour}-{end_hour}/{interval_hours} * * 1-5",
            "Send the user a brief, friendly nudge to drink water", "origin"
        },
        {
            "meal-plan", meal_slots, 6,
            "Weekly meal plan", "A weekly meal plan", "weekly",
            "{minute} {hour} * * {dow}",
            "Build the user a meal plan for the coming week", "origin"
        },
        {
            "learn-daily", learn_slots, 4,
            "Daily learning drip", "One bite-sized lesson a day", "daily",
            "{minute} {hour} * * {dow}",
            "Teach the user one bite-sized lesson about: {topic}", "origin"
        },
        {
            "gratitude-journal", gratitude_slots, 3,
            "Gratitude & reflection prompt", "A gentle evening prompt", "general",
            "{minute} {hour} * * {dow}",
            "Send the user a short, warm reflection prompt", "origin"
        },
        {
            "on-this-day", onthisday_slots, 3,
            "On-this-day discovery", "A daily dose of curiosity", "daily",
            "{minute} {hour} * * *",
            "Give the user one interesting '{flavor}' item for today", "origin"
        },
        { NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL }
    };

    for (int i = 0; catalog_entries[i].key; i++) {
        if (strcmp(key, catalog_entries[i].key) == 0) {
            /* Build a temporary blueprint struct on the stack */
            static automation_blueprint_t bp;
            memset(&bp, 0, sizeof(bp));
            strncpy(bp.key, catalog_entries[i].key, sizeof(bp.key) - 1);
            strncpy(bp.title, catalog_entries[i].title, sizeof(bp.title) - 1);
            strncpy(bp.description, catalog_entries[i].description, sizeof(bp.description) - 1);
            strncpy(bp.category, catalog_entries[i].category, sizeof(bp.category) - 1);
            strncpy(bp.schedule_template, catalog_entries[i].schedule_template,
                    sizeof(bp.schedule_template) - 1);
            strncpy(bp.prompt_template, catalog_entries[i].prompt_template,
                    sizeof(bp.prompt_template) - 1);
            strncpy(bp.deliver_default, catalog_entries[i].deliver_default,
                    sizeof(bp.deliver_default) - 1);
            bp.slots_count = catalog_entries[i].slots_count;
            if (catalog_entries[i].slots && catalog_entries[i].slots_count > 0) {
                memcpy(bp.slots, catalog_entries[i].slots,
                       sizeof(blueprint_slot_t) * catalog_entries[i].slots_count);
            }
            return &bp;
        }
    }
    return NULL;
}
