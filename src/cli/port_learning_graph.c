/*
 * port_learning_graph.c — I/O + composition layer for agent/learning_graph.py.
 *
 * Ports the filesystem-coupled functions the pure helpers compose with:
 *   _frontmatter, _iter_skill_files, _load_usage, build_skill_nodes,
 *   _memory_cards, _skill_roots, build_learning_graph.
 *
 * Reuses (no duplication):
 *   - libyaml        : parse SKILL.md frontmatter -> JSON (port_learning_graph_frontmatter)
 *   - libskillusage  : skill_usage_load() for _load_usage
 *   - port_learning_graph_helpers.c : the 9 pure JSON-in/out transforms
 *
 * JSON strings cross the boundary (malloc'd, caller-frees). Self-contained:
 * only the headers it needs.
 */

#include "port_learning_graph.h"
#include "hermes_json.h"
#include "libyaml/yaml.h"
#include "libskillusage/skill_usage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define LG_PATH_MAX 4096

/* Local HERMES_HOME resolver (mirrors Python get_hermes_home: HERMES_HOME env,
 * else ~/.hermes). Kept here so the module is self-contained. */
static void lg_hermes_home(char *out, size_t sz, const char *override)
{
    if (override && *override) { snprintf(out, sz, "%s", override); return; }
    const char *env = getenv("HERMES_HOME");
    if (env && *env) { snprintf(out, sz, "%s", env); return; }
    const char *home = getenv("HOME");
    if (!home) home = "";
    snprintf(out, sz, "%s/.hermes", home);
}

/* Recursively collect SKILL.md paths under a root into a JSON array of
 * {"source":src,"path":path}. Skips archive/hub/node_modules/.git segments. */
static void collect_skill_files(const char *source, const char *root, json_t *arr)
{
    DIR *d = opendir(root);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        const char *nm = de->d_name;
        if (strcmp(nm, ".") == 0 || strcmp(nm, "..") == 0) continue;
        if (strcmp(nm, ".archive") == 0 || strcmp(nm, ".hub") == 0 ||
            strcmp(nm, "node_modules") == 0 || strcmp(nm, ".git") == 0) continue;
        char full[LG_PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", root, nm);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            collect_skill_files(source, full, arr);
        } else if (S_ISREG(st.st_mode) && strcmp(nm, "SKILL.md") == 0) {
            json_t *o = json_new_object();
            json_object_set(o, "source", json_string(source));
            json_object_set(o, "path", json_string(full));
            json_array_append(arr, o);
        }
    }
    closedir(d);
}

/* PoP: learning_graph_iter_skill_files @ agent/learning_graph.py:_iter_skill_files */
/* Recursively collect SKILL.md paths under each root. Returns a malloc'd JSON
 * array of {"source":str,"path":str} for every SKILL.md found. */
char *learning_graph_iter_skill_files(const char *roots_json)
{
    json_t *roots = json_parse(roots_json ? roots_json : "[]", NULL);
    json_t *out = json_new_array();
    if (roots && roots->type == JSON_ARRAY) {
        size_t n = json_array_size(roots);
        for (size_t i = 0; i < n; i++) {
            json_t *r = json_array_get(roots, i);
            if (!r || r->type != JSON_OBJECT) continue;
            const char *src = json_object_get_string(r, "source", "");
            const char *p = json_object_get_string(r, "path", "");
            if (src && p && *p) collect_skill_files(src, p, out);
        }
    }
    if (roots) json_free(roots);
    char *dump = json_dumps(out, 0);
    char *ret = strdup(dump ? dump : "[]");
    free(dump);
    json_free(out);
    return ret;
}

/* PoP: learning_graph_frontmatter @ agent/learning_graph.py:_frontmatter */
/* Parses the leading YAML frontmatter block of a SKILL.md to a malloc'd JSON
 * object (or "{}" on failure). */
char *learning_graph_frontmatter(const char *skill_md_text)
{
    if (!skill_md_text) return strdup("{}");
    const char *p = skill_md_text;
    while (*p == ' ' || *p == '\n' || *p == '\r') p++;
    if (strncmp(p, "---", 3) != 0) return strdup("{}");
    p += 3;
    while (*p == '\n' || *p == '\r') p++;
    const char *close = strstr(p, "\n---");
    if (!close) close = strstr(p, "\r\n---");
    if (!close) return strdup("{}");
    size_t len = (size_t)(close - p);
    char *fm = (char *)malloc(len + 1);
    if (!fm) return strdup("{}");
    memcpy(fm, p, len);
    fm[len] = '\0';

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse(fm, &err);
    free(fm);
    if (err) free(err);
    if (!doc) return strdup("{}");

    char *js = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!js) return strdup("{}");
    char *ret = strdup(js);
    free(js);
    return ret;
}

