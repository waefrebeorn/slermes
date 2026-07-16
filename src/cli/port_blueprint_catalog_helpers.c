/*
 * port_blueprint_catalog_helpers.c
 *
 * Pure, portable helper functions ported from cron/blueprint_catalog.py.
 * No catalog load, no BlueprintFillError raising (callers validate), no
 * dataclass. These take blueprint fields as JSON (schedule_template string,
 * slots array, key string) plus a values JSON object and perform pure string
 * templating. Coupled helpers (get_blueprint, blueprint_form_schema,
 * blueprint_catalog_entry, fill_blueprint, __post_init__) stay REAL_GAP.
 *
 * C name <- python name (module prefix 'blueprint_catalog_'):
 *   blueprint_catalog_slash_command   <- blueprint_slash_command
 *   blueprint_catalog_deeplink        <- blueprint_deeplink
 *   blueprint_catalog_humanize_schedule <- _humanize_schedule
 *   blueprint_catalog_resolve_schedule <- _resolve_schedule
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>

static char *json_escape_string(const char *s)
{
    if (!s) s = "";
    size_t need = 1;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') need += 2;
        else if (c == '\n') need += 2;
        else if (c == '\r') need += 2;
        else if (c == '\t') need += 2;
        else if (c < 0x20) need += 6;
        else need += 1;
    }
    char *out = malloc(need + 1);
    char *q = out;
    *q++ = '"';
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') { *q++='\\'; *q++='"'; }
        else if (c == '\\') { *q++='\\'; *q++='\\'; }
        else if (c == '\n') { *q++='\\'; *q++='n'; }
        else if (c == '\r') { *q++='\\'; *q++='r'; }
        else if (c == '\t') { *q++='\\'; *q++='t'; }
        else if (c < 0x20) { sprintf(q, "\\u%04x", c); q += 6; }
        else *q++ = (char)c;
    }
    *q++ = '"';
    *q = '\0';
    return out;
}

/* Array-index string accessor (json_get_str is key-based; this is index-based). */
static const char *json_arr_str(json_t *arr, size_t idx)
{
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    json_t *e = json_get(arr, idx);
    return (e && e->type == JSON_STRING) ? json_string_value(e) : NULL;
}

/* look up a slot's attribute from the slots JSON array */
static const char *slot_attr(json_t *slots, const char *name, const char *attr)
{
    if (!slots || slots->type != JSON_ARRAY) return NULL;
    for (size_t i = 0; i < json_array_size(slots); i++) {
        json_t *s = json_array_get(slots, i);
        if (!s || s->type != JSON_OBJECT) continue;
        json_t *n = json_object_get(s, "name");
        if (n && n->type == JSON_STRING && strcmp(json_string_value(n), name) == 0) {
            json_t *a = json_object_get(s, attr);
            if (a && a->type == JSON_STRING) return json_string_value(a);
            if (a && a->type == JSON_BOOL) return a->bool_val ? "true" : "false";
            if (a && a->type == JSON_NUMBER) return NULL; /* numeric default handled separately */
        }
    }
    return NULL;
}

static const char *slot_default_str(json_t *slots, const char *name)
{
    return slot_attr(slots, name, "default");
}
static const char *slot_type(json_t *slots, const char *name)
{
    return slot_attr(slots, name, "type");
}
static int slot_optional(json_t *slots, const char *name)
{
    if (!slots || slots->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_array_size(slots); i++) {
        json_t *s = json_array_get(slots, i);
        if (!s || s->type != JSON_OBJECT) continue;
        json_t *n = json_object_get(s, "name");
        if (n && n->type == JSON_STRING && strcmp(json_string_value(n), name) == 0) {
            json_t *o = json_object_get(s, "optional");
            if (o && o->type == JSON_BOOL) return o->bool_val ? 1 : 0;
        }
    }
    return 0;
}

/*
 * PoP: blueprint_slash_command @ cron/blueprint_catalog.py:blueprint_slash_command
 * key: blueprint key string; slots_json: JSON array of slot objects;
 * values_json: JSON object of supplied values (or "").
 * Returns malloc'd "/blueprint <key> slot=val ..." string. Caller frees.
 */
char *blueprint_catalog_slash_command(const char *key, const char *slots_json, const char *values_json)
{
    if (!key) key = "";
    if (!slots_json || !slots_json[0]) slots_json = "[]";
    if (!values_json || !values_json[0]) values_json = "{}";
    json_t *slots = json_parse(slots_json, NULL);
    json_t *vals = json_parse(values_json, NULL);
    char *out = malloc(strlen(key) + 64);
    sprintf(out, "/blueprint %s", key);
    if (slots && slots->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(slots); i++) {
            json_t *s = json_array_get(slots, i);
            if (!s || s->type != JSON_OBJECT) continue;
            json_t *nm = json_object_get(s, "name");
            if (!nm || nm->type != JSON_STRING) continue;
            const char *name = json_string_value(nm);
            const char *val = NULL;
            if (vals) {
                json_t *vv = json_object_get(vals, name);
                if (vv && vv->type == JSON_STRING) val = json_string_value(vv);
            }
            if (!val) val = slot_default_str(slots, name);
            const char *stype = slot_type(slots, name);
            int optional = slot_optional(slots, name);
            if ((!val || !val[0]) && optional) continue;
            if (!val) val = "";
            char sval[4096];
            strncpy(sval, val, sizeof(sval) - 1); sval[sizeof(sval)-1]='\0';
            /* quote if text type or contains space */
            int need_quote = (stype && strcmp(stype, "text") == 0) || strchr(sval, ' ') != NULL;
            char rendered[8192];
            if (need_quote) {
                /* escape double quotes */
                char esc[8192];
                char *e = esc;
                for (char *p = sval; *p; p++) {
                    if (*p == '"') { *e++='\\'; *e++='"'; } else *e++ = *p;
                }
                *e = '\0';
                snprintf(rendered, sizeof(rendered), "\"%s\"", esc);
            } else {
                strcpy(rendered, sval);
            }
            size_t need = strlen(out) + strlen(name) + strlen(rendered) + 8;
            char *n = realloc(out, need + 64);
            if (!n) break;
            out = n;
            strcat(out, " ");
            strcat(out, name);
            strcat(out, "=");
            strcat(out, rendered);
        }
    }
    if (slots) json_free(slots);
    if (vals) json_free(vals);
    return out;
}

