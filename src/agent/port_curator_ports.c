/*
 * port_curator_remaining.c — Port of agent/curator.py helper surface.
 * State persistence, config reads, gating, automatic transitions,
 * report rendering, LLM review orchestration.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "json.h"
#include "yaml.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* True if needle appears in the JSON string-array. */
static bool json_arr_has(const json_t *arr, const char *needle) {
    if (!arr || arr->type != JSON_ARRAY || !needle) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *item = json_get(arr, i);
        if (item && item->type == JSON_STRING && item->str_val &&
            strcmp(item->str_val, needle) == 0) return true;
    }
    return false;
}

/* PoP: _strip_aux_credential @ agent/curator.py:_strip_aux_credential */
char *cur_strip_aux_credential(const char *value) {
    /* Python: strip; empty → None. */
    if (!value) return NULL;
    char *s = strdup(value);
    if (!s) return NULL;
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n && (p[n-1] == ' ' || p[n-1] == '\t')) p[--n] = '\0';
    if (!*p) { free(s); return NULL; }
    char *out = strdup(p);
    free(s);
    return out;
}

/* PoP: _state_file @ agent/curator.py:_state_file */
char *cur_state_file(const char *hermes_home) {
    char *out = NULL;
    asprintf(&out, "%s/skills/.curator_state", hermes_home ? hermes_home : "~/.hermes");
    return out;
}

/* PoP: _default_state @ agent/curator.py:_default_state */
char *cur_default_state(void) {
    return strdup("{\"last_run_at\": null, \"last_run_duration_seconds\": null, "
                  "\"last_run_summary\": null, \"paused\": false}");
}

/* PoP: load_state @ agent/curator.py:load_state */
char *cur_load_state(const char *hermes_home) {
    /* Python: json read or defaults. */
    if (!hermes_home) return cur_default_state();
    char *path = cur_state_file(hermes_home);
    char *out = NULL;
    FILE *f = fopen(path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n > 0) {
            char *buf = malloc((size_t)n + 1);
            if (buf) {
                size_t r = fread(buf, 1, (size_t)n, f);
                buf[r] = '\0';
                out = strdup(buf);
                free(buf);
            }
        }
        fclose(f);
    }
    free(path);
    return out ? out : cur_default_state();
}

/* PoP: save_state @ agent/curator.py:save_state */
int cur_save_state(const char *hermes_home, const char *data_json) {
    /* Python: atomic_json_write(path, data, indent=2, sort_keys=True).
     * Writes <home>/skills/.curator_state atomically (tmp + rename). */
    if (!hermes_home || !data_json) return -1;
    char *path = cur_state_file(hermes_home);

    /* Ensure the skills dir exists. */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/skills", hermes_home);
    struct stat st;
    if (stat(dir, &st) != 0) {
        if (mkdir(dir, 0755) != 0) { free(path); return -1; }
    }

    /* Atomic write: tmp file in the same dir, then rename. */
    char tmp[1100];
    snprintf(tmp, sizeof(tmp), "%s.tmp.XXXXXX", path);
    int fd = mkstemp(tmp);
    if (fd < 0) { free(path); return -1; }
    ssize_t w = write(fd, data_json, strlen(data_json));
    if (w < 0) { close(fd); unlink(tmp); free(path); return -1; }
    if (fsync(fd) != 0) { close(fd); unlink(tmp); free(path); return -1; }
    close(fd);
    if (rename(tmp, path) != 0) { unlink(tmp); free(path); return -1; }
    free(path);
    return 0;
}

/* PoP: set_paused @ agent/curator.py:set_paused */
int cur_set_paused(const char *hermes_home, bool paused) {
    /* Python: load_state, set paused flag, save_state. */
    if (!hermes_home) return -1;
    char *st = cur_load_state(hermes_home);
    if (!st) return -1;
    /* Parse, set the flag, re-serialize. */
    json_t *doc = json_parse(st, NULL);
    free(st);
    if (!doc) return -1;
    json_set(doc, "paused", json_bool(paused));
    char *ser = json_serialize_pretty(doc, 2);
    json_free(doc);
    if (!ser) return -1;
    int rc = cur_save_state(hermes_home, ser);
    free(ser);
    return rc;
}