/* PoP: learning_graph_load_usage @ agent/learning_graph.py:_load_usage */
/* hermes_home -> malloc'd JSON map keyed by skill name (use_count/state/
 * created_by/pinned/last_*_at). Mirrors Python load_usage() + .usage.json. */
char *learning_graph_load_usage(const char *hermes_home)
{
    skill_usage_map_t map;
    memset(&map, 0, sizeof(map));
    skill_usage_load(hermes_home, &map);

    json_t *out = json_new_object();
    for (int i = 0; i < map.count; i++) {
        const skill_usage_record_t *r = &map.records[i];
        if (!r->name[0]) continue;
        json_t *o = json_new_object();
        json_object_set(o, "use_count", json_int(r->use_count));
        json_object_set(o, "state", json_string(r->state[0] ? r->state : "active"));
        json_object_set(o, "created_by", json_string(r->created_by[0] ? r->created_by : (const char *)NULL));
        json_object_set(o, "pinned", json_bool(r->pinned));
        if (r->last_used_at[0])   json_object_set(o, "last_used_at",   json_string(r->last_used_at));
        if (r->last_viewed_at[0]) json_object_set(o, "last_viewed_at", json_string(r->last_viewed_at));
        if (r->last_patched_at[0])json_object_set(o, "last_patched_at",json_string(r->last_patched_at));
        if (r->created_at[0])     json_object_set(o, "created_at",     json_string(r->created_at));
        json_object_set(out, r->name, o);
    }
    char *dump = json_dumps(out, 0);
    char *ret = strdup(dump ? dump : "{}");
    free(dump);
    json_free(out);
    return ret;
}

/* PoP: learning_graph_build_skill_nodes @ agent/learning_graph.py:build_skill_nodes */
/* skill_roots_json: [{"source":str,"path":str},...]; usage_json: map from
 * load_usage. Returns malloc'd JSON object {"name":{node fields...}, ...}. */
