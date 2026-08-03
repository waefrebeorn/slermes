/*
 * port_learning_mutations.c
 *
 * Faithful C11 port of agent/learning_mutations.py (15 features):
 *   parse_node_kind, _memories_dir, _parse_memory_id, _memory_local_index,
 *   _locate_memory, node_detail, _node_detail, delete_node, _delete_skill,
 *   _delete_memory, edit_node, _edit_skill, _edit_memory, _write_memory,
 *   _clear_skill_cache.
 *
 * Reuses:
 *   - libskillusage (skill_usage_load/get_record/archive) for skill pins+archive
 *   - learning_graph_memory_cards() for the global memory-card ordering
 *   - clear_skills_system_prompt_cache() after skill mutations
 *   - POSIX I/O + the § delimiter parser (ENTRY_DELIMITER = "\n§\n")
 *
 * Every public entry returns malloc'd JSON (caller frees). Mirrors the Python
 * module's {"ok":bool,"message":str} / {"ok":true,"kind":...,"content":...}
 * return shapes exactly.
 */

#include "libyaml/yaml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include "hermes_json.h"
#include "port_learning_mutations.h"
#include "port_learning_graph.h"
#include "libskillusage/skill_usage.h"
#include "hermes_system_prompt.h"

/* ENTRY_DELIMITER mirrors tools/memory_tool.py (ENTRY_DELIMITER = "\n§\n"). */
#define LM_ENTRY_DELIM "\n§\n"
#define LM_MAX_PATH 2048
#define LM_MAX_FIELD 512

/* ── HERMES_HOME resolution ─────────────────────────────────────────────── */
static void lm_hermes_home(char *out, size_t sz)
{
    const char *h = getenv("HERMES_HOME");
    if (h && *h) { snprintf(out, sz, "%s", h); return; }
    const char *home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (home) snprintf(out, sz, "%s/.hermes", home);
    else snprintf(out, sz, "%s", ".hermes");
}

/* small strstrip helper returning malloc'd trimmed copy */
static char *strstrip_copy(const char *s)
{
    if (!s) return strdup("");
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    size_t L = strlen(s);
    while (L > 0 && (s[L-1] == ' ' || s[L-1] == '\t' || s[L-1] == '\r' || s[L-1] == '\n')) L--;
    return strndup(s, L);
}

static void lm_yaml_count_keys(const char *key, const char *val, void *user)
{
    (void)key; (void)val;
    (*(int *)user)++;
}

/* PoP: learning_mutations_validate_frontmatter @ agent/learning_mutations.py:_edit_skill */
/* Faithful port of tools/skill_manager_tool._validate_frontmatter.
 * Returns a malloc'd error string (caller frees) or NULL if valid. */
static char *lm_validate_frontmatter(const char *content)
{
    if (!content || !*content) {
        return strdup("Content cannot be empty.");
    }
    char *stripped = strstrip_copy(content);
    int empty = (*stripped == '\0');
    free(stripped);
    if (empty) {
        return strdup("Content cannot be empty.");
    }
    /* must start with --- */
    {
        char *s = strdup(content);
        const char *p = s;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        int starts = (strncmp(p, "---", 3) == 0);
        free(s);
        if (!starts) return strdup("SKILL.md must start with YAML frontmatter (---). See existing skills for format.");
    }
    /* find closing \n---\s*\n in content[3:] */
    const char *rest = content + 3;
    const char *end = strstr(rest, "\n---\n");
    if (!end) {
        end = strstr(rest, "\n---");
        if (end) {
            const char *a = end + 4;
            while (*a == ' ' || *a == '\t') a++;
            if (*a != '\n' && *a != '\0') end = NULL;
        }
    }
    if (!end) return strdup("SKILL.md frontmatter is not closed. Ensure you have a closing '---' line.");
    size_t fm_len = (size_t)(end - rest);
    char *yaml_content = (char *)malloc(fm_len + 1);
    memcpy(yaml_content, rest, fm_len);
    yaml_content[fm_len] = '\0';

    char *yerr = NULL;
    yaml_doc_t *doc = yaml_parse(yaml_content, &yerr);
    free(yaml_content);
    if (!doc) {
        char *e = malloc(64 + (yerr ? strlen(yerr) : 0));
        snprintf(e, 64 + (yerr ? strlen(yerr) : 0), "YAML frontmatter parse error: %s", yerr ? yerr : "unknown");
        if (yerr) free(yerr);
        return e;
    }
    int nkeys = 0;
    yaml_iterate(doc, lm_yaml_count_keys, &nkeys);
    if (nkeys == 0) {
        yaml_free(doc);
        return strdup("Frontmatter must be a YAML mapping (key: value pairs).");
    }
    const char *name = yaml_get_string(doc, "name");
    if (!name) {
        yaml_free(doc);
        return strdup("Frontmatter must include 'name' field.");
    }
    const char *desc = yaml_get_string(doc, "description");
    if (!desc) {
        yaml_free(doc);
        return strdup("Frontmatter must include 'description' field.");
    }
    if ((int)strlen(desc) > 1024) {
        yaml_free(doc);
        char *e = malloc(64);
        snprintf(e, 64, "Description exceeds 1024 characters.");
        return e;
    }
    /* body after frontmatter must be non-empty */
    const char *b = end + 4;
    while (*b == ' ' || *b == '\t') b++;
    if (*b == '\n') b++;
    while (*b == ' ' || *b == '\t' || *b == '\r' || *b == '\n') b++;
    if (!*b) {
        yaml_free(doc);
        return strdup("SKILL.md must have content after the frontmatter (instructions, procedures, etc.).");
    }
    yaml_free(doc);
    return NULL;
}