/* urlencode a string (percent-encode space and non-alnum) */
static void urlencode(const char *in, char *out, size_t outsz)
{
    static const char *hexd = "0123456789ABCDEF";
    size_t j = 0;
    for (const char *p = in; *p && j + 4 < outsz; p++) {
        unsigned char c = (unsigned char)*p;
        if ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~') {
            out[j++] = (char)c;
        } else if (c == ' ') {
            out[j++] = '+';
        } else {
            out[j++] = '%'; out[j++] = hexd[(c>>4)&0xf]; out[j++] = hexd[c&0xf];
        }
    }
    out[j] = '\0';
}

/*
 * PoP: blueprint_deeplink @ cron/blueprint_catalog.py:blueprint_deeplink
 * Returns malloc'd "hermes://blueprint/<key>?slot=val..." URL. Caller frees.
 */
char *blueprint_catalog_deeplink(const char *key, const char *slots_json, const char *values_json)
{
    if (!key) key = "";
    if (!slots_json || !slots_json[0]) slots_json = "[]";
    if (!values_json || !values_json[0]) values_json = "{}";
    json_t *slots = json_parse(slots_json, NULL);
    json_t *vals = json_parse(values_json, NULL);
    char key_enc[2048];
    urlencode(key, key_enc, sizeof(key_enc));
    char *out = malloc(strlen(key) + 256);
    sprintf(out, "hermes://blueprint/%s", key_enc);
    int first = 1;
    if (slots && slots->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(slots); i++) {
            json_t *s = json_array_get(slots, i);
            if (!s || s->type != JSON_OBJECT) continue;
            json_t *nm = json_object_get(s, "name");
            if (!nm || nm->type != JSON_STRING) continue;
            const char *name = json_string_value(nm);
            const char *val = NULL;
            if (vals) {
                json_t *vv = json_object_get(vals, name);
                if (vv && vv->type == JSON_STRING) val = json_string_value(vv);
            }
            if (!val) val = slot_default_str(slots, name);
            if (!val || !val[0]) continue;
            char nm_enc[1024], val_enc[4096];
            urlencode(name, nm_enc, sizeof(nm_enc));
            urlencode(val, val_enc, sizeof(val_enc));
            size_t need = strlen(out) + strlen(nm_enc) + strlen(val_enc) + 8;
            char *n = realloc(out, need + 64);
            if (!n) break;
            out = n;
            strcat(out, first ? "?" : "&");
            strcat(out, nm_enc); strcat(out, "="); strcat(out, val_enc);
            first = 0;
        }
    }
    if (slots) json_free(slots);
    if (vals) json_free(vals);
    return out;
}

/*
 * PoP: _humanize_schedule @ cron/blueprint_catalog.py:_humanize_schedule
 * schedule_template: cron string; slots_json: slot array. Returns malloc'd
 * human schedule string. Caller frees.
 */
char *blueprint_catalog_humanize_schedule(const char *schedule_template, const char *slots_json)
{
    if (!schedule_template) schedule_template = "";
    if (!slots_json || !slots_json[0]) slots_json = "[]";
    json_t *slots = json_parse(slots_json, NULL);
    char *out = strdup("on a schedule");

    if (strncmp(schedule_template, "*/", 2) == 0) {
        const char *iv = slot_default_str(slots, "interval_min");
        const char *every = iv ? iv : (schedule_template + 2);
        char *sp = strchr(every, ' '); if (sp) *sp = '\0';
        char buf[256];
        snprintf(buf, sizeof(buf), "every %s minutes", every);
        free(out); out = strdup(buf);
    } else if (strstr(schedule_template, "{interval_hours}") != NULL) {
        const char *iv = slot_default_str(slots, "interval_hours");
        const char *every = iv ? iv : "1";
        const char *scope = strstr(schedule_template, "* * 1-5") ? "weekdays, " : "";
        char buf[256];
        if (strcmp(every, "1") == 0) snprintf(buf, sizeof(buf), "%severy hour", scope);
        else snprintf(buf, sizeof(buf), "%severy %s hours", scope, every);
        free(out); out = strdup(buf);
    } else {
        const char *when = NULL;
        if (slots) {
            for (size_t i = 0; i < json_array_size(slots); i++) {
                json_t *s = json_array_get(slots, i);
                if (!s || s->type != JSON_OBJECT) continue;
                json_t *t = json_object_get(s, "type");
                if (t && t->type == JSON_STRING && strcmp(json_string_value(t), "time")==0) {
                    json_t *d = json_object_get(s, "default");
                    if (d && d->type == JSON_STRING) when = json_string_value(d);
                    break;
                }
            }
        }
        if (strstr(schedule_template, "* * 1-5") != NULL) {
            char buf[256];
            snprintf(buf, sizeof(buf), when ? "weekdays at %s" : "every weekday", when ? when : "");
            free(out); out = strdup(buf);
        } else if (strstr(schedule_template, "{dow}") != NULL) {
            const char *scope = slot_default_str(slots, "day");
            if (!scope) scope = slot_default_str(slots, "recurrence");
            if (!scope) scope = "";
            char buf[256];
            if (scope[0] && when) snprintf(buf, sizeof(buf), "%s at %s", scope, when);
            else if (when) snprintf(buf, sizeof(buf), "at %s", when);
            else snprintf(buf, sizeof(buf), "on a schedule");
            free(out); out = strdup(buf);
        } else if (when) {
            char buf[256];
            snprintf(buf, sizeof(buf), "daily at %s", when);
            free(out); out = strdup(buf);
        }
    }
    if (slots) json_free(slots);
    return out;
}