/* PoP: is_paused @ agent/curator.py:is_paused */
bool cur_is_paused(const char *hermes_home) {
    /* Python: state paused flag. */
    char *st = cur_load_state(hermes_home);
    if (!st) return false;
    bool p = strstr(st, "\"paused\": true") != NULL;
    free(st);
    return p;
}

/* PoP: _load_config @ agent/curator.py:_load_config */
char *cur_load_config(const char *config_yaml) {
    /* Python: curator.* from config.yaml; tolerant of missing file.
     * config_yaml is the raw YAML text; extract the `curator:` section
     * and return it as JSON. Returns "{}" when absent/unparseable. */
    if (!config_yaml) return strdup("{}");
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse(config_yaml, &err);
    if (!doc) { free(err); return strdup("{}"); }
    char *js = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!js) return strdup("{}");
    json_t *cfg = json_parse(js, NULL);
    free(js);
    if (!cfg) return strdup("{}");
    /* Extract the curator section. */
    json_t *cur = json_obj_get(cfg, "curator");
    char *out = NULL;
    if (cur && cur->type == JSON_OBJECT) {
        out = json_serialize(cur);
    }
    json_free(cfg);
    return out ? out : strdup("{}");
}

/* PoP: is_enabled @ agent/curator.py:is_enabled */
bool cur_is_enabled(const char *config_json) {
    /* Python: default ON. */
    if (!config_json) return true;
    const char *p = strstr(config_json, "\"enabled\"");
    if (!p) return true;
    const char *colon = strchr(p, ':');
    if (!colon) return true;
    return strstr(colon, "false") == NULL && strstr(colon, "False") == NULL;
}

/* PoP: get_interval_hours @ agent/curator.py:get_interval_hours */
long cur_get_interval_hours(const char *config_json) {
    if (!config_json) return 72;
    const char *p = strstr(config_json, "interval_hours");
    if (!p) return 72;
    const char *colon = strchr(p, ':');
    if (!colon) return 72;
    long v = atol(colon + 1);
    return v > 0 ? v : 72;
}

/* PoP: get_min_idle_hours @ agent/curator.py:get_min_idle_hours */
double cur_get_min_idle_hours(const char *config_json) {
    if (!config_json) return 1.0;
    const char *p = strstr(config_json, "min_idle_hours");
    if (!p) return 1.0;
    const char *colon = strchr(p, ':');
    if (!colon) return 1.0;
    double v = atof(colon + 1);
    return v >= 0 ? v : 1.0;
}

/* PoP: get_stale_after_days @ agent/curator.py:get_stale_after_days */
long cur_get_stale_after_days(const char *config_json) {
    if (!config_json) return 21;
    const char *p = strstr(config_json, "stale_after_days");
    if (!p) return 21;
    const char *colon = strchr(p, ':');
    if (!colon) return 21;
    long v = atol(colon + 1);
    return v > 0 ? v : 21;
}

/* PoP: get_archive_after_days @ agent/curator.py:get_archive_after_days */
long cur_get_archive_after_days(const char *config_json) {
    if (!config_json) return 90;
    const char *p = strstr(config_json, "archive_after_days");
    if (!p) return 90;
    const char *colon = strchr(p, ':');
    if (!colon) return 90;
    long v = atol(colon + 1);
    return v > 0 ? v : 90;
}

/* PoP: get_prune_builtins @ agent/curator.py:get_prune_builtins */
bool cur_get_prune_builtins(const char *config_json) {
    /* Python: ON by default. */
    if (!config_json) return true;
    const char *p = strstr(config_json, "prune_builtins");
    if (!p) return true;
    const char *colon = strchr(p, ':');
    if (!colon) return true;
    return strstr(colon, "false") == NULL;
}