char *learning_graph_build_skill_nodes(const char *skill_roots_json,
                                       const char *usage_json)
{
    char *files_json = learning_graph_iter_skill_files(skill_roots_json);
    json_t *files = json_parse(files_json ? files_json : "[]", NULL);
    free(files_json);
    json_t *usage = json_parse(usage_json ? usage_json : "{}", NULL);
    if (!usage || usage->type != JSON_OBJECT) { if (usage) json_free(usage); usage = json_new_object(); }

    json_t *nodes = json_new_object();
    if (files && files->type == JSON_ARRAY) {
        size_t n = json_array_size(files);
        for (size_t i = 0; i < n; i++) {
            json_t *f = json_array_get(files, i);
            if (!f || f->type != JSON_OBJECT) continue;
            const char *path = json_object_get_string(f, "path", "");
            const char *source = json_object_get_string(f, "source", "profile");
            if (!path || !*path) continue;

            /* read first 4000 bytes of the file */
            char buf[4096];
            size_t got = 0;
            FILE *ff = fopen(path, "rb");
            if (ff) {
                got = fread(buf, 1, sizeof(buf) - 1, ff);
                fclose(ff);
            }
            buf[got] = '\0';
            char *fm_json = learning_graph_frontmatter(buf);

            /* name = fm.name or parent dir name */
            json_t *fm = json_parse(fm_json, NULL);
            const char *name = NULL;
            if (fm && fm->type == JSON_OBJECT) {
                json_t *nm = json_object_get(fm, "name");
                if (nm && nm->type == JSON_STRING && json_string_value(nm)[0])
                    name = json_string_value(nm);
            }
            if (!name || !*name) {
                /* parent dir name */
                const char *slash = strrchr(path, '/');
                const char *parent = slash ? slash + 1 : path;
                /* parent is the skill dir; find its parent */
                size_t plen = (size_t)(slash - path);
                static char pbuf[LG_PATH_MAX];
                if (slash) { memcpy(pbuf, path, plen); pbuf[plen] = '\0'; }
                const char *s2 = strrchr(pbuf, '/');
                name = s2 ? s2 + 1 : (slash ? parent : path);
            }
            if (!name || !*name) { free(fm_json); if (fm) json_free(fm); continue; }

            /* skip duplicate names (matches Python `if name in nodes: continue`) */
            if (json_object_get(nodes, name)) { free(fm_json); if (fm) json_free(fm); continue; }

            /* usage record */
            json_t *rec = json_object_get(usage, name);
            const char *created_by = rec ? json_object_get_string(rec, "created_by", "") : "";
            int use_count = rec ? (int)json_object_get_number(rec, "use_count", 0) : 0;
            const char *state = rec ? json_object_get_string(rec, "state", "active") : "active";
            int pinned = rec ? json_object_get_bool(rec, "pinned", 0) : 0;

            /* timestamps: prefer usage activity, else file mtime */
            int found = 0;
            long long last_activity = 0;
            if (rec) {
                char *rec_json = json_dumps(rec, 0);
                last_activity = learning_graph_usage_timestamp(rec_json, &found);
                free(rec_json);
            }
            long long file_ts = 0;
            struct stat st;
            if (stat(path, &st) == 0) file_ts = (long long)st.st_mtime;
            long long ts = found ? last_activity : file_ts;

            /* category */
            char fallback[LG_PATH_MAX];
            /* .../skills/<category>/<skill>/SKILL.md -> parts[-3] */
            {
                char tmp[LG_PATH_MAX];
                snprintf(tmp, sizeof(tmp), "%s", path);
                char *parts[64]; int pc = 0;
                char *tok = strtok(tmp, "/");
                while (tok && pc < 64) { parts[pc++] = tok; tok = strtok(NULL, "/"); }
                if (pc >= 3) snprintf(fallback, sizeof(fallback), "%s", parts[pc - 3]);
                else snprintf(fallback, sizeof(fallback), "general");
            }
            char *cat = learning_graph_category(fm_json, fallback);
            /* related */
            char *related = learning_graph_related(fm_json);

            json_t *node = json_new_object();
            json_object_set(node, "name", json_string(name));
            json_object_set(node, "category", json_string(cat ? cat : "general"));
            json_object_set(node, "source", json_string(source ? source : "profile"));
            json_object_set(node, "timestamp", json_int(ts));
            json_object_set(node, "use_count", json_int(use_count));
            json_object_set(node, "state", json_string(state ? state : "active"));
            json_object_set(node, "created_by", json_string(created_by && *created_by ? created_by : (const char *)NULL));
            json_object_set(node, "pinned", json_bool(pinned));
            json_object_set(node, "related", json_parse(related ? related : "[]", NULL));

            json_object_set(nodes, name, node);

            free(cat); free(related); free(fm_json);
            if (fm) json_free(fm);
        }
    }
    if (files) json_free(files);
    if (usage) json_free(usage);

    char *dump = json_dumps(nodes, 0);
    char *ret = strdup(dump ? dump : "{}");
    free(dump);
    json_free(nodes);
    return ret;
}

/* PoP: learning_graph_memory_cards @ agent/learning_graph.py:_memory_cards */
/* Reads MEMORY.md / USER.md, splits on "\n§\n", returns a malloc'd JSON array
 * of {source, timestamp, title, body}. */