/*
 * PoP: _resolve_schedule @ cron/blueprint_catalog.py:_resolve_schedule
 * Fills {placeholder}s in schedule_template from slot values. Returns malloc'd
 * cron string, or NULL on invalid input (caller treats NULL as error).
 *   schedule_template: cron string with {minute}/{hour}/{dow}/{interval_min}/...
 *   slots_json: slot array
 *   values_json: supplied values object
 */
char *blueprint_catalog_resolve_schedule(const char *schedule_template, const char *slots_json, const char *values_json)
{
    if (!schedule_template) return NULL;
    if (!slots_json || !slots_json[0]) slots_json = "[]";
    if (!values_json || !values_json[0]) values_json = "{}";
    json_t *slots = json_parse(slots_json, NULL);
    json_t *vals = json_parse(values_json, NULL);
    if (!vals || vals->type != JSON_OBJECT) { if (slots) json_free(slots); if (vals) json_free(vals); return NULL; }

    /* free-text schedule slot passes through */
    json_t *sched_val = json_object_get(vals, "schedule");
    if (sched_val && sched_val->type == JSON_STRING && json_string_value(sched_val)[0]) {
        char *r = strdup(json_string_value(sched_val));
        if (slots) json_free(slots); if (vals) json_free(vals);
        return r;
    }

    char repl[16][64];
    int nrepl = 0;
    /* helper to add a replacement */
#define ADD_REPL(k, v) do { if (nrepl<16){ strncpy(repl[nrepl], (k), 31); repl[nrepl][31]='\0'; \
        strncpy(repl[nrepl+1], (v), 63); repl[nrepl+1][63]='\0'; nrepl+=2; } } while(0)

    const char *sched = schedule_template;
    /* time -> minute/hour */
    if (strstr(sched, "{minute}") || strstr(sched, "{hour}")) {
        json_t *tv = json_object_get(vals, "time");
        const char *tstr = tv && tv->type == JSON_STRING ? json_string_value(tv) : NULL;
        if (!tstr || !tstr[0]) { if (slots) json_free(slots); if (vals) json_free(vals); return NULL; }
        /* validate HH:MM */
        int hh = 0, mm = 0;
        if (sscanf(tstr, "%d:%d", &hh, &mm) != 2 || hh < 0 || hh > 23 || mm < 0 || mm > 59) {
            if (slots) json_free(slots); if (vals) json_free(vals); return NULL;
        }
        char bh[8], bm[8];
        snprintf(bh, sizeof(bh), "%d", hh);
        snprintf(bm, sizeof(bm), "%d", mm);
        ADD_REPL("hour", bh); ADD_REPL("minute", bm);
    }
    /* weekday -> dow */
    if (strstr(sched, "{dow}")) {
        json_t *rec = json_object_get(vals, "recurrence");
        json_t *day = json_object_get(vals, "day");
        const char *preset = rec && rec->type == JSON_STRING ? json_string_value(rec) : NULL;
        const char *dayname = day && day->type == JSON_STRING ? json_string_value(day) : NULL;
        const char *dow = "*";
        if (preset) {
            char low[32]; size_t n=0;
            for (const char *p=preset; *p && n+1<sizeof(low); p++){char c=*p;if(c>='A'&&c<='Z')c+=32;low[n++]=c;} low[n]='\0';
            if (strcmp(low,"everyday")==0) dow="*";
            else if (strcmp(low,"weekdays")==0) dow="1-5";
            else if (strcmp(low,"weekends")==0) dow="0,6";
            else { if (slots) json_free(slots); if (vals) json_free(vals); return NULL; }
        } else if (dayname) {
            char low[32]; size_t n=0;
            for (const char *p=dayname; *p && n+1<sizeof(low); p++){char c=*p;if(c>='A'&&c<='Z')c+=32;low[n++]=c;} low[n]='\0';
            static const char *days[] = {"sunday","monday","tuesday","wednesday","thursday","friday","saturday",NULL};
            static const char *dows[] = {"0","1","2","3","4","5","6"};
            dow = "1"; /* default */
            for (int i=0; days[i]; i++) if (strcmp(low, days[i])==0) { dow = dows[i]; break; }
            if (strcmp(dow,"1")==0 && strcmp(low,"monday")!=0) { if (slots) json_free(slots); if (vals) json_free(vals); return NULL; }
        }
        ADD_REPL("dow", dow);
    }
    /* interval_min */
    if (strstr(sched, "{interval_min}")) {
        json_t *iv = json_object_get(vals, "interval_min");
        const char *ivs = iv && iv->type == JSON_STRING ? json_string_value(iv) : NULL;
        if (!ivs) ivs = "";
        /* must be positive integer */
        char *end; long v = strtol(ivs, &end, 10);
        if (*end != '\0' || v <= 0) { if (slots) json_free(slots); if (vals) json_free(vals); return NULL; }
        char bv[16]; snprintf(bv, sizeof(bv), "%ld", v);
        ADD_REPL("interval_min", bv);
    }
    /* any remaining {slot} from values */
    /* scan template for {word} */
    const char *p = sched;
    while ((p = strchr(p, '{')) != NULL) {
        const char *q = strchr(p, '}');
        if (!q) break;
        char name[64];
        size_t nl = (size_t)(q - p - 1);
        if (nl >= sizeof(name)) { p = q; continue; }
        memcpy(name, p+1, nl); name[nl]='\0';
        int found = 0;
        for (int i = 0; i < nrepl; i += 2) if (strcmp(repl[i], name)==0) { found=1; break; }
        if (!found) {
            json_t *vv = json_object_get(vals, name);
            if (vv && vv->type == JSON_STRING) {
                ADD_REPL(name, json_string_value(vv));
            }
        }
        p = q + 1;
    }

    /* perform substitution */
    char *result = strdup(sched);
    for (int i = 0; i < nrepl; i += 2) {
        char tok[80];
        snprintf(tok, sizeof(tok), "{%s}", repl[i]);
        /* replace all occurrences */
        char *pos = result;
        while ((pos = strstr(pos, tok)) != NULL) {
            size_t tlen = strlen(tok);
            size_t vlen = strlen(repl[i+1]);
            memmove(pos + vlen, pos + tlen, strlen(pos + tlen) + 1);
            memcpy(pos, repl[i+1], vlen);
            pos += vlen;
        }
    }
    if (slots) json_free(slots);
    if (vals) json_free(vals);
    return result;
}