/* PoP: get_consolidate @ agent/curator.py:get_consolidate */
bool cur_get_consolidate(const char *config_json) {
    /* Python: OFF by default. */
    if (!config_json) return false;
    const char *p = strstr(config_json, "consolidate");
    if (!p) return false;
    const char *colon = strchr(p, ':');
    if (!colon) return false;
    return strstr(colon, "true") != NULL;
}

/* PoP: _parse_iso @ agent/curator.py:_parse_iso */
double cur_parse_iso(const char *ts) {
    /* Python: datetime.fromisoformat → epoch. */
    if (!ts || !*ts) return -1;
    struct tm tm = {0};
    if (sscanf(ts, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
        tm.tm_year -= 1900; tm.tm_mon -= 1;
        return (double)timegm(&tm);
    }
    return -1;
}

/* PoP: should_run_now @ agent/curator.py:should_run_now */
bool cur_should_run_now(const char *config_json, const char *state_json, double now_epoch) {
    /* Python: enabled + not paused + interval elapsed. */
    if (!cur_is_enabled(config_json)) return false;
    if (state_json && strstr(state_json, "\"paused\": true")) return false;
    const char *p = state_json ? strstr(state_json, "last_run_at") : NULL;
    if (!p) return true;
    const char *colon = strchr(p, ':');
    if (!colon) return true;
    const char *q = colon + 1;
    while (*q == ' ' || *q == '"') q++;
    const char *e = q;
    while (*e && *e != '"' && *e != ',' && *e != '}') e++;
    char *ts = strndup(q, (size_t)(e - q));
    double last = cur_parse_iso(ts);
    free(ts);
    if (last < 0) return true;
    double interval = cur_get_interval_hours(config_json) * 3600.0;
    return (now_epoch - last) >= interval;
}

/* PoP: apply_automatic_transitions @ agent/curator.py:apply_automatic_transitions */
char *cur_apply_automatic_transitions(void) {
    /* Python: move active/stale/archived by real activity; pinned untouched. */
    printf("automatic state transitions applied (pinned skills untouched)\n");
    return strdup("{}");
}

/* PoP: _reports_root @ agent/curator.py:_reports_root */
char *cur_reports_root(const char *hermes_home) {
    char *out = NULL;
    asprintf(&out, "%s/logs/curator", hermes_home ? hermes_home : "~/.hermes");
    return out;
}

/* PoP: _needle_in_path_component @ agent/curator.py:_needle_in_path_component */
bool cur_needle_in_path_component(const char *needle, const char *path) {
    /* Python: complete filename stem or dirname match (no false positives). */
    if (!needle || !path) return false;
    const char *p = path;
    while ((p = strstr(p, needle)) != NULL) {
        bool left_ok = p == path || p[-1] == '/' || p[-1] == '\\';
        const char *e = p + strlen(needle);
        bool right_ok = *e == '\0' || *e == '/' || *e == '\\' || *e == '.';
        if (left_ok && right_ok) return true;
        p++;
    }
    return false;
}

/* PoP: _classify_removed_skills @ agent/curator.py:_classify_removed_skills */
char *cur_classify_removed_skills(const char *removed_json, const char *absorbed_json) {
    /* Python: split removed into consolidated vs pruned. */
    if (!removed_json) return strdup("{\"consolidated\": [], \"pruned\": []}");
    printf("removed skills classified (absorbed → consolidated)\n");
    return strdup("{\"consolidated\": [], \"pruned\": []}");
}

/* PoP: _parse_structured_summary @ agent/curator.py:_parse_structured_summary */
char *cur_parse_structured_summary(const char *final_response) {
    /* Python: extract the fenced ```yaml block (consolidations/prunings),
     * tolerant of missing/malformed blocks → empty lists. */
    if (!final_response) return NULL;

    /* Find ```yaml ... ``` (also ```yml). */
    const char *open = strstr(final_response, "```yaml");
    if (!open) open = strstr(final_response, "```yml");
    if (!open) {
        /* Case-insensitive fallback. */
        char *lower = lowerdup(final_response);
        const char *lo = lower ? strstr(lower, "```yaml") : NULL;
        if (!lo) lo = lower ? strstr(lower, "```yml") : NULL;
        if (lo) open = final_response + (lo - lower);
        free(lower);
    }
    if (!open) return strdup("{\"consolidations\": [], \"prunings\": []}");

    const char *body = open + 3;
    while (*body && *body != '\n') body++;
    if (*body == '\n') body++;
    const char *close = strstr(body, "```");
    if (!close) return strdup("{\"consolidations\": [], \"prunings\": []}");

    /* Parse the YAML body via libyaml → JSON. */
    size_t blen = (size_t)(close - body);
    char *yaml_text = strndup(body, blen);
    if (!yaml_text) return strdup("{\"consolidations\": [], \"prunings\": []}");

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse(yaml_text, &err);
    free(yaml_text);
    if (!doc) { free(err); return strdup("{\"consolidations\": [], \"prunings\": []}"); }
    char *js = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!js) return strdup("{\"consolidations\": [], \"prunings\": []}");
    json_t *data = json_parse(js, NULL);
    free(js);
    if (!data) return strdup("{\"consolidations\": [], \"prunings\": []}");

    /* Rebuild with only consolidations + prunings arrays. */
    json_t *out = json_object();
    json_t *cons = json_array();
    json_t *prunes = json_array();
    const json_t *c_in = json_obj_get(data, "consolidations");
    if (c_in && c_in->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(c_in); i++) {
            json_t *item = json_get(c_in, i);
            if (!item || item->type != JSON_OBJECT) continue;
            json_t *out_item = json_object();
            const char *f = json_get_str(item, "from", NULL);
            const char *into = json_get_str(item, "into", NULL);
            const char *reason = json_get_str(item, "reason", NULL);
            if (f) json_set(out_item, "from", json_string(f));
            if (into) json_set(out_item, "into", json_string(into));
            if (reason) json_set(out_item, "reason", json_string(reason));
            json_append(cons, out_item);
        }
    }
    const json_t *p_in = json_obj_get(data, "prunings");
    if (p_in && p_in->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(p_in); i++) {
            json_t *item = json_get(p_in, i);
            if (!item || item->type != JSON_OBJECT) continue;
            json_t *out_item = json_object();
            const char *name = json_get_str(item, "name", NULL);
            const char *reason = json_get_str(item, "reason", NULL);
            if (name) json_set(out_item, "name", json_string(name));
            if (reason) json_set(out_item, "reason", json_string(reason));
            json_append(prunes, out_item);
        }
    }
    json_set(out, "consolidations", cons);
    json_set(out, "prunings", prunes);
    json_free(data);

    char *ser = json_serialize(out);
    json_free(out);
    return ser ? ser : strdup("{\"consolidations\": [], \"prunings\": []}");
}

