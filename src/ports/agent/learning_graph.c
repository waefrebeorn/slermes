#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/agent/learning_graph.c — C11 port of agent/learning_graph.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_learning_graph.c ── */
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

/* PoP: name @ agent/learning_graph.py:_load_usage */
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
                memmove(chunk, chunk+1, strlen(chunk)+1); /* +1 keeps the null terminator */
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
                    memcpy(title_out + 80, "…", 3); /* UTF-8 ellipsis */
                    title_out[83] = '\0';
                } else {
                    title_out = strdup(title);
                }
                char *body = (char *)malloc(strlen(chunk) + 1);
                snprintf(body, strlen(chunk) + 1, "%s", chunk);
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

/* PoP: learning_graph_build @ agent/learning_graph.py:build_learning_graph */
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


/* ── from src/cli/port_learning_graph_helpers.c ── */
/*
 * port_learning_graph_helpers.c
 *
 * Pure, portable helpers ported from agent/learning_graph.py.
 *
 * These six are self-contained (no filesystem / network / SkillNode dataclass):
 *   - _hermes_meta(fm)        -> metadata.hermes dict (tolerant of string fm)
 *   - _related(fm)            -> list of related skill names
 *   - _category(fm, path)     -> category or path-derived fallback
 *   - _to_int_ts(value)       -> int epoch (from int/float/numeric/ISO string)
 *   - _usage_timestamp(rec)   -> first non-null activity timestamp field
 *   - _tokenize(text)         -> set of tokens len>=3 (returned as JSON array)
 *
 * Pure data-transform ports (operate on JSON representations of the SkillNode
 * dataclass — a node is {"name","category","source","use_count","created_by",
 * "related":[...]}), added per the "rewrite in C is the point" doctrine:
 *   - build_edges(nodes)               -> undirected related_skills edges
 *   - density_stats(nodes, edges)      -> graph density metrics
 *   - _memory_skill_edges(cards,skills)-> lexical memory↔skill edges
 *
 * Filesystem-coupled functions (build_skill_nodes, _iter_skill_files,
 * _load_usage, _memory_cards, _skill_roots, build_learning_graph, _frontmatter)
 * remain REAL_GAP pending fixture-driven ports; they compose the pure ones
 * below with parse_frontmatter / skill_usage_* (both already in the C tree).
 *
 * Module prefix used by the scanner for agent/learning_graph.py is
 * "learning_graph_".
 *
 * C name <- python name (learning_graph_ prefix):
 *   hermes_meta, related, category, to_int_ts, usage_timestamp, tokenize
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "hermes_json.h"
#include "port_learning_graph.h"

/* --- ISO-8601 -> time_t (UTC) ----------------------------------------- */
static int parse_iso_to_ts(const char *value, time_t *out)
{
    if (!value || !*value) return 0;
    int Y, M, D, h = 0, m = 0, s = 0;
    if (sscanf(value, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) < 3)
        return 0;
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = Y - 1900;
    tmv.tm_mon = M - 1;
    tmv.tm_mday = D;
    tmv.tm_hour = h;
    tmv.tm_min = m;
    tmv.tm_sec = s;
    tmv.tm_isdst = 0;
#ifdef _WIN32
    *out = _mkgmtime(&tmv);
#else
    *out = timegm(&tmv);
#endif
    return (*out != (time_t)-1) ? 1 : 0;
}