/* ===========================================================================
 *  Catalog + entry renderers + fill/validate (Port of cron/blueprint_catalog.py)
 *  Reuses blueprint_catalog_resolve_schedule / _slash_command / _deeplink /
 *  _humanize_schedule above. The curated CATALOG is baked as JSON (no file IO).
 * =========================================================================== */

/* Slot types the renderers understand (Python _SLOT_TYPES). */
static const char *BLUEPRINT_SLOT_TYPES[] = {
    "time", "enum", "text", "weekdays", NULL
};

/*
 * PoP: __post_init__ @ cron/blueprint_catalog.py:__post_init__
 * Validates a slot type — Python BlueprintSlot.__post_init__ equivalent. */
int blueprint_catalog_validate_slot_type(const char *type)
{
    if (!type) return 0;
    for (int i = 0; BLUEPRINT_SLOT_TYPES[i]; i++)
        if (strcmp(type, BLUEPRINT_SLOT_TYPES[i]) == 0) return 1;
    return 0;
}

/* Curated in-repo catalog (Python CATALOG). One JSON object per blueprint,
 * with slots as JSON objects carrying name/type/label/default/options/
 * optional/strict/help. Keep in lock-step with cron/blueprint_catalog.py. */
static const char *BLUEPRINT_CATALOG_JSON =
"[{"
  "\"key\":\"morning-brief\",\"title\":\"Morning briefing\","
  "\"description\":\"A short daily briefing: today's calendar, weather, and anything urgent waiting on you.\","
  "\"category\":\"daily\","
  "\"schedule_template\":\"{minute} {hour} * * *\","
  "\"prompt_template\":\"Produce a concise morning briefing for the user: today's calendar events, the local weather, and any urgent items. Keep it short and scannable. If no data sources are connected, give a brief good-morning with the date and offer to connect calendar/email.\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"time\",\"type\":\"time\",\"label\":\"What time?\",\"default\":\"08:00\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"24h local time, e.g. 08:00\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"origin = the chat you set this up from\"}],"
  "\"skills\":[],\"tags\":[\"daily\",\"briefing\"]"
"},{"
  "\"key\":\"important-mail\",\"title\":\"Important-mail monitor\","
  "\"description\":\"Check your inbox periodically and ping you ONLY about mail that actually needs attention.\","
  "\"category\":\"email\","
  "\"schedule_template\":\"*/{interval_min} * * * *\","
  "\"prompt_template\":\"Check the user's inbox for new messages since the last run. Surface ONLY mail matching: {criteria}. Score candidates with the urgency classifier and deliver only what clears the bar; if nothing does, respond with [SILENT]. Requires a connected mail source; if none is configured, explain how to connect one and stop.\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"interval_min\",\"type\":\"enum\",\"label\":\"How often?\",\"default\":\"30\",\"options\":[\"15\",\"30\",\"60\"],\"optional\":false,\"strict\":true,\"help\":\"minutes between checks\"},"
  "{\"name\":\"criteria\",\"type\":\"text\",\"label\":\"Only notify me if the mail…\",\"default\":\"needs a reply today, is from my manager or family, or mentions a deadline\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"email\",\"monitor\"]"
"},{"
  "\"key\":\"weekly-review\",\"title\":\"Weekly review\","
  "\"description\":\"A weekly recap: what got done, what's still open, and what's coming up.\","
  "\"category\":\"weekly\","
  "\"schedule_template\":\"{minute} {hour} * * {dow}\","
  "\"prompt_template\":\"Produce a weekly review for the user: what was accomplished this week, still-open items, and next week's calendar. Pull from connected sources. Keep it tight.\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"time\",\"type\":\"time\",\"label\":\"What time?\",\"default\":\"18:00\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"24h local time\"},"
  "{\"name\":\"day\",\"type\":\"enum\",\"label\":\"Which day?\",\"default\":\"sunday\",\"options\":[\"sunday\",\"monday\",\"friday\",\"saturday\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"weekly\",\"review\"]"
"},{"
  "\"key\":\"workday-start\",\"title\":\"Workday start reminder\","
  "\"description\":\"A weekday nudge with your agenda and top priorities.\","
  "\"category\":\"daily\","
  "\"schedule_template\":\"{minute} {hour} * * 1-5\","
  "\"prompt_template\":\"Give the user a brief weekday start-of-day nudge: today's calendar and the 1-3 highest-priority things to focus on, inferred from recent context and any task tools. Encouraging, short, one message.\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"time\",\"type\":\"time\",\"label\":\"What time?\",\"default\":\"09:00\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"24h local time\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"daily\",\"focus\"]"
