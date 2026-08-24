/*
 * profiles.c — canonical C11 port of hermes_cli/web_routers/profiles.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
extern json_t *obj_values_array(const json_t *obj);



/* === dissolved from web_server_wrappers.c === */
/* PoP: ws_profiles_merge_by_id @ hermes_cli/web_routers/profiles.py:_merge_by_id */
void ws_profiles_merge_by_id(json_t *into, json_t *entries, const char *child_key) {
    if (!into || into->type != JSON_OBJECT) return;
    if (!entries || entries->type != JSON_ARRAY) return;
    size_t n = json_len(entries);
    for (size_t i = 0; i < n; i++) {
        json_t *entry = json_get(entries, i);
        if (!entry || entry->type != JSON_OBJECT) continue;
        const char *id = json_get_str(entry, "id", NULL);
        if (!id || !*id) continue;
        json_t *existing = json_obj_get(into, id);
        if (!existing) { json_set(into, id, json_copy(entry)); continue; }
        if (strcmp(child_key, "sessions") == 0) {
            json_t *es = json_obj_get(existing, "sessions");
            if (!es || es->type != JSON_ARRAY) { es = json_array(); json_set(existing, "sessions", es); }
            json_t *ns = json_obj_get(entry, "sessions");
            if (ns && ns->type == JSON_ARRAY) {
                size_t nn = json_len(ns);
                for (size_t k = 0; k < nn; k++) json_append(es, json_copy(json_get(ns, k)));
            }
        } else {
            json_t *children = json_object();
            json_t *ec = json_obj_get(existing, child_key);
            if (ec && ec->type == JSON_ARRAY) {
                size_t ecn = json_len(ec);
                for (size_t k = 0; k < ecn; k++) {
                    json_t *c = json_get(ec, k);
                    if (c && c->type == JSON_OBJECT) {
                        const char *cid = json_get_str(c, "id", NULL);
                        if (cid && *cid) json_set(children, cid, json_copy(c));
                    }
                }
            }
            ws_profiles_merge_by_id(children, json_obj_get(entry, child_key), "sessions");
            json_set(existing, child_key, obj_values_array(children));
            json_free(children);
        }
        if (json_has(existing, "sessionCount")) {
            double a = json_get_num(existing, "sessionCount", 0);
            double b = json_get_num(entry, "sessionCount", 0);
            json_set(existing, "sessionCount", json_number(a + b));
        }
    }
}

