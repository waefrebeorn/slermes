#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"

/* PoP: cron_incidents_u_incident_id @ cron/incidents.py:_incident_id */
/* f"{job_id[:6]}_{error_sig}" -- first 6 CODEPOINTS of job_id. */
char *cron_incidents_u_incident_id(const char *arg) {
    json_t *o = arg ? json_parse(arg, NULL) : NULL;
    if (!o || !json_is_object(o)) { if (o) json_free(o); return strdup("_"); }
    json_t *jv = json_obj_get(o, "job_id"), *ev = json_obj_get(o, "error_sig");
    const char *job = (jv && json_is_string(jv) && jv->str_val) ? jv->str_val : "";
    const char *sig = (ev && json_is_string(ev) && ev->str_val) ? ev->str_val : "";
    size_t cut = 0; int seen = 0;
    for (size_t i = 0; job[i] && seen < 6; ) {
        unsigned char b = (unsigned char)job[i]; int len = 1;
        if (b >= 0xF0) len = 4; else if (b >= 0xE0) len = 3; else if (b >= 0xC0) len = 2;
        i += (size_t)len; cut = i; seen++;
    }
    size_t need = cut + strlen(sig) + 2;
    char *r = malloc(need);
    snprintf(r, need, "%.*s_%s", (int)cut, job, sig);
    json_free(o);
    return r;
}