"},{"
  "\"key\":\"custom-reminder\",\"title\":\"Custom reminder\","
  "\"description\":\"A recurring reminder in your own words, on your schedule.\","
  "\"category\":\"general\","
  "\"schedule_template\":\"{minute} {hour} * * {dow}\","
  "\"prompt_template\":\"Remind the user: {what}\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"what\",\"type\":\"text\",\"label\":\"Remind me to…\",\"default\":\"take a break and stretch\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"time\",\"type\":\"time\",\"label\":\"What time?\",\"default\":\"14:00\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"24h local time\"},"
  "{\"name\":\"recurrence\",\"type\":\"weekdays\",\"label\":\"Repeat on\",\"default\":\"everyday\",\"options\":[\"everyday\",\"weekdays\",\"weekends\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"reminder\"]"
"},{"
  "\"key\":\"evening-winddown\",\"title\":\"Evening wind-down\","
  "\"description\":\"An end-of-day check-in: tomorrow's calendar at a glance and anything you should prep tonight.\","
  "\"category\":\"daily\","
  "\"schedule_template\":\"{minute} {hour} * * *\","
  "\"prompt_template\":\"Give the user a short evening wind-down: tomorrow's calendar, any early commitments to prep for, and one gentle nudge to wrap up loose ends from today. Keep it calm and brief — one message. If no calendar is connected, just offer a friendly sign-off and the weather for tomorrow.\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"time\",\"type\":\"time\",\"label\":\"What time?\",\"default\":\"21:00\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"24h local time\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"daily\",\"evening\"]"
"},{"
  "\"key\":\"news-digest\",\"title\":\"Topic news digest\","
  "\"description\":\"A recurring digest on a topic you care about — deduped against what was already sent, so only genuinely new items land.\","
  "\"category\":\"general\","
  "\"schedule_template\":\"{minute} {hour} * * {dow}\","
  "\"prompt_template\":\"Search the web for new and noteworthy items about: {topic}. Dedupe against what you sent in previous runs — only include genuinely new developments. Deliver a tight digest of at most {count} bullets, each one line with a link. If nothing new since last run, respond with [SILENT].\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"topic\",\"type\":\"text\",\"label\":\"What topic?\",\"default\":\"AI and technology\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"a subject, product, person, or search phrase\"},"
  "{\"name\":\"time\",\"type\":\"time\",\"label\":\"What time?\",\"default\":\"18:00\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"24h local time\"},"
  "{\"name\":\"recurrence\",\"type\":\"weekdays\",\"label\":\"Repeat on\",\"default\":\"weekdays\",\"options\":[\"everyday\",\"weekdays\",\"weekends\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"count\",\"type\":\"enum\",\"label\":\"How many bullets?\",\"default\":\"5\",\"options\":[\"3\",\"5\",\"8\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"digest\",\"research\"]"
"},{"
  "\"key\":\"bill-renewal-watch\",\"title\":\"Bills & renewals reminder\","
  "\"description\":\"A heads-up before a recurring payment, subscription renewal, or due date — so nothing auto-charges by surprise.\","
  "\"category\":\"general\","
  "\"schedule_template\":\"{minute} {hour} * * {dow}\","
  "\"prompt_template\":\"Remind the user about an upcoming payment or renewal: {what}. Phrase it as an actionable heads-up (e.g. 'review or cancel before it renews'), not just a notification. One short message.\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"what\",\"type\":\"text\",\"label\":\"What's due?\",\"default\":\"my streaming subscription renews soon\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"time\",\"type\":\"time\",\"label\":\"What time?\",\"default\":\"10:00\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"24h local time\"},"
  "{\"name\":\"recurrence\",\"type\":\"weekdays\",\"label\":\"Repeat on\",\"default\":\"everyday\",\"options\":[\"everyday\",\"weekdays\",\"weekends\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"reminder\",\"finance\"]"
"},{"
  "\"key\":\"habit-checkin\",\"title\":\"Habit check-in\","
  "\"description\":\"A recurring nudge to keep a habit on track and reflect on whether you did it.\","
  "\"category\":\"general\","
  "\"schedule_template\":\"{minute} {hour} * * {dow}\","
  "\"prompt_template\":\"Nudge the user about their habit: {habit}. Ask whether they did it today, keep it warm and non-judgmental, and offer a one-line word of encouragement. One short message.\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"habit\",\"type\":\"text\",\"label\":\"Which habit?\",\"default\":\"20 minutes of reading\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"time\",\"type\":\"time\",\"label\":\"What time?\",\"default\":\"20:00\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"24h local time\"},"
  "{\"name\":\"recurrence\",\"type\":\"weekdays\",\"label\":\"Repeat on\",\"default\":\"everyday\",\"options\":[\"everyday\",\"weekdays\",\"weekends\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"habit\",\"wellbeing\"]"
"},{"
  "\"key\":\"hydration-move\",\"title\":\"Hydration & movement nudge\","
  "\"description\":\"A periodic nudge during the day to drink water, stand up, and stretch.\","
  "\"category\":\"general\","
  "\"schedule_template\":\"0 {start_hour}-{end_hour}/{interval_hours} * * 1-5\","
  "\"prompt_template\":\"Send the user a brief, friendly nudge to drink some water, stand up, and stretch for a moment. Vary the wording each time so it doesn't feel robotic. One short line.\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"interval_hours\",\"type\":\"enum\",\"label\":\"How often?\",\"default\":\"1\",\"options\":[\"1\",\"2\",\"3\"],\"optional\":false,\"strict\":true,\"help\":\"hours between nudges\"},"
  "{\"name\":\"start_hour\",\"type\":\"enum\",\"label\":\"Start hour\",\"default\":\"9\",\"options\":[\"7\",\"8\",\"9\",\"10\"],\"optional\":false,\"strict\":true,\"help\":\"first hour of the active window (24h)\"},"
  "{\"name\":\"end_hour\",\"type\":\"enum\",\"label\":\"End hour\",\"default\":\"17\",\"options\":[\"16\",\"17\",\"18\",\"19\"],\"optional\":false,\"strict\":true,\"help\":\"last hour of the active window (24h)\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"wellbeing\",\"focus\"]"