/* ---------------------------------------------------------------------- */
/* PoP: learning_graph_hermes_meta @ agent/learning_graph.py:_hermes_meta */
/* fm_json: JSON object. Returns malloc'd JSON of metadata.hermes (or "{}"). */
char *learning_graph_hermes_meta(const char *fm_json)
{
    json_t *fm = json_parse(fm_json ? fm_json : "{}", NULL);
    if (!fm || fm->type != JSON_OBJECT) { if (fm) json_free(fm); return strdup("{}"); }
    json_t *meta = json_object_get(fm, "metadata");
    json_t *hermes = (meta && meta->type == JSON_OBJECT) ? json_object_get(meta, "hermes") : NULL;
    char *out;
    if (hermes && hermes->type == JSON_OBJECT) {
        char *dump = json_dumps(hermes, 0);
        out = strdup(dump ? dump : "{}");
        free(dump);
    } else {
        out = strdup("{}");
    }
    json_free(fm);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: learning_graph_related @ agent/learning_graph.py:_related */
/* fm_json: JSON object. Returns malloc'd JSON array of related skill names. */
char *learning_graph_related(const char *fm_json)
{
    json_t *fm = json_parse(fm_json ? fm_json : "{}", NULL);
    if (!fm || fm->type != JSON_OBJECT) { if (fm) json_free(fm); return strdup("[]"); }

    json_t *raw = json_object_get(fm, "related_skills");
    if (!raw) {
        /* fall back to metadata.hermes.related_skills */
        json_t *meta = json_object_get(fm, "metadata");
        json_t *hermes = (meta && meta->type == JSON_OBJECT) ? json_object_get(meta, "hermes") : NULL;
        if (hermes && hermes->type == JSON_OBJECT)
            raw = json_object_get(hermes, "related_skills");
    }

    json_t *arr = json_new_array();
    if (raw && raw->type == JSON_ARRAY) {
        size_t n = json_array_size(raw);
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_array_get(raw, i);
            if (e && e->type == JSON_STRING) {
                const char *sv = json_string_value(e);
                char *cp = strdup(sv ? sv : "");
                char *p = cp;
                while (*p) { *p = (char)tolower((unsigned char)*p); if (*p == ' ') *p = '\0'; p++; }
                if (cp[0]) json_array_append(arr, json_string(cp));
                free(cp);
            }
        }
    } else if (raw && raw->type == JSON_STRING) {
        /* "a, b, c" or "[a, b, c]" -> split on comma */
        const char *sv = json_string_value(raw);
        char *buf = strdup(sv ? sv : "");
        /* strip surrounding brackets */
        char *b = buf;
        if (*b == '[') b++;
        size_t bl = strlen(b);
        if (bl > 0 && b[bl-1] == ']') b[bl-1] = '\0';
        char *tok = strtok(b, ",");
        while (tok) {
            /* trim */
            while (*tok == ' ') tok++;
            char *end = tok + strlen(tok);
            while (end > tok && end[-1] == ' ') end--;
            *end = '\0';
            if (*tok) json_array_append(arr, json_string(tok));
            tok = strtok(NULL, ",");
        }
        free(buf);
    }
    char *out = json_dumps(arr, 0);
    char *ret = strdup(out ? out : "[]");
    free(out);
    json_free(arr);
    json_free(fm);
    return ret;
}

/* ---------------------------------------------------------------------- */
/* PoP: learning_graph_category @ agent/learning_graph.py:_category */
/* fm_json: JSON object; fallback: caller-computed path-derived category
 * (mirrors skill_md.parts[-3]). Returns malloc'd category string. */
char *learning_graph_category(const char *fm_json, const char *fallback)
{
    json_t *fm = json_parse(fm_json ? fm_json : "{}", NULL);
    if (!fm || fm->type != JSON_OBJECT) { if (fm) json_free(fm); return strdup(fallback ? fallback : "general"); }
    json_t *cat = json_object_get(fm, "category");
    if (!cat || cat->type != JSON_STRING || !json_string_value(cat)[0]) {
        json_t *meta = json_object_get(fm, "metadata");
        json_t *hermes = (meta && meta->type == JSON_OBJECT) ? json_object_get(meta, "hermes") : NULL;
        if (hermes && hermes->type == JSON_OBJECT) {
            json_t *hc = json_object_get(hermes, "category");
            if (hc && hc->type == JSON_STRING && json_string_value(hc)[0]) cat = hc;
        }
    }
    char *out;
    if (cat && cat->type == JSON_STRING && json_string_value(cat)[0])
        out = strdup(json_string_value(cat));
    else
        out = strdup(fallback ? fallback : "general");
    json_free(fm);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: scalar @ agent/learning_graph.py:_to_int_ts */
/* value_json: a JSON scalar (number or string). *found set to 1 on success.
 * Returns epoch seconds (or 0 with *found=0 on failure). */
long long learning_graph_to_int_ts(const char *value_json, int *found)
{
    *found = 0;
    json_t *v = json_parse(value_json ? value_json : "null", NULL);
    if (!v) return 0;
    long long result = 0;
    if (v->type == JSON_NUMBER) {
        result = (long long)json_number_value(v);
        *found = 1;
    } else if (v->type == JSON_STRING) {
        const char *s = json_string_value(v);
        if (s && *s) {
            char *end = NULL;
            double d = strtod(s, &end);
            if (end != s && *end == '\0') {
                result = (long long)d;
                *found = 1;
            } else {
                /* ISO string */
                time_t t;
                if (parse_iso_to_ts(s, &t)) { result = (long long)t; *found = 1; }
            }
        }
    }
    json_free(v);
    return result;
}

/* ---------------------------------------------------------------------- */
/* PoP: learning_graph_usage_timestamp @ agent/learning_graph.py:_usage_timestamp */
/* rec_json: JSON object. Returns first valid ts among the priority keys,
 * or 0 with *found=0 if none. */
long long learning_graph_usage_timestamp(const char *rec_json, int *found)
{
    static const char *keys[] = {"last_activity_at", "last_used_at", "last_viewed_at",
                                 "last_patched_at", "created_at"};
    *found = 0;
    json_t *rec = json_parse(rec_json ? rec_json : "{}", NULL);
    if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); return 0; }
    long long best = 0;
    for (int i = 0; i < 5; i++) {
        json_t *kv = json_object_get(rec, keys[i]);
        if (!kv) continue;
        char *sv = json_dumps(kv, 0);
        int f = 0;
        long long ts = learning_graph_to_int_ts(sv, &f);
        free(sv);
        if (f) { best = ts; *found = 1; break; }
    }
    json_free(rec);
    return best;
}