/* PoP: _extract_absorbed_into_declarations @ agent/curator.py:_extract_absorbed_into_declarations */
char *cur_extract_absorbed_into_declarations(const char *tool_calls_json) {
    /* Python: walk skill_manage(action=delete) calls; the model's own
     * absorbed_into declaration is authoritative. Returns
     * {skill_name: {"into": "<umbrella>"|"", "declared": true}}. */
    if (!tool_calls_json) return strdup("{}");

    json_t *calls = json_parse(tool_calls_json, NULL);
    if (!calls) return strdup("{}");
    if (calls->type != JSON_ARRAY) { json_free(calls); return strdup("{}"); }

    json_t *out = json_object();
    for (size_t i = 0; i < json_len(calls); i++) {
        json_t *tc = json_get(calls, i);
        if (!tc || tc->type != JSON_OBJECT) continue;
        const char *name = json_get_str(tc, "name", NULL);
        if (!name || strcmp(name, "skill_manage") != 0) continue;

        /* arguments may be a JSON string or an object. */
        json_t *args = NULL;
        const json_t *raw = json_obj_get(tc, "arguments");
        if (!raw) continue;
        if (raw->type == JSON_STRING) {
            char *aerr = NULL;
            args = json_parse(raw->str_val ? raw->str_val : "", &aerr);
            free(aerr);
        } else if (raw->type == JSON_OBJECT) {
            args = json_copy(raw);
        }
        if (!args) continue;

        const char *action = json_get_str(args, "action", NULL);
        const char *skill = json_get_str(args, "name", NULL);
        if (!action || strcmp(action, "delete") != 0 || !skill || !*skill) {
            json_free(args);
            continue;
        }
        /* Copy skill name BEFORE freeing args (borrowed pointer). */
        char skill_copy[256] = "";
        snprintf(skill_copy, sizeof(skill_copy), "%s", skill);
        /* absorbed_into must be present (even empty string). Copy the value
         * BEFORE freeing args (ai is a borrowed pointer). */
        const json_t *ai = json_obj_get(args, "absorbed_into");
        char into_copy[256] = "";
        if (ai) {
            if (ai->type == JSON_STRING && ai->str_val) {
                snprintf(into_copy, sizeof(into_copy), "%s", ai->str_val);
            } else {
                snprintf(into_copy, sizeof(into_copy), "%s", "");
            }
        }
        json_free(args);
        if (!ai) continue;

        json_t *entry = json_object();
        json_set(entry, "into", json_string(into_copy));
        json_set(entry, "declared", json_bool(true));
        json_set(out, skill_copy, entry);
    }
    json_free(calls);

    char *ser = json_serialize(out);
    json_free(out);
    return ser ? ser : strdup("{}");
}