char *learning_graph_memory_cards(const char *hermes_home)
{
    char home[LG_PATH_MAX];
    lg_hermes_home(home, sizeof(home), hermes_home);

    static const char *files[2] = { "MEMORY.md", "USER.md" };
    static const char *sources[2] = { "memory", "profile" };

    json_t *cards = json_new_array();
    for (int fi = 0; fi < 2; fi++) {
        char path[LG_PATH_MAX];
        snprintf(path, sizeof(path), "%s/memories/%s", home, files[fi]);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *text = (char *)malloc(sz > 0 ? (size_t)sz + 1 : 1);
        size_t rd = fread(text, 1, (size_t)(sz > 0 ? sz : 0), f);
        fclose(f);
        text[rd] = '\0';

        long long file_ts = 0;
        struct stat st;
        if (stat(path, &st) == 0) file_ts = (long long)st.st_mtime;

        /* split on "\n§\n" */
        char *start = text;
        int chunk_idx = 0;
        while (1) {
            char *sep = strstr(start, "\n§\n");
            size_t chunk_len = sep ? (size_t)(sep - start) : strlen(start);
            char *chunk = (char *)malloc(chunk_len + 1);
            memcpy(chunk, start, chunk_len);
            chunk[chunk_len] = '\0';
            /* trim */
            while (*chunk && (chunk[chunk_len-1] == '\n' || chunk[chunk_len-1] == '\r' || chunk[chunk_len-1]==' '))
                chunk[--chunk_len] = '\0';
            while (*chunk == '\n' || *chunk == '\r' || *chunk == ' ') {
                memmove(chunk, chunk+1, strlen(chunk));
            }
            if (*chunk) {
                /* first line -> title (strip leading "# ") */
                char *nl = strchr(chunk, '\n');
                char *first = chunk;
                if (nl) *nl = '\0';
                char *title = first;
                while (*title == '#' || *title == ' ') title++;
                size_t tlen = strlen(title);
                char *title_out;
                if (tlen > 80) {
                    title_out = (char *)malloc(84);
                    memcpy(title_out, title, 80);
                    memcpy(title_out + 80, "…\0", 3);
                } else {
                    title_out = strdup(title);
                }
                char *body = (char *)malloc(chunk_len + 1);
                snprintf(body, chunk_len + 1, "%s", chunk);
                if (nl) *nl = '\n'; /* restore for body copy below */
                /* body = chunk up to 1200 */
                size_t bl = strlen(chunk);
                if (bl > 1200) chunk[1200] = '\0';

                json_t *c = json_new_object();
                json_object_set(c, "source", json_string(sources[fi]));
                json_object_set(c, "timestamp", json_int(file_ts + chunk_idx));
                json_object_set(c, "title", json_string(title_out));
                json_object_set(c, "body", json_string(chunk));
                json_array_append(cards, c);
                free(title_out);
                free(body);
                chunk_idx++;
            }
            free(chunk);
            if (!sep) break;
            start = sep + 3;
        }
        free(text);
    }
    char *dump = json_dumps(cards, 0);
    char *ret = strdup(dump ? dump : "[]");
    free(dump);
    json_free(cards);
    return ret;
}

/* PoP: learning_graph_skill_roots @ agent/learning_graph.py:_skill_roots */
/* Returns malloc'd JSON: [{"source":"base","path":repo_skills},
 *                         {"source":"profile","path":home/skills}]. */
char *learning_graph_skill_roots(const char *hermes_home, const char *repo_skills_dir)
{
    char home[LG_PATH_MAX];
    lg_hermes_home(home, sizeof(home), hermes_home);
    char home_skills[LG_PATH_MAX];
    snprintf(home_skills, sizeof(home_skills), "%s/skills", home);

    json_t *arr = json_new_array();
    json_t *base = json_new_object();
    json_object_set(base, "source", json_string("base"));
    json_object_set(base, "path", json_string(repo_skills_dir ? repo_skills_dir : ""));
    json_array_append(arr, base);
    json_t *prof = json_new_object();
    json_object_set(prof, "source", json_string("profile"));
    json_object_set(prof, "path", json_string(home_skills));
    json_array_append(arr, prof);

    char *dump = json_dumps(arr, 0);
    char *ret = strdup(dump ? dump : "[]");
    free(dump);
    json_free(arr);
    return ret;
}