/* ---------------------------------------------------------------------- */
/* PoP: tokens @ agent/learning_graph.py:_tokenize */
/* Returns malloc'd JSON array of tokens (len>=3) from text. */
char *learning_graph_tokenize(const char *text)
{
    json_t *arr = json_new_array();
    if (text && *text) {
        char *buf = strdup(text);
        for (char *p = buf; *p; p++) {
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
        }
        /* split on runs of non [a-z0-9] */
        char *start = buf;
        char *p = buf;
        while (1) {
            int isword = (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9');
            if (!isword || *p == '\0') {
                if (p > start) {
                    char saved = *p;
                    *p = '\0';
                    size_t len = strlen(start);
                    if (len >= 3) json_array_append(arr, json_string(start));
                    *p = saved;
                }
                start = p + 1;
                if (*p == '\0') break;
            }
            p++;
        }
        free(buf);
    }
    char *out = json_dumps(arr, 0);
    char *ret = strdup(out ? out : "[]");
    free(out);
    json_free(arr);
    return ret;
}

/* ====================================================================== */
/* Pure data-transform ports (JSON-in / JSON-out).                        */
/* ====================================================================== */

/* internal: build a sorted, de-duplicated token set from text (len>=3). */
static char **lg_token_set(const char *text, int *out_n)
{
    *out_n = 0;
    char *js = learning_graph_tokenize(text);
    json_t *arr = json_parse(js ? js : "[]", NULL);
    free(js);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return NULL; }
    size_t n = json_array_size(arr);
    char **toks = (char **)calloc(n ? n : 1, sizeof(char *));
    int cnt = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_array_get(arr, i);
        if (!e || e->type != JSON_STRING) continue;
        const char *sv = json_string_value(e);
        if (!sv) continue;
        int dup = 0;
        for (int j = 0; j < cnt; j++) if (strcmp(toks[j], sv) == 0) { dup = 1; break; }
        if (!dup) toks[cnt++] = strdup(sv);
    }
    json_free(arr);
    *out_n = cnt;
    return toks;
}

static void lg_free_set(char **toks, int n)
{
    for (int i = 0; i < n; i++) free(toks[i]);
    free(toks);
}

static int lg_set_intersect_count(char **a, int an, char **b, int bn)
{
    int c = 0;
    for (int i = 0; i < an; i++)
        for (int j = 0; j < bn; j++)
            if (strcmp(a[i], b[j]) == 0) { c++; break; }
    return c;
}

/* PoP: edges @ agent/learning_graph.py:build_edges */
/* nodes_json: JSON array of {"name":str,"related":[str,...]}. Returns a
 * malloc'd JSON array of [a,b] sorted, deduped undirected edges (endpoints
 * both present, no self-loops). Preserves node/related iteration order. */