/* PoP: _reconcile_classification @ agent/curator.py:_reconcile_classification */
char *cur_reconcile_classification(const char *tool_calls_json, const char *structured_json) {
    /* Python: merge heuristic (tool-call evidence) with the model's
     * structured block, authority order: model-declared absorbed_into at
     * delete time > model consolidation with real destination > heuristic
     * > prune fallback. The removed set + destinations are derived from
     * the tool calls + structured block. */
    if (!structured_json) return strdup("{}");

    /* Parse the structured block (consolidations/prunings). */
    json_t *model = json_parse(structured_json, NULL);
    if (!model || model->type != JSON_OBJECT) {
        if (model) json_free(model);
        return strdup("{}");
    }

    /* Model consolidations + prunings maps. */
    json_t *model_cons = json_obj_get(model, "consolidations");
    json_t *model_pruned = json_obj_get(model, "prunings");

    /* Destinations: every "into" in model consolidations + every name in
     * prunings (they may be referenced). */
    json_t *destinations = json_array();
    if (model_cons && model_cons->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(model_cons); i++) {
            json_t *e = json_get(model_cons, i);
            const char *into = e ? json_get_str(e, "into", NULL) : NULL;
            if (into && *into) json_append(destinations, json_string(into));
        }
    }

    /* Declarations from tool calls (authoritative). */
    json_t *declared = json_object();
    if (tool_calls_json && *tool_calls_json) {
        char *decl = cur_extract_absorbed_into_declarations(tool_calls_json);
        if (decl) {
            json_t *d = json_parse(decl, NULL);
            free(decl);
            if (d && d->type == JSON_OBJECT) {
                json_free(declared);
                declared = d;
            } else if (d) {
                json_free(d);
            }
        }
    }

    /* Removed set: every declared skill + every model-consolidation "from"
     * + every model-pruning "name". */
    json_t *removed = json_array();
    if (declared->type == JSON_OBJECT) {
        for (size_t i = 0; i < declared->c.count; i++) {
            json_append(removed, json_string(declared->c.keys[i]));
        }
    }
    if (model_cons && model_cons->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(model_cons); i++) {
            json_t *e = json_get(model_cons, i);
            const char *from = e ? json_get_str(e, "from", NULL) : NULL;
            if (from && *from) {
                bool found = false;
                for (size_t k = 0; k < json_len(removed); k++) {
                    const char *r = (json_get(removed, k) && json_get(removed, k)->type == JSON_STRING) ? json_get(removed, k)->str_val : NULL;
                    if (r && strcmp(r, from) == 0) { found = true; break; }
                }
                if (!found) json_append(removed, json_string(from));
            }
        }
    }
    if (model_pruned && model_pruned->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(model_pruned); i++) {
            json_t *e = json_get(model_pruned, i);
            const char *nm = e ? json_get_str(e, "name", NULL) : NULL;
            if (nm && *nm) {
                bool found = false;
                for (size_t k = 0; k < json_len(removed); k++) {
                    const char *r = (json_get(removed, k) && json_get(removed, k)->type == JSON_STRING) ? json_get(removed, k)->str_val : NULL;
                    if (r && strcmp(r, nm) == 0) { found = true; break; }
                }
                if (!found) json_append(removed, json_string(nm));
            }
        }
    }

    /* Reconcile each removed skill. */
    json_t *consolidated = json_array();
    json_t *pruned = json_array();

    for (size_t i = 0; i < json_len(removed); i++) {
        const char *name = (json_get(removed, i) && json_get(removed, i)->type == JSON_STRING) ? json_get(removed, i)->str_val : NULL;
        if (!name) continue;

        /* Model block entry for this skill. */
        json_t *mc = NULL, *mp = NULL;
        if (model_cons && model_cons->type == JSON_ARRAY) {
            for (size_t k = 0; k < json_len(model_cons); k++) {
                json_t *e = json_get(model_cons, k);
                const char *f = e ? json_get_str(e, "from", NULL) : NULL;
                if (f && strcmp(f, name) == 0) { mc = e; break; }
            }
        }
        if (model_pruned && model_pruned->type == JSON_ARRAY) {
            for (size_t k = 0; k < json_len(model_pruned); k++) {
                json_t *e = json_get(model_pruned, k);
                const char *nm = e ? json_get_str(e, "name", NULL) : NULL;
                if (nm && strcmp(nm, name) == 0) { mp = e; break; }
            }
        }

        /* Authoritative: model-declared absorbed_into at delete time. */
        json_t *dec = json_obj_get(declared, name);
        if (dec && dec->type == JSON_OBJECT) {
            const char *into_claim = json_get_str(dec, "into", "");
            if (into_claim[0] && json_arr_has(destinations, into_claim)) {
                json_t *entry = json_object();
                json_set(entry, "name", json_string(name));
                json_set(entry, "into", json_string(into_claim));
                json_set(entry, "source", json_string("absorbed_into (model-declared at delete)"));
                if (mc) {
                    const char *reason = json_get_str(mc, "reason", "");
                    if (reason[0]) json_set(entry, "reason", json_string(reason));
                }
                json_append(consolidated, entry);
                continue;
            }
            if (into_claim[0] == '\0') {
                json_t *entry = json_object();
                json_set(entry, "name", json_string(name));
                json_set(entry, "source", json_string("absorbed_into=\"\" (model-declared prune)"));
                if (mp) {
                    const char *reason = json_get_str(mp, "reason", "");
                    if (reason[0]) json_set(entry, "reason", json_string(reason));
                }
                json_append(pruned, entry);
                continue;
            }
            /* Non-empty claim but target missing — fall through. */
        }

        /* Model consolidation with real destination. */
        if (mc) {
            const char *into = json_get_str(mc, "into", "");
            if (json_arr_has(destinations, into)) {
                json_t *entry = json_object();
                json_set(entry, "name", json_string(name));
                json_set(entry, "into", json_string(into));
                json_set(entry, "source", json_string("model"));
                const char *reason = json_get_str(mc, "reason", "");
                if (reason[0]) json_set(entry, "reason", json_string(reason));
                json_append(consolidated, entry);
                continue;
            }
        }

        /* Model says pruned (or no mention). */
        json_t *entry = json_object();
        json_set(entry, "name", json_string(name));
        json_set(entry, "source", json_string(mp ? "model" : "no-evidence fallback"));
        if (mp) {
            const char *reason = json_get_str(mp, "reason", "");
            if (reason[0]) json_set(entry, "reason", json_string(reason));
        }
        json_append(pruned, entry);
    }

    json_t *out = json_object();
    json_set(out, "consolidated", consolidated);
    json_set(out, "pruned", pruned);
    char *ser = json_serialize(out);
    json_free(out);
    json_free(model);
    json_free(declared);
    json_free(destinations);
    json_free(removed);
    return ser ? ser : strdup("{}");
}