/* PoP: ws_profiles_merge_profile_tree @ hermes_cli/web_routers/profiles.py:_merge_profile_tree */
void ws_profiles_merge_profile_tree(json_t *merged, json_t *projects,
                                    const char *profile, int preview_limit) {
    if (!merged || merged->type != JSON_OBJECT) return;
    if (!projects || projects->type != JSON_ARRAY) return;
    size_t n = json_len(projects);
    for (size_t i = 0; i < n; i++) {
        json_t *project = json_get(projects, i);
        if (!project || project->type != JSON_OBJECT) continue;

        /* tag lane sessions with owning profile */
        json_t *repos = json_obj_get(project, "repos");
        if (repos && repos->type == JSON_ARRAY) {
            size_t rn = json_len(repos);
            for (size_t r = 0; r < rn; r++) {
                json_t *repo = json_get(repos, r);
                if (!repo || repo->type != JSON_OBJECT) continue;
                json_t *groups = json_obj_get(repo, "groups");
                if (groups && groups->type == JSON_ARRAY) {
                    size_t gn = json_len(groups);
                    for (size_t g = 0; g < gn; g++) {
                        json_t *lane = json_get(groups, g);
                        if (!lane || lane->type != JSON_OBJECT) continue;
                        json_t *sessions = json_obj_get(lane, "sessions");
                        if (sessions && sessions->type == JSON_ARRAY) {
                            size_t sn = json_len(sessions);
                            for (size_t s = 0; s < sn; s++) {
                                json_t *sess = json_get(sessions, s);
                                if (sess && sess->type == JSON_OBJECT) {
                                    json_set(sess, "profile", json_string(profile));
                                    json_set(sess, "is_default_profile", json_bool(strcmp(profile, "default") == 0));
                                }
                            }
                        }
                    }
                }
            }
        }
        json_t *pv = json_obj_get(project, "previewSessions");
        if (pv && pv->type == JSON_ARRAY) {
            size_t pn = json_len(pv);
            for (size_t p = 0; p < pn; p++) {
                json_t *sess = json_get(pv, p);
                if (sess && sess->type == JSON_OBJECT) {
                    json_set(sess, "profile", json_string(profile));
                    json_set(sess, "is_default_profile", json_bool(strcmp(profile, "default") == 0));
                }
            }
        }

        const char *path = json_get_str(project, "path", NULL);
        const char *key = (path && *path) ? path : json_get_str(project, "id", "");
        json_t *existing = json_obj_get(merged, key);
        if (!existing) { json_set(merged, key, json_copy(project)); continue; }

        bool ex_auto = json_get_bool(existing, "isAuto", false);
        bool pr_auto = json_get_bool(project, "isAuto", false);
        if (ex_auto && !pr_auto) {
            json_set(merged, key, json_copy(project));
            existing = json_obj_get(merged, key);
        }

        json_t *repos_dict = json_object();
        json_t *ex_repos = json_obj_get(existing, "repos");
        if (ex_repos && ex_repos->type == JSON_ARRAY) {
            size_t ern = json_len(ex_repos);
            for (size_t r = 0; r < ern; r++) {
                json_t *repo = json_get(ex_repos, r);
                if (repo && repo->type == JSON_OBJECT) {
                    const char *rid = json_get_str(repo, "id", NULL);
                    if (rid && *rid) json_set(repos_dict, rid, json_copy(repo));
                }
            }
        }
        ws_profiles_merge_by_id(repos_dict, json_obj_get(project, "repos"), "groups");
        json_set(existing, "repos", obj_values_array(repos_dict));
        json_free(repos_dict);

        json_set(existing, "sessionCount",
                 json_number(json_get_num(existing, "sessionCount", 0) + json_get_num(project, "sessionCount", 0)));
        json_set(existing, "totalTokens",
                 json_number(json_get_num(existing, "totalTokens", 0) + json_get_num(project, "totalTokens", 0)));
        json_set(existing, "totalCostUsd",
                 json_number(json_get_num(existing, "totalCostUsd", 0) + json_get_num(project, "totalCostUsd", 0)));
        double ela = json_get_num(existing, "lastActive", 0);
        double pla = json_get_num(project, "lastActive", 0);
        json_set(existing, "lastActive", json_number(ela > pla ? ela : pla));

        /* previews = existing + project previewSessions, sorted desc by
         * last_active/started_at, truncated to preview_limit. */
        json_t *previews = json_array();
        json_t *ex_pv = json_obj_get(existing, "previewSessions");
        if (ex_pv && ex_pv->type == JSON_ARRAY) {
            size_t pn = json_len(ex_pv);
            for (size_t p = 0; p < pn; p++) json_append(previews, json_copy(json_get(ex_pv, p)));
        }
        json_t *pr_pv = json_obj_get(project, "previewSessions");
        if (pr_pv && pr_pv->type == JSON_ARRAY) {
            size_t pn = json_len(pr_pv);
            for (size_t p = 0; p < pn; p++) json_append(previews, json_copy(json_get(pr_pv, p)));
        }
        /* selection sort descending by last_active/started_at, swapping the
         * array item slots in place (libjson exposes c.items). */
        size_t pn = json_len(previews);
        for (size_t a = 0; a < pn; a++) {
            size_t best = a;
            double bestscore = json_get_num(json_get(previews, a), "last_active",
                                            json_get_num(json_get(previews, a), "started_at", 0));
            for (size_t b = a + 1; b < pn; b++) {
                double bscore = json_get_num(json_get(previews, b), "last_active",
                                             json_get_num(json_get(previews, b), "started_at", 0));
                if (bscore > bestscore) { best = b; bestscore = bscore; }
            }
            if (best != a) {
                json_t *av = json_get(previews, a);
                json_t *bv = json_get(previews, best);
                json_t *tmp = av;
                previews->c.items[a] = bv;
                previews->c.items[best] = tmp;
            }
        }
        size_t take = json_len(previews);
        if (preview_limit >= 0 && take > (size_t)preview_limit) take = (size_t)preview_limit;
        json_t *final = json_array();
        for (size_t a = 0; a < take; a++) json_append(final, json_copy(json_get(previews, a)));
        json_set(existing, "previewSessions", final);  /* json_set steals */
        json_free(previews);
    }
}

/* PoP: a @ hermes_cli/web_routers/profiles.py:_pr_url_from_tool_output */
/* Return JSON "[number, url]" of the PR a tool result announces, or "null".
 * Python returns a (int, str) tuple or None. */
json_t *ws_profiles_pr_url_from_tool_output(const char *content) {
    if (!content || !*content) return NULL;
    json_t *parsed = json_parse(content, NULL);
    if (!parsed) return NULL;
    json_t *output = json_obj_get(parsed, "output");
    if (!output || output->type != JSON_STRING || !output->str_val) {
        json_free(parsed);
        return NULL;
    }
    /* regex: ^https://github\.com/[\w.-]+/[\w.-]+/pull/(\d+)/?$ */
    const char *s = output->str_val;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    const char *prefix = "https://github.com/";
    size_t pl = strlen(prefix);
    if (strncmp(s, prefix, pl) != 0) { json_free(parsed); return NULL; }
    const char *p = s + pl;
    /* owner segment: [\w.-]+ */
    const char *owner = p;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '-')) p++;
    if (p == owner || *p != '/') { json_free(parsed); return NULL; }
    p++;
    /* repo segment */
    const char *repo = p;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '-')) p++;
    if (p == repo || strncmp(p, "/pull/", 6) != 0) { json_free(parsed); return NULL; }
    p += 6;
    /* number */
    long num = 0;
    const char *numstart = p;
    while (*p && isdigit((unsigned char)*p)) { num = num * 10 + (*p - '0'); p++; }
    if (p == numstart) { json_free(parsed); return NULL; }
    /* optional trailing slash */
    if (*p == '/') p++;
    if (*p != '\0') { json_free(parsed); return NULL; }
    /* The returned URL is the full matched string (output.strip()), which is
     * `s` pointing into output->str_val owned by `parsed`. Copy it before
     * freeing parsed. */
    char *url = strdup(s);
    json_free(parsed);
    json_t *arr = json_array();
    json_append(arr, json_number((double)num));
    json_append(arr, json_string(url ? url : ""));
    free(url);
    return arr;
}