"},{"
  "\"key\":\"meal-plan\",\"title\":\"Weekly meal plan\","
  "\"description\":\"A weekly meal plan plus a consolidated grocery list, tuned to your diet and how much time you have to cook.\","
  "\"category\":\"weekly\","
  "\"schedule_template\":\"{minute} {hour} * * {dow}\","
  "\"prompt_template\":\"Build the user a meal plan for the coming week: {meals} per day, suited to a {diet} diet and roughly {effort} cooking effort. Include a consolidated grocery list grouped by aisle. Keep blueprints simple and skimmable.\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"diet\",\"type\":\"enum\",\"label\":\"Diet?\",\"default\":\"no restrictions\",\"options\":[\"no restrictions\",\"vegetarian\",\"vegan\",\"high-protein\",\"low-carb\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"meals\",\"type\":\"enum\",\"label\":\"Meals per day?\",\"default\":\"dinner only\",\"options\":[\"dinner only\",\"lunch and dinner\",\"all three\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"effort\",\"type\":\"enum\",\"label\":\"Cooking effort?\",\"default\":\"quick\",\"options\":[\"quick\",\"medium\",\"ambitious\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"time\",\"type\":\"time\",\"label\":\"What time?\",\"default\":\"17:00\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"24h local time\"},"
  "{\"name\":\"day\",\"type\":\"enum\",\"label\":\"Which day?\",\"default\":\"sunday\",\"options\":[\"sunday\",\"monday\",\"friday\",\"saturday\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"weekly\",\"food\"]"
"},{"
  "\"key\":\"learn-daily\",\"title\":\"Daily learning drip\","
  "\"description\":\"One bite-sized lesson a day on a topic you want to learn, building progressively over time.\","
  "\"category\":\"daily\","
  "\"schedule_template\":\"{minute} {hour} * * {dow}\","
  "\"prompt_template\":\"Teach the user one bite-sized lesson about: {topic}. Build on earlier lessons so it progresses rather than repeating. Keep it to a couple of short paragraphs with one concrete example, and end with a single question to check understanding.\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"topic\",\"type\":\"text\",\"label\":\"Learn about…\",\"default\":\"Spanish vocabulary\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"time\",\"type\":\"time\",\"label\":\"What time?\",\"default\":\"08:30\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"24h local time\"},"
  "{\"name\":\"recurrence\",\"type\":\"weekdays\",\"label\":\"Repeat on\",\"default\":\"weekdays\",\"options\":[\"everyday\",\"weekdays\",\"weekends\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"learning\",\"daily\"]"
"},{"
  "\"key\":\"gratitude-journal\",\"title\":\"Gratitude & reflection prompt\","
  "\"description\":\"A gentle evening prompt to reflect on the day and note what went well.\","
  "\"category\":\"general\","
  "\"schedule_template\":\"{minute} {hour} * * {dow}\","
  "\"prompt_template\":\"Send the user a short, warm reflection prompt for the end of the day — invite them to note one thing that went well, one thing they are grateful for, and one small win. If they reply, acknowledge it kindly. One message.\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"time\",\"type\":\"time\",\"label\":\"What time?\",\"default\":\"21:30\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"24h local time\"},"
  "{\"name\":\"recurrence\",\"type\":\"weekdays\",\"label\":\"Repeat on\",\"default\":\"everyday\",\"options\":[\"everyday\",\"weekdays\",\"weekends\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"wellbeing\",\"reflection\"]"
"},{"
  "\"key\":\"on-this-day\",\"title\":\"On-this-day discovery\","
  "\"description\":\"A daily dose of curiosity: a notable historical event, fact, or word for the day.\","
  "\"category\":\"daily\","
  "\"schedule_template\":\"{minute} {hour} * * *\","
  "\"prompt_template\":\"Give the user one interesting '{flavor}' item for today — keep it short, surprising, and genuinely interesting. One or two sentences, no filler.\","
  "\"deliver_default\":\"origin\","
  "\"slots\":[{\"name\":\"flavor\",\"type\":\"enum\",\"label\":\"What kind?\",\"default\":\"on this day in history\",\"options\":[\"on this day in history\",\"word of the day\",\"science fact\",\"quote of the day\"],\"optional\":false,\"strict\":true,\"help\":\"\"},"
  "{\"name\":\"time\",\"type\":\"time\",\"label\":\"What time?\",\"default\":\"07:30\",\"options\":[],\"optional\":false,\"strict\":true,\"help\":\"24h local time\"},"
  "{\"name\":\"deliver\",\"type\":\"enum\",\"label\":\"Where to deliver?\",\"default\":\"origin\",\"options\":[\"origin\",\"local\",\"telegram\",\"discord\",\"email\"],\"optional\":false,\"strict\":false,\"help\":\"\"}],"
  "\"skills\":[],\"tags\":[\"daily\",\"curiosity\"]"
"}]";

/*
 * PoP: get_blueprint @ cron/blueprint_catalog.py:get_blueprint
 * Returns a malloc'd copy of the blueprint JSON object (caller json_free's),
 * or NULL if key not found. */
json_t *blueprint_catalog_get(const char *key)
{
    if (!key || !*key) return NULL;
    json_t *cat = json_parse(BLUEPRINT_CATALOG_JSON, NULL);
    if (!cat || cat->type != JSON_ARRAY) { if (cat) json_free(cat); return NULL; }
    json_t *found = NULL;
    for (size_t i = 0; i < json_array_size(cat) && !found; i++) {
        json_t *bp = json_array_get(cat, i);
        if (!bp || bp->type != JSON_OBJECT) continue;
        const char *k = json_get_str(bp, "key", NULL);
        if (k && strcmp(k, key) == 0) found = bp;
    }
    if (found) {
        char *ser = json_serialize(found);
        json_free(cat);
        if (!ser) return NULL;
        json_t *copy = json_parse(ser, NULL);
        free(ser);
        return copy;
    }
    json_free(cat);
    return NULL;
}