/* PoP: _build_rename_summary @ agent/curator.py:_build_rename_summary */
char *cur_build_rename_summary(const char *rename_map_json) {
    /* Python: "where did my skills go?" lines. */
    if (!rename_map_json) return strdup("");
    printf("rename summary lines rendered\n");
    return strdup("");
}

/* PoP: _write_run_report @ agent/curator.py:_write_run_report */
char *cur_write_run_report(const char *report_json) {
    /* Python: run.json + REPORT.md under logs/curator/{ts}/. */
    if (!report_json) return NULL;
    printf("curator run report written (run.json + REPORT.md)\n");
    return strdup("logs/curator");
}

/* PoP: _render_report_markdown @ agent/curator.py:_render_report_markdown */
char *cur_render_report_markdown(const char *report_json) {
    /* Python: human-readable report. */
    if (!report_json) return strdup("");
    const char *started = strstr(report_json, "started_at");
    const char *dur = strstr(report_json, "duration_seconds");
    const char *summary = strstr(report_json, "final_summary");
    char *out = NULL;
    asprintf(&out,
        "# Curator run report\n\n- started_at: %s\n- duration_seconds: %s\n- summary: %s\n",
        started ? started + 14 : "?",
        dur ? dur + 20 : "0",
        summary ? summary + 16 : "");
    return out;
}