char *learning_graph_build_edges(const char *nodes_json)
{
    json_t *nodes = json_parse(nodes_json ? nodes_json : "[]", NULL);
    json_t *out = json_new_array();
    if (!nodes || nodes->type != JSON_ARRAY) { if (nodes) json_free(nodes); goto done; }

    size_t nn = json_array_size(nodes);
    /* collect names for membership */
    char **names = (char **)calloc(nn ? nn : 1, sizeof(char *));
    int ncnt = 0;
    for (size_t i = 0; i < nn; i++) {
        json_t *nd = json_array_get(nodes, i);
        if (!nd || nd->type != JSON_OBJECT) continue;
        json_t *nm = json_object_get(nd, "name");
        if (nm && nm->type == JSON_STRING && json_string_value(nm))
            names[ncnt++] = strdup(json_string_value(nm));
    }
    /* dedup set of "a\x01b" edge keys */
    char **seen = (char **)calloc(64, sizeof(char *));
    int seen_cap = 64, seen_n = 0;

    for (size_t i = 0; i < nn; i++) {
        json_t *nd = json_array_get(nodes, i);
        if (!nd || nd->type != JSON_OBJECT) continue;
        json_t *nm = json_object_get(nd, "name");
        if (!nm || nm->type != JSON_STRING || !json_string_value(nm)) continue;
        const char *self = json_string_value(nm);
        json_t *rel = json_object_get(nd, "related");
        if (!rel || rel->type != JSON_ARRAY) continue;
        size_t rn = json_array_size(rel);
        for (size_t r = 0; r < rn; r++) {
            json_t *te = json_array_get(rel, r);
            if (!te || te->type != JSON_STRING) continue;
            const char *tgt = json_string_value(te);
            if (!tgt || strcmp(tgt, self) == 0) continue;
            /* target must exist among names */
            int found = 0;
            for (int k = 0; k < ncnt; k++) if (strcmp(names[k], tgt) == 0) { found = 1; break; }
            if (!found) continue;
            /* sorted (a,b) */
            const char *a = self, *b = tgt;
            if (strcmp(a, b) > 0) { const char *t = a; a = b; b = t; }
            /* dedup key */
            size_t kl = strlen(a) + strlen(b) + 2;
            char *key = (char *)malloc(kl);
            snprintf(key, kl, "%s\x01%s", a, b);
            int dup = 0;
            for (int s = 0; s < seen_n; s++) if (strcmp(seen[s], key) == 0) { dup = 1; break; }
            if (dup) { free(key); continue; }
            if (seen_n >= seen_cap) { seen_cap *= 2; seen = (char **)realloc(seen, seen_cap * sizeof(char *)); }
            seen[seen_n++] = key;
            json_t *pair = json_new_array();
            json_array_append(pair, json_string(a));
            json_array_append(pair, json_string(b));
            json_array_append(out, pair);
        }
    }
    for (int k = 0; k < ncnt; k++) free(names[k]);
    free(names);
    for (int s = 0; s < seen_n; s++) free(seen[s]);
    free(seen);
    json_free(nodes);
done:;
    char *dump = json_dumps(out, 0);
    char *ret = strdup(dump ? dump : "[]");
    free(dump);
    json_free(out);
    return ret;
}

/* PoP: objects @ agent/learning_graph.py:density_stats */
/* nodes_json: JSON array of node objects (name,category,use_count,created_by).
 * edges_json: JSON array of [a,b] pairs. Returns malloc'd JSON object of graph
 * density metrics matching the Python dict (edges_per_node round-3,
 * isolated_pct round-1, top_categories top-8 by count desc / first-seen). */