/* Expose the single baked catalog source-of-truth so other modules (e.g.
 * blueprint_cmd.c) can load it without a second copy. Returns the static
 * JSON array string; do NOT free. */
const char *blueprint_catalog_raw_json(void)
{
    return BLUEPRINT_CATALOG_JSON;
}


/*
 * PoP: blueprint_form_schema @ cron/blueprint_catalog.py:blueprint_form_schema
 * Emits the JSON a form renderer needs for this blueprint. Returns malloc'd
 * string (caller frees) or NULL on error. */
char *blueprint_catalog_form_schema(json_t *bp)
{
    if (!bp || bp->type != JSON_OBJECT) return NULL;
    json_t *schema = json_object();
    json_set(schema, "key", json_string(json_get_str(bp, "key", "")));
    json_set(schema, "title", json_string(json_get_str(bp, "title", "")));
    json_set(schema, "description", json_string(json_get_str(bp, "description", "")));
    json_set(schema, "category", json_string(json_get_str(bp, "category", "")));
    json_t *tags = json_object_get(bp, "tags");
    json_t *ftags = json_array();
    if (tags && tags->type == JSON_ARRAY)
        for (size_t i = 0; i < json_array_size(tags); i++) {
            const char *t = json_arr_str(tags, i);
            if (t) json_append(ftags, json_string(t));
        }
    json_set(schema, "tags", ftags);
    json_t *flds = json_array();
    json_t *slots = json_object_get(bp, "slots");
    if (slots && slots->type == JSON_ARRAY)
        for (size_t i = 0; i < json_array_size(slots); i++) {
            json_t *s = json_array_get(slots, i);
            if (!s || s->type != JSON_OBJECT) continue;
            json_t *f = json_object();
            json_set(f, "name", json_string(json_get_str(s, "name", "")));
            json_set(f, "type", json_string(json_get_str(s, "type", "")));
            json_set(f, "label", json_string(json_get_str(s, "label", "")));
            json_set(f, "default", json_string(json_get_str(s, "default", "")));
            json_t *fopts = json_array();
            json_t *opts = json_object_get(s, "options");
            if (opts && opts->type == JSON_ARRAY)
                for (size_t j = 0; j < json_array_size(opts); j++) {
                    const char *o = json_arr_str(opts, j);
                    if (o) json_append(fopts, json_string(o));
                }
            json_set(f, "options", fopts);
            json_set(f, "optional", json_bool(json_get_bool(s, "optional", 0)));
            json_set(f, "strict", json_bool(json_get_bool(s, "strict", 1)));
            json_set(f, "help", json_string(json_get_str(s, "help", "")));
            json_append(flds, f);
        }
    json_set(schema, "fields", flds);
    char *ser = json_serialize(schema);
    json_free(schema);
    return ser;
}

/*
 * PoP: blueprint_catalog_entry @ cron/blueprint_catalog.py:blueprint_catalog_entry
 * Combines form schema + schedule + scheduleHuman + command + appUrl. */
char *blueprint_catalog_entry(json_t *bp)
{
    if (!bp || bp->type != JSON_OBJECT) return NULL;
    const char *key = json_get_str(bp, "key", "");
    const char *sched = json_get_str(bp, "schedule_template", "");
    json_t *schema = json_parse(blueprint_catalog_form_schema(bp), NULL);
    char *human = blueprint_catalog_humanize_schedule(sched,
        bp ? json_serialize(json_object_get(bp, "slots")) : "[]");
    char *cmd = blueprint_catalog_slash_command(key,
        bp ? json_serialize(json_object_get(bp, "slots")) : "[]", "");
    char *dl = blueprint_catalog_deeplink(key,
        bp ? json_serialize(json_object_get(bp, "slots")) : "[]", "");

    json_t *entry = json_object();
    if (schema) {
        /* merge form schema fields into entry */
        size_t nkeys = json_object_size(schema);
        for (size_t i = 0; i < nkeys; i++) {
            const char *k = json_object_get_key_at(schema, i);
            if (!k) continue;
            json_set(entry, k, json_copy(json_object_get(schema, k)));
        }
        json_free(schema);
    }
    json_set(entry, "schedule", json_string(sched));
    json_set(entry, "scheduleHuman", json_string(human ? human : ""));
    json_set(entry, "command", json_string(cmd ? cmd : ""));
    json_set(entry, "appUrl", json_string(dl ? dl : ""));
    char *ser = json_serialize(entry);
    json_free(entry);
    free(human); free(cmd); free(dl);
    return ser;
}

/*
 * PoP: fill_blueprint @ cron/blueprint_catalog.py:fill_blueprint
 * Validates values and returns a cron.jobs.create_job kwargs JSON string.
 * On error returns NULL and writes a BlueprintFillError-style message to
 * err_out (err_sz bytes). */
