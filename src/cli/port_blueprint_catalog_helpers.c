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