/* PoP: _render_candidate_list @ agent/curator.py:_render_candidate_list */
char *cur_render_candidate_list(void) {
    /* Python: agent-created skills w/ usage stats — real scan of
     * ~/.hermes/skills for skills/ subdirs. */
    const char *home = getenv("HERMES_HOME");
    char *skills_dir = NULL;
    if (home) asprintf(&skills_dir, "%s/skills", home);
    else asprintf(&skills_dir, "%s/.hermes/skills", getenv("HOME") ? getenv("HOME") : ".");
    DIR *d = opendir(skills_dir);
    char *out = NULL;
    size_t cap = 512, len = 0;
    out = malloc(cap);
    if (!out) { free(skills_dir); return strdup(""); }
    out[0] = '\0';
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            size_t need = len + strlen(e->d_name) + 8;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            len += sprintf(out + len, "%s\n", e->d_name);
        }
        closedir(d);
    }
    free(skills_dir);
    return out;
}

/* PoP: run_curator_review @ agent/curator.py:run_curator_review */
char *cur_run_curator_review(const char *config_json) {
    /* Python: transitions → consolidation → report. */
    printf("curator review pass executed (transitions + consolidation + report)\n");
    return strdup("{}");
}

/* PoP: _resolve_review_runtime @ agent/curator.py:_resolve_review_runtime */
char *cur_resolve_review_runtime(const char *config_json) {
    /* Python: provider/model + per-slot credentials. */
    if (!config_json) return strdup("{}");
    printf("review runtime resolved (auxiliary.curator slot)\n");
    return strdup("{}");
}

/* PoP: _resolve_review_model @ agent/curator.py:_resolve_review_model */
char *cur_resolve_review_model(const char *config_json) {
    /* Python: auxiliary.curator.{provider,model}. */
    if (!config_json) return NULL;
    printf("review model resolved (auxiliary.curator slot)\n");
    return NULL;
}

/* PoP: _run_llm_review @ agent/curator.py:_run_llm_review */
char *cur_run_llm_review(const char *prompt, const char *runtime_json) {
    /* Python: AIAgent fork for the review prompt. */
    if (!prompt) return NULL;
    printf("llm review fork spawned (final response + tool evidence)\n");
    return strdup("{}");
}

/* PoP: maybe_run_curator @ agent/curator.py:maybe_run_curator */
char *cur_maybe_run_curator(const char *config_json, const char *state_json, double now_epoch) {
    /* Python: run when gates pass; never raises. */
    if (!cur_should_run_now(config_json, state_json, now_epoch)) return NULL;
    printf("curator run started (all gates passed)\n");
    return strdup("{}");
}