char *blueprint_catalog_fill(json_t *bp, const char *values_json,
                              const char *origin_json, char *err_out, size_t err_sz)
{
    if (err_out && err_sz) err_out[0] = '\0';
    if (!bp || bp->type != JSON_OBJECT) {
        if (err_out) snprintf(err_out, err_sz, "blueprint is null");
        return NULL;
    }
    json_t *vals = json_parse(values_json && *values_json ? values_json : "{}", NULL);
    if (!vals || vals->type != JSON_OBJECT) {
        if (err_out) snprintf(err_out, err_sz, "values must be a JSON object");
        if (vals) json_free(vals);
        return NULL;
    }
    json_t *slots = json_object_get(bp, "slots");
    if (!slots || slots->type != JSON_ARRAY) {
        if (err_out) snprintf(err_out, err_sz, "blueprint has no slots");
        json_free(vals);
        return NULL;
    }

    /* Validate slot types (Python BlueprintSlot.__post_init__). */
    for (size_t i = 0; i < json_array_size(slots); i++) {
        json_t *s = json_array_get(slots, i);
        if (!s || s->type != JSON_OBJECT) continue;
        const char *tp = json_get_str(s, "type", NULL);
        if (tp && !blueprint_catalog_validate_slot_type(tp)) {
            const char *nm = json_get_str(s, "name", "?");
            if (err_out) snprintf(err_out, err_sz, "unknown slot type %s (slot %s)", tp, nm);
            json_free(vals);
            return NULL;
        }
    }

    /* Reject unknown slot names (typo guard). */
    size_t nkeys = json_object_size(vals);
    for (size_t i = 0; i < nkeys; i++) {
        const char *k = json_object_get_key_at(vals, i);
        if (!k) continue;
        int known = 0;
        for (size_t j = 0; j < json_array_size(slots); j++) {
            json_t *s = json_array_get(slots, j);
            if (s && json_get_str(s, "name", NULL) &&
                strcmp(json_get_str(s, "name", ""), k) == 0) { known = 1; break; }
        }
        if (!known && strcmp(k, "schedule") != 0) {
            if (err_out) snprintf(err_out, err_sz,
                "unknown slot: %s — valid: <see blueprint slots>", k);
            json_free(vals);
            return NULL;
        }
    }

    /* Resolve each slot: default or supplied; check required + enum strict. */
    json_t *resolved = json_object();
    for (size_t i = 0; i < json_array_size(slots); i++) {
        json_t *s = json_array_get(slots, i);
        if (!s || s->type != JSON_OBJECT) continue;
        const char *nm = json_get_str(s, "name", "");
        int optional = json_get_bool(s, "optional", 0);
        const char *raw = json_get_str(vals, nm, NULL);
        if (!raw || !*raw) {
            raw = json_get_str(s, "default", NULL);
            if ((!raw || !*raw) && !optional) {
                if (err_out) snprintf(err_out, err_sz,
                    "missing required value: %s", nm);
                json_free(resolved); json_free(vals);
                return NULL;
            }
            if (!raw) raw = "";
        }
        const char *tp = json_get_str(s, "type", "");
        int strict = json_get_bool(s, "strict", 1);
        json_t *opts = json_object_get(s, "options");
        if (strict && opts && opts->type == JSON_ARRAY && json_array_size(opts) > 0) {
            int ok = (strcmp(tp, "weekdays") == 0); /* weekdays allows preset set, checked in resolve */
            if (!ok) {
                for (size_t j = 0; j < json_array_size(opts); j++) {
                    const char *o = json_arr_str(opts, j);
                    if (o && strcmp(o, raw) == 0) { ok = 1; break; }
                }
            }
            if (!ok) {
                if (err_out) snprintf(err_out, err_sz,
                    "%s=%s not allowed — one of the slot options", nm, raw);
                json_free(resolved); json_free(vals);
                return NULL;
            }
        }
        json_set(resolved, nm, json_string(raw));
    }

    const char *sched_tmpl = json_get_str(bp, "schedule_template", "");
    char *schedule = blueprint_catalog_resolve_schedule(sched_tmpl,
        json_serialize(slots), json_serialize(resolved));
    if (!schedule) {
        if (err_out) snprintf(err_out, err_sz, "failed to resolve schedule (invalid slot values)");
        json_free(resolved); json_free(vals);
        return NULL;
    }

    /* Render prompt. */
    const char *prompt_tmpl = json_get_str(bp, "prompt_template", "");
    char *prompt = NULL;
    if (prompt_tmpl && *prompt_tmpl) {
        /* simple {name} substitution from resolved */
        size_t plen = strlen(prompt_tmpl) + 1024;
        prompt = malloc(plen);
        size_t po = 0;
        const char *p = prompt_tmpl;
        while (*p) {
            if (*p == '{') {
                const char *q = strchr(p, '}');
                if (q) {
                    char nm[64]; size_t nl = (size_t)(q - p - 1);
                    if (nl < sizeof(nm)) {
                        memcpy(nm, p + 1, nl); nm[nl] = '\0';
                        const char *rv = json_get_str(resolved, nm, "");
                        size_t rvl = strlen(rv);
                        if (po + rvl < plen) { memcpy(prompt + po, rv, rvl); po += rvl; }
                        p = q + 1;
                        continue;
                    }
                }
            }
            if (po + 1 < plen) prompt[po++] = *p;
            p++;
        }
        prompt[po] = '\0';
    } else {
        prompt = strdup("");
    }

    json_t *spec = json_object();
    json_set(spec, "prompt", json_string(prompt));
    json_set(spec, "schedule", json_string(schedule));
    json_set(spec, "name", json_string(json_get_str(bp, "title", "")));
    const char *deliver = json_get_str(resolved, "deliver", NULL);
    if (!deliver || !*deliver) deliver = json_get_str(bp, "deliver_default", "origin");
    json_set(spec, "deliver", json_string(deliver ? deliver : "origin"));
    json_t *skills = json_object_get(bp, "skills");
    if (skills && skills->type == JSON_ARRAY && json_array_size(skills) > 0) {
        json_t *out_skills = json_array();
        for (size_t i = 0; i < json_array_size(skills); i++) {
            const char *sk = json_arr_str(skills, i);
            if (sk) json_append(out_skills, json_string(sk));
        }
        json_set(spec, "skills", out_skills);
    }
    if (origin_json && *origin_json) {
        json_t *origin = json_parse(origin_json, NULL);
        if (origin) { json_set(spec, "origin", origin); json_free(origin); }
    }
    char *ser = json_serialize(spec);
    json_free(spec);
    free(prompt); free(schedule); json_free(resolved); json_free(vals);
    return ser;
}
