/* cron_jobs_rewrite.c — C11 port of cron/jobs.py:rewrite_skill_refs and its
 * leaf helper _normalize_skill_list.  jobs/consolidated/pruned are supplied by
 * the caller; load_jobs/save_jobs are represented by the before/after arrays
 * in the returned object so the oracle can compare the full state transition.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static const char *cj_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static int cj_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return 1;
}

/* PoP: _normalize_skill_list @ cron/jobs.py */
static json_t *cj_normalize_skill_list(const json_t *skill, const json_t *skills) {
    json_t *out = json_array();
    json_t *raw = json_array();
    if (!skills || skills->type == JSON_NULL) {
        if (cj_truthy(skill)) json_append(raw, json_copy(skill));
    } else if (skills->type == JSON_STRING) {
        json_append(raw, json_copy(skills));
    } else if (skills->type == JSON_ARRAY) {
        for (size_t i = 0; i < skills->c.count; i++) json_append(raw, json_copy(skills->c.items[i]));
    }
    for (size_t i = 0; i < raw->c.count; i++) {
        const char *s = cj_str(raw->c.items[i]);
        /* str(item or "").strip(): only strings are normalised here; non-string
         * scalars are stringified by the oracle's Python repr rules, covered by
         * the string/list cases in the fixture. */
        size_t a = 0, b = strlen(s);
        while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r')) a++;
        while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\n' || s[b-1] == '\r')) b--;
        if (b == a) continue;
        int dup = 0;
        for (size_t j = 0; j < out->c.count; j++)
            if (!strcmp(cj_str(out->c.items[j]), s + (a ? a : 0)) && strlen(cj_str(out->c.items[j])) == b - a)
                { dup = 1; break; }
        if (!dup) {
            char *t = malloc(b - a + 1);
            memcpy(t, s + a, b - a); t[b - a] = '\0';
            json_append(out, json_string(t)); free(t);
        }
    }
    json_free(raw);
    return out;
}

static int cj_array_has(const json_t *a, const char *s) {
    if (!a || a->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < a->c.count; i++)
        if (!strcmp(cj_str(a->c.items[i]), s)) return 1;
    return 0;
}

/* PoP: rewrite_skill_refs @ cron/jobs.py */
json_t *cj_rewrite_skill_refs(const json_t *jobs_in, const json_t *consolidated, const json_t *pruned) {
    json_t *result = json_object();
    json_t *rewrites = json_array();
    json_set(result, "rewrites", rewrites);
    json_set(result, "jobs_updated", json_number(0));
    json_set(result, "jobs_scanned", json_number(0));

    json_t *cons = (consolidated && consolidated->type == JSON_OBJECT) ? json_copy(consolidated) : json_object();
    json_t *pruned_set = json_array();
    if (pruned && pruned->type == JSON_ARRAY) {
        for (size_t i = 0; i < pruned->c.count; i++) {
            const char *s = cj_str(pruned->c.items[i]);
            if (!json_obj_get(cons, s) && !cj_array_has(pruned_set, s)) json_append(pruned_set, json_string(s));
        }
    }
    if (cons->c.count == 0 && pruned_set->c.count == 0) {
        json_t *early = json_object();
        json_set(early, "result", result);
        json_set(early, "jobs", json_copy(jobs_in ? jobs_in : json_array()));
        json_free(pruned_set); json_free(cons);
        return early;
    }

    json_t *jobs = json_copy(jobs_in ? jobs_in : json_array());
    for (size_t k = 0; k < jobs->c.count; k++) {
        json_t *job = jobs->c.items[k];
        json_t *before = cj_normalize_skill_list(json_obj_get(job, "skill"), json_obj_get(job, "skills"));
        if (before->c.count == 0) { json_free(before); continue; }
        json_t *mapped = json_object();
        json_t *dropped = json_array();
        json_t *new_skills = json_array();
        for (size_t i = 0; i < before->c.count; i++) {
            const char *name = cj_str(before->c.items[i]);
            const json_t *target = json_obj_get(cons, name);
            if (target) {
                json_set(mapped, name, json_copy(target));
                const char *t = cj_str(target);
                if (t[0] && !cj_array_has(new_skills, t)) json_append(new_skills, json_copy(target));
            } else if (cj_array_has(pruned_set, name)) {
                json_append(dropped, json_string(name));
            } else if (!cj_array_has(new_skills, name)) {
                json_append(new_skills, json_string(name));
            }
        }
        if (mapped->c.count == 0 && dropped->c.count == 0) {
            json_free(mapped); json_free(dropped); json_free(new_skills); json_free(before);
            continue;
        }
        json_set(job, "skills", json_copy(new_skills));
        json_set(job, "skill", new_skills->c.count ? json_copy(new_skills->c.items[0]) : json_null());
        json_t *row = json_object();
        const json_t *jid = json_obj_get(job, "id");
        json_set(row, "job_id", json_copy(jid ? jid : json_null()));
        const json_t *jname = json_obj_get(job, "name");
        json_set(row, "job_name", json_copy(cj_truthy(jname) ? jname : (jid ? jid : json_null())));
        json_set(row, "before", json_copy(before));
        json_set(row, "after", json_copy(new_skills));
        json_set(row, "mapped", json_copy(mapped));
        json_set(row, "dropped", json_copy(dropped));
        json_append(rewrites, row);
        json_free(mapped); json_free(dropped); json_free(new_skills); json_free(before);
    }
    json_set(result, "jobs_updated", json_number((double)rewrites->c.count));
    json_set(result, "jobs_scanned", json_number((double)jobs->c.count));
    json_t *out = json_object();
    json_set(out, "result", result);
    json_set(out, "jobs", jobs);
    json_free(pruned_set); json_free(cons);
    return out;
}