/* PoP: learning_mutations_read_memory_file @ agent/learning_mutations.py:_write_memory */
/* ── memory-file § parser (MemoryStore._read_file) ──────────────────────── */
char **learning_mutations_read_memory_file(const char *path, int *out_n)
{
    *out_n = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    rewind(f);
    if (fsz <= 0) { fclose(f); return NULL; }
    char *raw = (char *)malloc((size_t)fsz + 1);
    size_t nr = fread(raw, 1, (size_t)fsz, f);
    raw[nr] = '\0';
    fclose(f);

    /* Split on the literal ENTRY_DELIMITER sequence "\n§\n" (strtok would
     * split on each char in the set, which is wrong). Preserve each entry's
     * internal newlines so the full body survives. */
    int cap = 4, n = 0;
    char **arr = (char **)calloc(cap, sizeof(char *));
    char *cur = raw;
    while (1) {
        char *sep = strstr(cur, LM_ENTRY_DELIM);
        size_t len = sep ? (size_t)(sep - cur) : strlen(cur);
        /* strip leading/trailing whitespace of the entry */
        char *s = cur;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
        size_t L = len - (size_t)(s - cur);
        while (L > 0 && (s[L-1] == ' ' || s[L-1] == '\t' || s[L-1] == '\r' || s[L-1] == '\n')) L--;
        if (*s && L > 0) {
            if (n >= cap) { cap *= 2; arr = (char **)realloc(arr, cap * sizeof(char *)); }
            arr[n++] = strndup(s, L);
        }
        if (!sep) break;
        cur = sep + strlen(LM_ENTRY_DELIM);
    }
    free(raw);
    /* NULL-terminate so callers can iterate safely even if realloc grew cap */
    if (n >= cap) { cap = n + 1; arr = (char **)realloc(arr, cap * sizeof(char *)); }
    arr[n] = NULL;
    *out_n = n;
    return arr;
}