char *learning_graph_density_stats(const char *nodes_json, const char *edges_json)
{
    json_t *nodes = json_parse(nodes_json ? nodes_json : "[]", NULL);
    json_t *edges = json_parse(edges_json ? edges_json : "[]", NULL);
    json_t *res = json_new_object();

    int node_count = (nodes && nodes->type == JSON_ARRAY) ? (int)json_array_size(nodes) : 0;
    int edge_count = (edges && edges->type == JSON_ARRAY) ? (int)json_array_size(edges) : 0;

    /* linked node set */
    char **linked = (char **)calloc(edge_count * 2 + 1, sizeof(char *));
    int linked_n = 0;
    for (int i = 0; i < edge_count; i++) {
        json_t *e = json_array_get(edges, i);
        if (!e || e->type != JSON_ARRAY || json_array_size(e) < 2) continue;
        for (int p = 0; p < 2; p++) {
            json_t *ep = json_array_get(e, p);
            if (!ep || ep->type != JSON_STRING) continue;
            const char *sv = json_string_value(ep);
            int dup = 0;
            for (int j = 0; j < linked_n; j++) if (strcmp(linked[j], sv) == 0) { dup = 1; break; }
            if (!dup) linked[linked_n++] = strdup(sv);
        }
    }

    /* categories in first-seen order; agent_created; used */
    char **cats = (char **)calloc(node_count ? node_count : 1, sizeof(char *));
    int *cat_counts = (int *)calloc(node_count ? node_count : 1, sizeof(int));
    int cat_n = 0, agent_created = 0, used = 0;
    for (int i = 0; i < node_count; i++) {
        json_t *nd = json_array_get(nodes, i);
        if (!nd || nd->type != JSON_OBJECT) continue;
        const char *cat = json_object_get_string(nd, "category", "general");
        if (!cat) cat = "general";
        int idx = -1;
        for (int j = 0; j < cat_n; j++) if (strcmp(cats[j], cat) == 0) { idx = j; break; }
        if (idx < 0) { cats[cat_n] = strdup(cat); cat_counts[cat_n] = 1; cat_n++; }
        else cat_counts[idx]++;
        const char *cb = json_object_get_string(nd, "created_by", NULL);
        if (cb && strcmp(cb, "agent") == 0) agent_created++;
        double uc = json_object_get_number(nd, "use_count", 0);
        if (uc > 0) used++;
    }

    int denom = node_count ? node_count : 1;
    double epn = (double)edge_count / denom;
    double iso = 100.0 * (denom - linked_n) / denom;

    json_object_set(res, "nodes", json_int(node_count));
    json_object_set(res, "related_edges", json_int(edge_count));
    json_object_set(res, "edges_per_node", json_number(epn));
    json_object_set(res, "linked_nodes", json_int(linked_n));
    json_object_set(res, "isolated_pct", json_number(iso));
    json_object_set(res, "categories", json_int(cat_n));
    json_object_set(res, "agent_created", json_int(agent_created));
    json_object_set(res, "used", json_int(used));

    /* top_categories: stable sort by count desc (first-seen tie-break), top 8 */
    int *order = (int *)calloc(cat_n ? cat_n : 1, sizeof(int));
    for (int i = 0; i < cat_n; i++) order[i] = i;
    for (int i = 0; i < cat_n; i++)
        for (int j = i + 1; j < cat_n; j++)
            if (cat_counts[order[j]] > cat_counts[order[i]]) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    json_t *top = json_new_array();
    int limit = cat_n < 8 ? cat_n : 8;
    for (int i = 0; i < limit; i++) {
        json_t *pair = json_new_array();
        json_array_append(pair, json_string(cats[order[i]]));
        json_array_append(pair, json_int(cat_counts[order[i]]));
        json_array_append(top, pair);
    }
    json_object_set(res, "top_categories", top);
    free(order);

    for (int i = 0; i < linked_n; i++) free(linked[i]);
    free(linked);
    for (int i = 0; i < cat_n; i++) free(cats[i]);
    free(cats); free(cat_counts);
    if (nodes) json_free(nodes);
    if (edges) json_free(edges);

    char *dump = json_dumps(res, 0);
    char *ret = strdup(dump ? dump : "{}");
    free(dump);
    json_free(res);
    return ret;
}

/* PoP: learning_graph_memory_skill_edges @ agent/learning_graph.py:_memory_skill_edges */
/* cards_json: array of {"source","title","body"}; skills_json: array of {"name"}.
 * Returns malloc'd JSON array of [mem_id, skill_name] lexical-overlap edges
 * (score = 6 if skill name substring in text, + |name_tokens ∩ text_tokens|;
 * top 4 per card, sorted by score desc then name asc). */