/* PoP: build_learning_graph @ agent/learning_graph.py:build_learning_graph */
/* Full payload for the desktop learning panel. */
char *learning_graph_build(const char *hermes_home, const char *repo_skills_dir)
{
    char *roots = learning_graph_skill_roots(hermes_home, repo_skills_dir);
    char *usage = learning_graph_load_usage(hermes_home);
    char *nodes_map = learning_graph_build_skill_nodes(roots, usage);

    /* filter learned skills: source != "base" and (created_by=="agent" or use_count>0) */
    json_t *nodes = json_parse(nodes_map, NULL);
    json_t *learned = json_new_object();
    if (nodes && nodes->type == JSON_OBJECT) {
        size_t n = json_object_size(nodes);
        for (size_t i = 0; i < n; i++) {
            const char *key = json_object_get_key_at(nodes, i);
            json_t *val = json_object_get_at(nodes, i);
            if (!key || !val) continue;
            const char *src = json_object_get_string(val, "source", "");
            int uc = (int)json_object_get_number(val, "use_count", 0);
            const char *cb = json_object_get_string(val, "created_by", "");
            if (strcmp(src, "base") != 0 && (strcmp(cb, "agent") == 0 || uc > 0)) {
                json_object_set(learned, key, json_copy(val)); /* owned copy */
            }
        }
    }

    /* learned nodes -> JSON array for the pure helpers */
    json_t *learned_arr = json_new_array();
    {
        size_t n = json_object_size(learned);
        for (size_t i = 0; i < n; i++) {
            json_t *val = json_object_get_at(learned, i);
            if (val) json_array_append(learned_arr, json_copy(val));
        }
    }
    char *learned_json = json_dumps(learned_arr, 0);

    char *skill_edges_json = learning_graph_build_edges(learned_json);
    char *memory_json = learning_graph_memory_cards(hermes_home);

    /* memory edges need skills as [{"name":...}] */
    json_t *skills_arr = json_new_array();
    {
        size_t n = json_object_size(learned);
        for (size_t i = 0; i < n; i++) {
            const char *key = json_object_get_key_at(learned, i);
            json_t *s = json_new_object();
            json_object_set(s, "name", json_string(key ? key : ""));
            json_array_append(skills_arr, s);
        }
    }
    char *skills_json = json_dumps(skills_arr, 0);
    char *memory_edges_json = learning_graph_memory_skill_edges(memory_json, skills_json);

    /* clusters */
    json_t *clusters = json_new_object();
    {
        size_t n = json_object_size(learned);
        for (size_t i = 0; i < n; i++) {
            json_t *val = json_object_get_at(learned, i);
            if (!val) continue;
            const char *cat = json_object_get_string(val, "category", "general");
            json_t *c = json_object_get(clusters, cat);
            long cur = c ? json_number_value(c) : 0;
            json_object_set(clusters, cat, json_number(cur + 1));
        }
    }
    int mem_count = 0;
    {
        json_t *mj = json_parse(memory_json, NULL);
        if (mj && mj->type == JSON_ARRAY) mem_count = (int)json_array_size(mj);
        if (mj) json_free(mj);
    }
    if (mem_count > 0) json_object_set(clusters, "memory", json_int(mem_count));

    /* graph_nodes */
    json_t *gnodes = json_new_array();
    {
        size_t n = json_object_size(learned);
        for (size_t i = 0; i < n; i++) {
            const char *key = json_object_get_key_at(learned, i);
            json_t *val = json_object_get_at(learned, i);
            if (!key || !val) continue;
            json_t *g = json_new_object();
            json_object_set(g, "id", json_string(key));
            json_object_set(g, "label", json_string(key));
            json_object_set(g, "kind", json_string("skill"));
            json_object_set(g, "timestamp", json_copy(json_object_get(val, "timestamp")));
            json_object_set(g, "category", json_copy(json_object_get(val, "category")));
            json_object_set(g, "useCount", json_copy(json_object_get(val, "use_count")));
            json_object_set(g, "state", json_copy(json_object_get(val, "state")));
            json_object_set(g, "createdBy", json_copy(json_object_get(val, "created_by")));
            json_object_set(g, "pinned", json_copy(json_object_get(val, "pinned")));
            json_array_append(gnodes, g);
        }
    }
    {
        json_t *mj = json_parse(memory_json, NULL);
        if (mj && mj->type == JSON_ARRAY) {
            size_t n = json_array_size(mj);
            for (size_t i = 0; i < n; i++) {
                json_t *card = json_array_get(mj, i);
                json_t *g = json_new_object();
                char id[256];
                snprintf(id, sizeof(id), "memory:%s:%zu",
                         json_object_get_string(card, "source", ""), i);
                json_object_set(g, "id", json_string(id));
                json_object_set(g, "label", json_copy(json_object_get(card, "title")));
                json_object_set(g, "kind", json_string("memory"));
                json_object_set(g, "memorySource", json_copy(json_object_get(card, "source")));
                json_object_set(g, "timestamp", json_copy(json_object_get(card, "timestamp")));
                json_object_set(g, "category", json_string("memory"));
                json_object_set(g, "useCount", json_int(0));
                json_object_set(g, "state", json_string("active"));
                json_object_set(g, "createdBy", json_string("memory"));
                json_object_set(g, "pinned", json_bool(0));
                json_array_append(gnodes, g);
            }
        }
        if (mj) json_free(mj);
    }

    /* edges array [{source,target}] */
    json_t *gedges = json_new_array();
    {
        json_t *se = json_parse(skill_edges_json, NULL);
        if (se && se->type == JSON_ARRAY) {
            size_t n = json_array_size(se);
            for (size_t i = 0; i < n; i++) {
                json_t *p = json_array_get(se, i);
                if (!p || p->type != JSON_ARRAY || json_array_size(p) < 2) continue;
                json_t *e = json_new_object();
                json_object_set(e, "source", json_copy(json_array_get(p, 0)));
                json_object_set(e, "target", json_copy(json_array_get(p, 1)));
                json_array_append(gedges, e);
            }
        }
        if (se) json_free(se);
        json_t *me = json_parse(memory_edges_json, NULL);
        if (me && me->type == JSON_ARRAY) {
            size_t n = json_array_size(me);
            for (size_t i = 0; i < n; i++) {
                json_t *p = json_array_get(me, i);
                if (!p || p->type != JSON_ARRAY || json_array_size(p) < 2) continue;
                json_t *e = json_new_object();
                json_object_set(e, "source", json_copy(json_array_get(p, 0)));
                json_object_set(e, "target", json_copy(json_array_get(p, 1)));
                json_array_append(gedges, e);
            }
        }
        if (me) json_free(me);
    }

    /* clusters sorted desc */
    json_t *clusters_arr = json_new_array();
    {
        size_t cnt = json_object_size(clusters);
        char **ck = (char **)calloc(cnt ? cnt : 1, sizeof(char *));
        long *cv = (long *)calloc(cnt ? cnt : 1, sizeof(long));
        int k = 0;
        for (size_t i = 0; i < cnt; i++) {
            const char *key = json_object_get_key_at(clusters, i);
            json_t *val = json_object_get_at(clusters, i);
            if (!key || !val) continue;
            ck[k] = strdup(key);
            cv[k] = (long)json_number_value(val);
            k++;
        }
        for (int a = 0; a < k; a++)
            for (int b = a + 1; b < k; b++)
                if (cv[b] > cv[a]) { char *ts = ck[a]; ck[a] = ck[b]; ck[b] = ts; long tl = cv[a]; cv[a] = cv[b]; cv[b] = tl; }
        for (int a = 0; a < k; a++) {
            json_t *obj = json_new_object();
            json_object_set(obj, "category", json_string(ck[a]));
            json_object_set(obj, "count", json_int(cv[a]));
            json_array_append(clusters_arr, obj);
            free(ck[a]);
        }
        free(ck); free(cv);
    }

    /* stats */
    char *skill_edges_for_stats = learning_graph_build_edges(learned_json);
    char *stats_json = learning_graph_density_stats(learned_json, skill_edges_for_stats);
    json_t *stats = json_parse(stats_json, NULL);
    if (stats && stats->type == JSON_OBJECT) {
        json_object_set(stats, "memory_nodes", json_int(mem_count));
        json_object_set(stats, "memory_skill_edges", json_int(0)); /* filled below */
        json_object_set(stats, "learned_skills", json_int((long)json_object_size(learned)));
    }
    if (stats) {
        json_t *me = json_parse(memory_edges_json, NULL);
        if (me && me->type == JSON_ARRAY)
            json_object_set(stats, "memory_skill_edges", json_int((long)json_array_size(me)));
        if (me) json_free(me);
    }

    /* assemble */
    json_t *result = json_new_object();
    json_object_set(result, "nodes", gnodes);
    json_object_set(result, "edges", gedges);
    json_object_set(result, "clusters", clusters_arr);
    json_object_set(result, "memory", json_parse(memory_json, NULL));
    json_object_set(result, "stats", stats);

    char *dump = json_dumps(result, 0);
    char *ret = strdup(dump ? dump : "{}");

    /* cleanup.
     * NOTE: result owns gnodes, gedges, clusters_arr, stats, and the parsed
     * memory array — freeing those again would double-free. Only the truly
     * intermediate objects (skills_arr, learned_arr, clusters, learned,
     * nodes) and the malloc'd JSON strings are freed here. */
    free(dump);
    json_free(result);          /* frees gnodes/gedges/clusters_arr/stats/memory */
    free(stats_json);
    free(skill_edges_for_stats);
    free(skill_edges_json);
    free(memory_edges_json);
    free(skills_json);
    free(learned_json);
    free(memory_json);
    json_free(skills_arr);
    json_free(learned_arr);
    json_free(clusters);
    if (nodes) json_free(nodes);
    json_free(learned);
    free(nodes_map);
    free(usage);
    free(roots);

    return ret;
}