/* ── atomic memory-file rewrite (MemoryStore._write_file) ───────────────── */
int learning_mutations_write_memory_file(const char *path, char **chunks, int n)
{
    /* build joined content with tmpfile + rename for atomicity */
    size_t cap = 256, len = 0;
    char *buf = (char *)malloc(cap);
    for (int i = 0; i < n; i++) {
        const char *c = chunks[i] ? chunks[i] : "";
        /* strip trailing whitespace per entry (matches [c.strip() ...]) */
        size_t L = strlen(c);
        while (L > 0 && (c[L-1] == ' ' || c[L-1] == '\t' || c[L-1] == '\r' || c[L-1] == '\n')) L--;
        size_t need = len + L + (i ? strlen(LM_ENTRY_DELIM) : 0) + 1;
        if (need > cap) { while (need > cap) cap *= 2; buf = (char *)realloc(buf, cap); }
        if (i) { memcpy(buf + len, LM_ENTRY_DELIM, strlen(LM_ENTRY_DELIM)); len += strlen(LM_ENTRY_DELIM); }
        memcpy(buf + len, c, L); len += L;
    }
    buf[len] = '\0';

    char tmp[LM_MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(buf); return -1; }
    size_t w = fwrite(buf, 1, len, f);
    int closed = fclose(f);
    free(buf);
    if (w != (size_t)len || closed != 0) { unlink(tmp); return -1; }
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

/* PoP: learning_mutations_parse_node_kind @ agent/learning_mutations.py:parse_node_kind */
/* Python: return "memory" if node_id.startswith("memory:") else "skill".
 * Returns a malloc'd string; caller frees. */
char *learning_mutations_parse_node_kind(const char *node_id)
{
    if (node_id && strncmp(node_id, "memory:", 7) == 0) return strdup("memory");
    return strdup("skill");
}

/* Internal convenience: true when the node kind is "memory". Frees the
 * kind string. */
static int learning_mutations_parse_node_kind_ismem(const char *node_id)
{
    char *kind = learning_mutations_parse_node_kind(node_id);
    int is_mem = kind && strcmp(kind, "memory") == 0;
    free(kind);
    return is_mem;
}

/* PoP: learning_mutations_parse_memory_id @ agent/learning_mutations.py:_parse_memory_id */
int learning_mutations_parse_memory_id(const char *node_id, char *out_source,
                                       int *out_gidx, char *err, size_t errsz)
{
    if (err) err[0] = '\0';
    if (!node_id) { if (err) snprintf(err, errsz, "bad memory node id: (null)"); return -1; }
    /* memory:<source>:<index> */
    char buf[LM_MAX_FIELD];
    snprintf(buf, sizeof(buf), "%s", node_id);
    char *parts[3];
    int cnt = 0;
    char *p = buf;
    char *save = NULL;
    char *tok = strtok_r(p, ":", &save);
    while (tok && cnt < 3) { parts[cnt++] = tok; tok = strtok_r(NULL, ":", &save); }
    if (cnt != 3 || strcmp(parts[0], "memory") != 0 ||
        (strcmp(parts[1], "memory") != 0 && strcmp(parts[1], "profile") != 0)) {
        if (err) snprintf(err, errsz, "bad memory node id: '%s'", node_id);
        return -1;
    }
    char *end = NULL;
    long idx = strtol(parts[2], &end, 10);
    if (end == parts[2] || *end != '\0') {
        if (err) snprintf(err, errsz, "bad memory node id: '%s'", node_id);
        return -1;
    }
    if (out_source) snprintf(out_source, LM_MAX_FIELD, "%s", parts[1]);
    if (out_gidx) *out_gidx = (int)idx;
    return 0;
}

/* PoP: learning_mutations_memory_local_index @ agent/learning_mutations.py:_memory_local_index */
/* local index: which entry within the source's own file */
static int lm_memory_local_index(const char *hermes_home, const char *source, int gidx)
{
    char *cards_json = learning_graph_memory_cards(hermes_home);
    json_t *cards = json_parse(cards_json ? cards_json : "[]", NULL);
    free(cards_json);
    if (!cards || cards->type != JSON_ARRAY) { if (cards) json_free(cards); return -1; }
    int n = (int)json_array_size(cards);
    if (!(0 <= gidx && gidx < n)) { json_free(cards); return -1; }
    json_t *card = json_array_get(cards, gidx);
    const char *csrc = card ? json_object_get_string(card, "source", "") : "";
    if (strcmp(csrc, source) != 0) { json_free(cards); return -2; } /* stale */
    if (strcmp(source, "memory") == 0) { json_free(cards); return gidx; }
    int mem_count = 0;
    for (int i = 0; i < n; i++) {
        json_t *c = json_array_get(cards, i);
        const char *s = c ? json_object_get_string(c, "source", "") : "";
        if (strcmp(s, "memory") == 0) mem_count++;
    }
    json_free(cards);
    return gidx - mem_count;
}

/* PoP: learning_mutations_locate_memory @ agent/learning_mutations.py:_locate_memory */
/* PoP: learning_mutations_memories_dir @ agent/learning_mutations.py:_memories_dir */
/* resolve memory card to (path, all entries, local index) */
static int lm_locate_memory(const char *hermes_home, const char *source, int gidx,
                            char *out_path, char ***out_chunks, int *out_local)
{
    char memdir[LM_MAX_PATH];
    snprintf(memdir, sizeof(memdir), "%s/memories", hermes_home);
    const char *fname = (strcmp(source, "profile") == 0) ? "USER.md" : "MEMORY.md";
    char path[LM_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", memdir, fname);

    struct stat st;
    if (stat(path, &st) != 0) return -1;

    int local = lm_memory_local_index(hermes_home, source, gidx);
    if (local < 0) return -2;

    int n = 0;
    char **chunks = learning_mutations_read_memory_file(path, &n);
    if (!chunks || !(0 <= local && local < n)) {
        if (chunks) { for (int i = 0; i < n; i++) free(chunks[i]); free(chunks); }
        return -2;
    }
    if (out_path) snprintf(out_path, LM_MAX_PATH, "%s", path);
    if (out_chunks) *out_chunks = chunks;
    if (out_local) *out_local = local;
    return 0;
}

/* PoP: learning_mutations_node_detail_impl @ agent/learning_mutations.py:_node_detail */
static char *lm_node_detail(const char *node_id)
{
    char home[LM_MAX_PATH];
    lm_hermes_home(home, sizeof(home));

    if (learning_mutations_parse_node_kind_ismem(node_id)) {
        char source[LM_MAX_FIELD]; int gidx; char err[LM_MAX_FIELD];
        if (learning_mutations_parse_memory_id(node_id, source, &gidx, err, sizeof(err)) != 0)
            return NULL;
        char path[LM_MAX_PATH]; char **chunks; int local;
        if (lm_locate_memory(home, source, gidx, path, &chunks, &local) != 0)
            return NULL;
        const char *body = chunks[local];
        /* Python _node_detail does body.strip() */
        while (*body == ' ' || *body == '\t' || *body == '\r' || *body == '\n') body++;
        size_t bl = strlen(body);
        while (bl > 0 && (body[bl-1] == ' ' || body[bl-1] == '\t' || body[bl-1] == '\r' || body[bl-1] == '\n')) bl--;
        char *body_copy = strndup(body, bl);
        /* label = first line, truncated to 80 */
        const char *nl = strchr(body_copy, '\n');
        size_t llen = nl ? (size_t)(nl - body_copy) : strlen(body_copy);
        if (llen > 80) llen = 80;
        char label[96];
        memcpy(label, body_copy, llen); label[llen] = '\0';

        json_t *r = json_new_object();
        json_object_set(r, "ok", json_bool(1));
        json_object_set(r, "kind", json_new_string("memory"));
        json_object_set(r, "id", json_new_string(node_id));
        json_object_set(r, "label", json_new_string(label));
        json_object_set(r, "content", json_new_string(body_copy));
        char *dump = json_dumps(r, 0);
        char *ret = strdup(dump ? dump : "{}");
        free(dump); json_free(r);
        free(body_copy);
        for (int i = 0; chunks[i]; i++) free(chunks[i]);
        free(chunks);
        return ret;
    }

    /* skill path */
    char skdir[LM_MAX_PATH];
    snprintf(skdir, sizeof(skdir), "%s/skills", home);
    char skpath[LM_MAX_PATH];
    int found = 0;
    {
        DIR *d = opendir(skdir);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
                char full[LM_MAX_PATH];
                snprintf(full, sizeof(full), "%s/%s", skdir, e->d_name);
                struct stat st;
                if (stat(full, &st) == 0 && S_ISDIR(st.st_mode) && !strcmp(e->d_name, node_id)) {
                    snprintf(skpath, sizeof(skpath), "%s/SKILL.md", full);
                    found = 1; break;
                }
            }
            closedir(d);
        }
    }
    if (!found) return NULL;
    FILE *f = fopen(skpath, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long fsz = ftell(f); rewind(f);
    char *md = (char *)malloc(fsz + 1);
    size_t nr = fread(md, 1, fsz, f); md[nr] = '\0'; fclose(f);

    json_t *r = json_new_object();
    json_object_set(r, "ok", json_bool(1));
    json_object_set(r, "kind", json_new_string("skill"));
    json_object_set(r, "id", json_new_string(node_id));
    json_object_set(r, "label", json_new_string(node_id));
    json_object_set(r, "content", json_new_string(md));
    char *dump = json_dumps(r, 0);
    char *ret = strdup(dump ? dump : "{}");
    free(dump); json_free(r); free(md);
    return ret;
}

/* PoP: learning_mutations_node_detail @ agent/learning_mutations.py:node_detail */
char *learning_mutations_node_detail(const char *node_id)
{
    char *detail = lm_node_detail(node_id);
    if (detail) return detail;
    json_t *r = json_new_object();
    json_object_set(r, "ok", json_bool(0));
    json_object_set(r, "message", json_new_string("not found"));
    char *dump = json_dumps(r, 0);
    char *ret = strdup(dump ? dump : "{}");
    free(dump); json_free(r);
    return ret;
}

/* PoP: learning_mutations_delete_skill @ agent/learning_mutations.py:_delete_skill */
/* PoP: learning_mutations_delete_memory @ agent/learning_mutations.py:_delete_memory */
/* PoP: learning_mutations_delete_node @ agent/learning_mutations.py:delete_node */
char *learning_mutations_delete_node(const char *node_id)
{
    char home[LM_MAX_PATH];
    lm_hermes_home(home, sizeof(home));
    json_t *r = json_new_object();

    if (learning_mutations_parse_node_kind_ismem(node_id)) {
        char source[LM_MAX_FIELD]; int gidx; char err[LM_MAX_FIELD];
        if (learning_mutations_parse_memory_id(node_id, source, &gidx, err, sizeof(err)) != 0) {
            json_object_set(r, "ok", json_bool(0));
            json_object_set(r, "message", json_new_string(err));
            goto out;
        }
        char path[LM_MAX_PATH]; char **chunks; int local;
        if (lm_locate_memory(home, source, gidx, path, &chunks, &local) != 0) {
            json_object_set(r, "ok", json_bool(0));
            json_object_set(r, "message", json_new_string("memory node id is stale — refresh the graph"));
            goto out;
        }
        /* delete entry at local */
        int n = 0;
        while (chunks[n]) n++;
        char **out = (char **)calloc(n, sizeof(char *));
        int on = 0;
        for (int i = 0; i < n; i++) {
            if (i == local) { free(chunks[i]); continue; }
            out[on++] = chunks[i];
        }
        int wrc = learning_mutations_write_memory_file(path, out, on);
        for (int i = 0; i < on; i++) free(out[i]);
        free(out); free(chunks);
        if (wrc != 0) {
            json_object_set(r, "ok", json_bool(0));
            json_object_set(r, "message", json_new_string("failed to write memory file"));
            goto out;
        }
        json_object_set(r, "ok", json_bool(1));
        json_object_set(r, "message", json_new_string("deleted memory"));
        goto out;
    }

    /* delete skill: archive if not pinned */
    skill_usage_map_t map;
    skill_usage_load(home, &map);
    skill_usage_record_t rec;
    skill_usage_get_record(&map, node_id, &rec);
    if (rec.pinned) {
        char msg[LM_MAX_FIELD];
        snprintf(msg, sizeof(msg), "'%s' is pinned — unpin it first (hermes curator unpin %s)", node_id, node_id);
        json_object_set(r, "ok", json_bool(0));
        json_object_set(r, "message", json_new_string(msg));
        goto out;
    }
    char msg[SKILL_USAGE_MAX_VALUE];
    int rc = skill_usage_archive(home, node_id, msg);
    if (rc == 0) clear_skills_system_prompt_cache(home, 1);
    json_object_set(r, "ok", json_bool(rc == 0 ? 1 : 0));
    if (rc == 0) {
        char m[LM_MAX_FIELD];
        snprintf(m, sizeof(m), "archived '%s' — restore with: hermes curator restore %s", node_id, node_id);
        json_object_set(r, "message", json_new_string(m));
    } else {
        json_object_set(r, "message", json_new_string(msg));
    }
out:
    char *dump = json_dumps(r, 0);
    char *ret = strdup(dump ? dump : "{}");
    free(dump); json_free(r);
    return ret;
}

/* PoP: learning_mutations_edit_skill @ agent/learning_mutations.py:_edit_skill */
/* PoP: learning_mutations_edit_memory @ agent/learning_mutations.py:_edit_memory */
/* PoP: learning_mutations_edit_node @ agent/learning_mutations.py:edit_node */
char *learning_mutations_edit_node(const char *node_id, const char *content)
{
    char home[LM_MAX_PATH];
    lm_hermes_home(home, sizeof(home));
    json_t *r = json_new_object();

    if (learning_mutations_parse_node_kind_ismem(node_id)) {
        char source[LM_MAX_FIELD]; int gidx; char err[LM_MAX_FIELD];
        if (learning_mutations_parse_memory_id(node_id, source, &gidx, err, sizeof(err)) != 0) {
            json_object_set(r, "ok", json_bool(0));
            json_object_set(r, "message", json_new_string(err));
            goto edit_out;
        }
        char *body = content ? strdup(content) : strdup("");
        /* strip */
        char *p = body;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        size_t L = strlen(p);
        while (L > 0 && (p[L-1] == ' ' || p[L-1] == '\t' || p[L-1] == '\r' || p[L-1] == '\n')) p[--L] = '\0';
        if (!*p) {
            free(body);
            json_object_set(r, "ok", json_bool(0));
            json_object_set(r, "message", json_new_string("empty memory — use delete to remove it"));
            goto edit_out;
        }
        char path[LM_MAX_PATH]; char **chunks; int local;
        if (lm_locate_memory(home, source, gidx, path, &chunks, &local) != 0) {
            free(body);
            json_object_set(r, "ok", json_bool(0));
            json_object_set(r, "message", json_new_string("memory node id is stale — refresh the graph"));
            goto edit_out;
        }
        int n = 0; while (chunks[n]) n++;
        /* replace entry at local with stripped body (reuse body pointer) */
        free(chunks[local]);
        chunks[local] = body; /* body now stripped */
        int wrc = learning_mutations_write_memory_file(path, chunks, n);
        for (int i = 0; i < n; i++) free(chunks[i]);
        free(chunks);
        if (wrc != 0) {
            json_object_set(r, "ok", json_bool(0));
            json_object_set(r, "message", json_new_string("failed to write memory file"));
            goto edit_out;
        }
        json_object_set(r, "ok", json_bool(1));
        json_object_set(r, "message", json_new_string("updated memory"));
        goto edit_out;
    }

    /* edit skill: rewrite SKILL.md */
    char skdir[LM_MAX_PATH];
    snprintf(skdir, sizeof(skdir), "%s/skills", home);
    char skpath[LM_MAX_PATH];
    int found = 0;
    {
        DIR *d = opendir(skdir);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
                char full[LM_MAX_PATH];
                snprintf(full, sizeof(full), "%s/%s", skdir, e->d_name);
                struct stat st;
                if (stat(full, &st) == 0 && S_ISDIR(st.st_mode) && !strcmp(e->d_name, node_id)) {
                    snprintf(skpath, sizeof(skpath), "%s/SKILL.md", full);
                    found = 1; break;
                }
            }
            closedir(d);
        }
    }
    if (!found) {
        json_object_set(r, "ok", json_bool(0));
        json_object_set(r, "message", json_new_string("skill not found"));
        goto edit_out;
    }
    /* Faithful to tools/skill_manager_tool._edit_skill: validate frontmatter
     * (name + description required) before writing. */
    char *verr = lm_validate_frontmatter(content);
    if (verr) {
        json_object_set(r, "ok", json_bool(0));
        json_object_set(r, "message", json_new_string(verr));
        free(verr);
        goto edit_out;
    }
    {
        FILE *f = fopen(skpath, "wb");
        if (!f) {
            json_object_set(r, "ok", json_bool(0));
            json_object_set(r, "message", json_new_string("cannot write SKILL.md"));
            goto edit_out;
        }
        size_t clen = content ? strlen(content) : 0;
        size_t w = fwrite(content ? content : "", 1, clen, f);
        int wrc = (w == clen) && (fclose(f) == 0) ? 0 : -1;
        if (wrc != 0) {
            json_object_set(r, "ok", json_bool(0));
            json_object_set(r, "message", json_new_string("failed to write SKILL.md"));
            goto edit_out;
        }
    }
    /* PoP: learning_mutations_clear_skill_cache @ agent/learning_mutations.py:_clear_skill_cache */
    clear_skills_system_prompt_cache(home, 1);
    json_object_set(r, "ok", json_bool(1));
    json_object_set(r, "message", json_new_string("updated skill"));

edit_out:
    char *dump = json_dumps(r, 0);
    char *ret = strdup(dump ? dump : "{}");
    free(dump); json_free(r);
    return ret;
}