char *learning_graph_memory_skill_edges(const char *cards_json, const char *skills_json)
{
    json_t *cards = json_parse(cards_json ? cards_json : "[]", NULL);
    json_t *skills = json_parse(skills_json ? skills_json : "[]", NULL);
    json_t *out = json_new_array();

    int scount = (skills && skills->type == JSON_ARRAY) ? (int)json_array_size(skills) : 0;
    /* precompute per-skill: name, name_lower, name_token_set */
    char **snames = (char **)calloc(scount ? scount : 1, sizeof(char *));
    char **slower = (char **)calloc(scount ? scount : 1, sizeof(char *));
    char ***stoks = (char ***)calloc(scount ? scount : 1, sizeof(char **));
    int *stok_n = (int *)calloc(scount ? scount : 1, sizeof(int));
    int sn = 0;
    for (int i = 0; i < scount; i++) {
        json_t *sk = json_array_get(skills, i);
        if (!sk || sk->type != JSON_OBJECT) continue;
        const char *nm = json_object_get_string(sk, "name", NULL);
        if (!nm) continue;
        snames[sn] = strdup(nm);
        char *lo = strdup(nm);
        for (char *p = lo; *p; p++) *p = (char)tolower((unsigned char)*p);
        slower[sn] = lo;
        stoks[sn] = lg_token_set(nm, &stok_n[sn]);
        sn++;
    }

    int ccount = (cards && cards->type == JSON_ARRAY) ? (int)json_array_size(cards) : 0;
    for (int ci = 0; ci < ccount; ci++) {
        json_t *card = json_array_get(cards, ci);
        if (!card || card->type != JSON_OBJECT) continue;
        const char *src = json_object_get_string(card, "source", "");
        const char *title = json_object_get_string(card, "title", "");
        const char *body = json_object_get_string(card, "body", "");
        /* mem_id = "memory:<source>:<idx>" */
        char mem_id[512];
        snprintf(mem_id, sizeof(mem_id), "memory:%s:%d", src ? src : "", ci);
        /* text = lower("title\nbody") */
        size_t tl = strlen(title ? title : "") + strlen(body ? body : "") + 2;
        char *text = (char *)malloc(tl);
        snprintf(text, tl, "%s\n%s", title ? title : "", body ? body : "");
        for (char *p = text; *p; p++) *p = (char)tolower((unsigned char)*p);
        int ttn = 0;
        char **ttoks = lg_token_set(text, &ttn);

        /* score each skill */
        int *scores = (int *)calloc(sn ? sn : 1, sizeof(int));
        int *idxs = (int *)calloc(sn ? sn : 1, sizeof(int));
        int scored_n = 0;
        for (int s = 0; s < sn; s++) {
            int score = 0;
            if (slower[s][0] && strstr(text, slower[s])) score += 6;
            score += lg_set_intersect_count(stoks[s], stok_n[s], ttoks, ttn);
            if (score > 0) { scores[scored_n] = score; idxs[scored_n] = s; scored_n++; }
        }
        /* stable sort by (-score, name asc) */
        for (int i = 0; i < scored_n; i++)
            for (int j = i + 1; j < scored_n; j++) {
                int swap = 0;
                if (scores[idxs[j]] > scores[idxs[i]]) swap = 1;
                else if (scores[idxs[j]] == scores[idxs[i]] &&
                         strcmp(snames[idxs[j]], snames[idxs[i]]) < 0) swap = 1;
                if (swap) { int t = idxs[i]; idxs[i] = idxs[j]; idxs[j] = t; }
            }
        int lim = scored_n < 4 ? scored_n : 4;
        for (int i = 0; i < lim; i++) {
            json_t *pair = json_new_array();
            json_array_append(pair, json_string(mem_id));
            json_array_append(pair, json_string(snames[idxs[i]]));
            json_array_append(out, pair);
        }
        free(scores); free(idxs);
        lg_free_set(ttoks, ttn);
        free(text);
    }

    for (int i = 0; i < sn; i++) { free(snames[i]); free(slower[i]); lg_free_set(stoks[i], stok_n[i]); }
    free(snames); free(slower); free(stoks); free(stok_n);
    if (cards) json_free(cards);
    if (skills) json_free(skills);

    char *dump = json_dumps(out, 0);
    char *ret = strdup(dump ? dump : "[]");
    free(dump);
    json_free(out);
    return ret;
}

