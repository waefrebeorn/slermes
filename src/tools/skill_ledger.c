/*
 * skill_ledger.c — Per-mutation skill audit ledger + single-edit rollback.
 * Port of Python tools/skill_ledger.py (388 lines).
 *
 * Every public symbol is a faithful 1:1 port of the Python function with the
 * same name (name-parity mandate). The ledger is TELEMETRY, NOT A GATE:
 * a ledger failure must never block the mutation it describes. The one
 * deliberate exception is rollback_entry, which FAILS CLOSED when its own
 * pre-rollback safety capture fails.
 */

#include "skill_ledger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "hash.h"            /* hash_sha256_hex */
#include "hermes_json.h"     /* libjson shim */
#include "port_web_server_paths.h" /* get_hermes_home */

/* ── Actor override (mirrors Python contextvars) ─────────────────────────── */
/* Thread-local token stack is overkill for a single var; use a simple
 * thread-local override + an int token that indexes a depth ring. We keep a
 * small ring so reset_ledger_actor is balanced, mirroring Token.reset. */

#define LEDGER_ACTOR_RING 64

static __thread const char *g_actor_override = NULL;
static __thread const char *g_actor_ring[LEDGER_ACTOR_RING];
static __thread int         g_actor_depth = 0;

static bool valid_actor(const char *a) {
    return a && (strcmp(a, SKILL_LEDGER_ACTOR_CURATOR) == 0 ||
                 strcmp(a, SKILL_LEDGER_ACTOR_AGENT) == 0 ||
                 strcmp(a, SKILL_LEDGER_ACTOR_USER) == 0);
}

int set_ledger_actor(const char *actor) {
    int token = g_actor_depth;
    if (token >= LEDGER_ACTOR_RING) token = 0; /* wrap; parity to ring semantics */
    g_actor_ring[token] = g_actor_override;
    g_actor_override = actor;
    g_actor_depth = (g_actor_depth + 1) % LEDGER_ACTOR_RING;
    return token;
}

void reset_ledger_actor(int token) {
    if (token < 0 || token >= LEDGER_ACTOR_RING) return;
    g_actor_override = g_actor_ring[token];
}

const char *derive_actor(void) {
    if (valid_actor(g_actor_override))
        return g_actor_override;
    /* Python: is_background_review() -> curator. We have no equivalent
     * provenance signal in the C port, so fall through to agent. */
    return SKILL_LEDGER_ACTOR_AGENT;
}

/* ── Paths + config gate ─────────────────────────────────────────────────── */

const char *ledger_path(void) {
    static char buf[4096];
    if (buf[0]) return buf;
    snprintf(buf, sizeof(buf), "%s/skills/.curator_ledger.jsonl", get_hermes_home());
    return buf;
}

const char *blobs_dir(void) {
    static char buf[4096];
    if (buf[0]) return buf;
    snprintf(buf, sizeof(buf), "%s/.curator_backups/blobs", get_hermes_home());
    return buf;
}

bool ledger_enabled(void) {
    /* Config gate skills.ledger (default True). Lazy: we attempt to read the
     * config YAML at HERMES_HOME/config.yaml; on any failure default ON. */
    const char *home = get_hermes_home();
    char cfg[4096];
    snprintf(cfg, sizeof(cfg), "%s/config.yaml", home);
    FILE *f = fopen(cfg, "r");
    if (!f) return true;
    bool enabled = true;
    char line[1024];
    bool in_skills = false;
    while (fgets(line, sizeof(line), f)) {
        /* crude YAML: skills: then ledger: <bool> */
        if (!in_skills) {
            if (strncmp(line, "skills:", 7) == 0) in_skills = true;
            continue;
        }
        if (line[0] == ' ' || line[0] == '\t') {
            if (strstr(line, "ledger:")) {
                char *v = strstr(line, ":");
                v += 1;
                while (*v == ' ' || *v == '\t') v++;
                if (strncmp(v, "false", 5) == 0) enabled = false;
                else if (strncmp(v, "true", 4) == 0) enabled = true;
                break;
            }
        } else {
            /* left-shifted key ends the skills block */
            break;
        }
    }
    fclose(f);
    return enabled;
}

/* ── Blob store (content-addressed, deduped) ─────────────────────────────── */

/* PoP: store_blob @ tools/skill_ledger.py:_store_blob */
char *store_blob(const unsigned char *data, size_t len) {
    char *digest = hash_sha256_hex(data, len);
    if (!digest) return NULL;
    char dest[4608];
    snprintf(dest, sizeof(dest), "%s/%s", blobs_dir(), digest);
    if (access(dest, F_OK) == 0)
        return digest; /* deduped: leave alone */
    mkdir(blobs_dir(), 0700);
    /* ensure parent (.curator_backups) exists too */
    {
        char parent[4608];
        snprintf(parent, sizeof(parent), "%s/.curator_backups", get_hermes_home());
        mkdir(parent, 0700);
    }
    /* atomic temp + rename */
    char tmp[4608];
    snprintf(tmp, sizeof(tmp), "%s/.tmp-%08lx-%s", blobs_dir(),
             (unsigned long)rand(), digest);
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(digest); return NULL; }
    if (fwrite(data, 1, len, f) != len) { fclose(f); remove(tmp); free(digest); return NULL; }
    fclose(f);
    if (rename(tmp, dest) != 0) { remove(tmp); free(digest); return NULL; }
    return digest;
}

unsigned char *read_blob(const char *sha256, size_t *out_len) {
    if (out_len) *out_len = 0;
    if (!sha256 || sha256[0] == '\0') return NULL;
    for (const char *p = sha256; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f')))
            return NULL;
    char path[4608];
    snprintf(path, sizeof(path), "%s/%s", blobs_dir(), sha256);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc((size_t)sz ? (size_t)sz : 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return NULL; }
    fclose(f);
    if (out_len) *out_len = (size_t)sz;
    return buf;
}

/* ── manifest helpers ────────────────────────────────────────────────────── */

void skill_ledger_manifest_free(skill_ledger_manifest_t *m) {
    if (!m) return;
    skill_ledger_manifest_free_arrays(m);
    free(m);
}

/* Free only the inner path/sha arrays. For manifests embedded inline inside
 * another struct (e.g. skill_ledger_entry_t.before/after), call this instead
 * of skill_ledger_manifest_free so we don't free the owning struct. */
void skill_ledger_manifest_free_arrays(skill_ledger_manifest_t *m) {
    if (!m) return;
    for (size_t i = 0; i < m->count; i++) {
        free(m->paths[i]);
        free(m->sha256s[i]);
    }
    free(m->paths);
    free(m->sha256s);
    m->paths = NULL;
    m->sha256s = NULL;
    m->count = 0;
}

static skill_ledger_manifest_t *manifest_new(void) {
    skill_ledger_manifest_t *m = calloc(1, sizeof(*m));
    return m;
}

static void manifest_push(skill_ledger_manifest_t *m, const char *path, const char *sha) {
    m->paths = realloc(m->paths, (m->count + 1) * sizeof(char *));
    m->sha256s = realloc(m->sha256s, (m->count + 1) * sizeof(char *));
    m->paths[m->count] = strdup(path);
    m->sha256s[m->count] = strdup(sha);
    m->count++;
}

#define MANIFEST_FIELD_DELIM '\x1f'   /* UNIT SEPARATOR: path can't contain it */
#define MANIFEST_REC_DELIM   '\x1e'   /* RECORD SEPARATOR: between records */

/* Compact manifest <-> delimited string for append_entry's *_json args.
 * We store manifests inline as "\x1e"-joined "path\x1fsha" records, which is
 * lossless and trivially reversible (mirrors Python's list of {path,sha256}).
 * No embedded NULs (would break C strings / JSON). */

static char *manifest_to_json(const skill_ledger_manifest_t *m) {
    if (!m || m->count == 0) return strdup("");
    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    out[0] = '\0';
    for (size_t i = 0; i < m->count; i++) {
        const char *p = m->paths[i] ? m->paths[i] : "";
        const char *s = m->sha256s[i] ? m->sha256s[i] : "";
        size_t rec = strlen(p) + 1 + strlen(s) + 1;
        if (len + rec + 1 > cap) { cap = (len + rec + 1) * 2; out = realloc(out, cap); }
        int n = snprintf(out + len, cap - len, "%s%c%s%c",
                         p, MANIFEST_FIELD_DELIM, s,
                         (i + 1 < m->count) ? MANIFEST_REC_DELIM : '\n');
        /* we use '\n' as a harmless terminator instead of NUL so the string
         * is a normal C string; manifest_from_json stops at record delim. */
        len += (size_t)n;
    }
    return out;
}

static skill_ledger_manifest_t *manifest_from_json(const char *s) {
    skill_ledger_manifest_t *m = manifest_new();
    if (!s || !*s) return m;
    const char *p = s;
    while (*p) {
        const char *d1 = strchr(p, MANIFEST_FIELD_DELIM);
        if (!d1) break;
        size_t plen = (size_t)(d1 - p);
        const char *q = d1 + 1;
        /* The sha field ends at the record separator (\x1e) or the final
         * '\n' terminator — NOT at the next field delimiter (\x1f), which
         * belongs to the following record. */
        const char *d2 = strchr(q, MANIFEST_REC_DELIM);
        if (!d2) d2 = strchr(q, '\n');
        if (!d2) break;
        size_t slen = (size_t)(d2 - q);
        char *path = malloc(plen + 1); memcpy(path, p, plen); path[plen] = '\0';
        char *sha = malloc(slen + 1); memcpy(sha, q, slen); sha[slen] = '\0';
        manifest_push(m, path, sha);
        free(path); free(sha);
        p = d2 + 1;
    }
    return m;
}

skill_ledger_manifest_t *snapshot_paths(const char *root) {
    skill_ledger_manifest_t *out = manifest_new();
    if (!root) return out;
    struct stat st;
    if (stat(root, &st) != 0) return out;
    if (S_ISREG(st.st_mode)) {
        FILE *f = fopen(root, "rb");
        if (!f) return out;
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        unsigned char *data = malloc(sz > 0 ? (size_t)sz : 1);
        if (fread(data, 1, sz > 0 ? (size_t)sz : 1, f) == (size_t)(sz > 0 ? sz : 1)) {
            char *sha = store_blob(data, sz > 0 ? (size_t)sz : 0);
            if (sha) { manifest_push(out, root, sha); free(sha); }
        }
        free(data); fclose(f);
        return out;
    }
    if (!S_ISDIR(st.st_mode)) return out;
    /* recursive rglob, sorted for determinism (mirrors sorted(...)) */
    /* simple non-recursive-sorted walk via nftw-like recursion */
    char *stack[4096]; int sp = 0;
    stack[sp++] = strdup(root);
    /* collect files first */
    char **files = NULL; size_t fc = 0;
    while (sp > 0) {
        char *dir = stack[--sp];
        DIR *d = opendir(dir);
        if (!d) { free(dir); continue; }
        struct dirent *de;
        while ((de = readdir(d))) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
            char full[4608];
            snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
            struct stat ds;
            if (stat(full, &ds) != 0) continue;
            if (S_ISDIR(ds.st_mode)) {
                if (sp < 4096) stack[sp++] = strdup(full);
            } else if (S_ISREG(ds.st_mode)) {
                files = realloc(files, (fc + 1) * sizeof(char *));
                files[fc++] = strdup(full);
            }
        }
        closedir(d);
        free(dir);
    }
    /* sort */
    for (size_t i = 0; i + 1 < fc; i++)
        for (size_t j = i + 1; j < fc; j++)
            if (strcmp(files[i], files[j]) > 0) { char *t = files[i]; files[i] = files[j]; files[j] = t; }
    for (size_t i = 0; i < fc; i++) {
        FILE *f = fopen(files[i], "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            unsigned char *data = malloc(sz > 0 ? (size_t)sz : 1);
            if (fread(data, 1, sz > 0 ? (size_t)sz : 1, f) == (size_t)(sz > 0 ? sz : 1)) {
                char *sha = store_blob(data, sz > 0 ? (size_t)sz : 0);
                if (sha) { manifest_push(out, files[i], sha); free(sha); }
            }
            free(data); fclose(f);
        }
        free(files[i]);
    }
    free(files);
    return out;
}

/* ── Append + read ────────────────────────────────────────────────────────── */

/* Mirror of Python append_entry. before/after may be NULL (use before_json/
 * after_json, which carry the delimited manifest). */
char *append_entry(const char *action, const char *skill,
                   const skill_ledger_manifest_t *before, const char *before_json,
                   const skill_ledger_manifest_t *after, const char *after_json,
                   const char *actor, const char *evidence_json) {
    if (!ledger_enabled()) return NULL;
    const char *real_actor = (actor && valid_actor(actor)) ? actor : derive_actor();
    char id[32];
    snprintf(id, sizeof(id), "%08lx", (unsigned long)rand() & 0xfffffff);
    /* 12-hex id */
    char id12[16];
    snprintf(id12, sizeof(id12), "%08lx%04lx", (unsigned long)rand() & 0xffffff,
             (unsigned long)rand() & 0xffff);

    char ts[64];
    time_t now = time(NULL);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);

    char *b_json = before ? manifest_to_json(before)
                          : (before_json ? strdup(before_json) : strdup(""));
    char *a_json = after ? manifest_to_json(after)
                         : (after_json ? strdup(after_json) : strdup(""));
    const char *ev = evidence_json ? evidence_json : "{}";

    json_t *entry = json_object();
    json_set(entry, "id", json_string(id12));
    json_set(entry, "ts", json_string(ts));
    json_set(entry, "actor", json_string(real_actor));
    json_set(entry, "action", json_string(action ? action : ""));
    json_set(entry, "skill", json_string(skill ? skill : ""));
    json_t *evobj = json_parse(ev, NULL);
    if (!evobj) evobj = json_object();
    json_set(entry, "evidence", evobj);
    /* before/after stored as delimited strings to keep the JSONL line flat */
    json_set(entry, "before", json_string(b_json));
    json_set(entry, "after", json_string(a_json));

    char *line = json_serialize(entry);
    json_free(entry);

    const char *lp = ledger_path();
    char d[4608];
    snprintf(d, sizeof(d), "%s", lp);
    char *slash = strrchr(d, '/');
    if (slash) { *slash = '\0'; mkdir(d, 0700); }
    FILE *f = fopen(lp, "a");
    if (!f) { free(line); free(b_json); free(a_json); return NULL; }
    fprintf(f, "%s\n", line);
    fclose(f);
    free(line); free(b_json); free(a_json);
    return strdup(id12);
}

char *record_mutation(const char *action, const char *skill,
                      const char *before_root, const char *before_json,
                      const char *after_root, const char *actor,
                      const char *evidence_json) {
    if (!ledger_enabled()) return NULL;
    skill_ledger_manifest_t *before = NULL;
    if (before_json && *before_json) {
        before = manifest_from_json(before_json);
    } else if (before_root) {
        before = snapshot_paths(before_root);
    }
    skill_ledger_manifest_t *after = after_root ? snapshot_paths(after_root) : manifest_new();
    char *id = append_entry(action, skill, before, before_json, after, NULL,
                            actor, evidence_json);
    skill_ledger_manifest_free(before);
    skill_ledger_manifest_free(after);
    return id;
}

skill_ledger_manifest_t *capture_before(const char *root) {
    if (!ledger_enabled()) return NULL;
    return snapshot_paths(root);
}

static skill_ledger_entry_t *entry_from_json(json_t *row) {
    skill_ledger_entry_t *e = calloc(1, sizeof(*e));
    e->id = strdup(json_get_str(row, "id", ""));
    e->ts = strdup(json_get_str(row, "ts", ""));
    e->actor = strdup(json_get_str(row, "actor", ""));
    e->action = strdup(json_get_str(row, "action", ""));
    e->skill = strdup(json_get_str(row, "skill", ""));
    json_t *ev = json_obj_get(row, "evidence");
    e->evidence_json = ev ? json_serialize(ev) : strdup("{}");
    skill_ledger_manifest_t *b = manifest_from_json(json_get_str(row, "before", ""));
    skill_ledger_manifest_t *a = manifest_from_json(json_get_str(row, "after", ""));
    e->before = *b;   /* transfer ownership of inner arrays */
    e->after = *a;
    free(b); free(a);  /* free the wrappers only; arrays now owned by e */
    return e;
}

skill_ledger_entry_t **list_entries(const char *skill, int limit, size_t *out_count) {
    if (out_count) *out_count = 0;
    const char *lp = ledger_path();
    FILE *f = fopen(lp, "r");
    if (!f) return NULL;
    skill_ledger_entry_t **rows = NULL;
    size_t rc = 0;
    {
        FILE *ff = fopen(lp, "r");
        if (ff) {
            char buf[8192];
            while (fgets(buf, sizeof(buf), ff)) {
                size_t bl = strlen(buf);
                if (bl && buf[bl-1] == '\n') buf[--bl] = '\0';
                if (bl && buf[bl-1] == '\r') buf[--bl] = '\0';
                if (bl == 0) continue;
                json_t *row = json_parse(buf, NULL);
                if (!row || row->type != JSON_OBJECT) { if (row) json_free(row); continue; }
                skill_ledger_entry_t *e = entry_from_json(row);
                json_free(row);
                if (skill && *skill && strcmp(e->skill, skill) != 0) {
                    skill_ledger_entry_free(e);
                    continue;
                }
                rows = realloc(rows, (rc + 1) * sizeof(*rows));
                rows[rc++] = e;
            }
            fclose(ff);
        }
    }
    /* newest first (reverse) */
    for (size_t i = 0; i < rc / 2; i++) {
        skill_ledger_entry_t *t = rows[i];
        rows[i] = rows[rc - 1 - i];
        rows[rc - 1 - i] = t;
    }
    if (limit >= 0 && (size_t)limit < rc) {
        for (size_t i = limit; i < rc; i++) skill_ledger_entry_free(rows[i]);
        rc = (size_t)limit;
        rows = realloc(rows, (rc + 1) * sizeof(*rows));
    }
    if (out_count) *out_count = rc;
    return rows;
}

skill_ledger_entry_t *get_entry(const char *entry_id) {
    if (!entry_id || !*entry_id) return NULL;
    size_t n = 0;
    skill_ledger_entry_t **rows = list_entries(NULL, -1, &n);
    if (!rows) return NULL;
    skill_ledger_entry_t *found = NULL;
    for (size_t i = 0; i < n; i++) {
        if (strcmp(rows[i]->id, entry_id) == 0) {
            found = rows[i];
            rows[i] = NULL; /* detach */
            break;
        }
    }
    skill_ledger_entries_free(rows, n);
    return found;
}

/* ── Single-edit rollback ─────────────────────────────────────────────────── */

/* True when path sits under root (handles ".." traversal). */
/* PoP: is_within @ tools/skill_ledger.py:_is_within */
static bool is_within(const char *root, const char *path) {
    char rcopy[4608], pcopy[4608];
    snprintf(rcopy, sizeof(rcopy), "%s", root);
    snprintf(pcopy, sizeof(pcopy), "%s", path);
    /* normpath: collapse repeated slashes + resolve ".." minimally */
    char rn[4608], pn[4608];
    {
        char *o = rn; const char *s = rcopy;
        bool abs = (*s == '/');
        char seg[4608]; int depth = 0; char *st[1024];
        /* tokenize */
        while (*s) {
            while (*s == '/') s++;
            if (!*s) break;
            char *q = seg;
            while (*s && *s != '/') *q++ = *s++;
            *q = '\0';
            if (strcmp(seg, ".") == 0) continue;
            if (strcmp(seg, "..") == 0) { if (depth > 0) depth--; continue; }
            st[depth++] = strdup(seg);
        }
        o = rn; if (abs) *o++ = '/';
        for (int i = 0; i < depth; i++) {
            if (i) *o++ = '/';
            memcpy(o, st[i], strlen(st[i])); o += strlen(st[i]);
        }
        if (depth == 0 && !abs) { /* relative empty -> "." */ *o++ = '.'; }
        *o = '\0';
        for (int i = 0; i < depth; i++) free(st[i]);
    }
    {
        char *o = pn; const char *s = pcopy;
        bool abs = (*s == '/');
        char seg[4608]; int depth = 0; char *st[1024];
        while (*s) {
            while (*s == '/') s++;
            if (!*s) break;
            char *q = seg;
            while (*s && *s != '/') *q++ = *s++;
            *q = '\0';
            if (strcmp(seg, ".") == 0) continue;
            if (strcmp(seg, "..") == 0) { if (depth > 0) depth--; continue; }
            st[depth++] = strdup(seg);
        }
        o = pn; if (abs) *o++ = '/';
        for (int i = 0; i < depth; i++) {
            if (i) *o++ = '/';
            memcpy(o, st[i], strlen(st[i])); o += strlen(st[i]);
        }
        if (depth == 0 && !abs) { *o++ = '.'; }
        *o = '\0';
        for (int i = 0; i < depth; i++) free(st[i]);
    }
    /* path == root, or root is a parent of path */
    if (strcmp(pn, rn) == 0) return true;
    size_t rl = strlen(rn);
    return strncmp(pn, rn, rl) == 0 && pn[rl] == '/';
}

/* Fail-closed validation: all paths must live under HERMES_HOME. */
/* PoP: validate_entry_paths @ tools/skill_ledger.py:_validate_entry_paths */
static char *validate_entry_paths(const skill_ledger_entry_t *entry) {
    const char *home = get_hermes_home();
    const skill_ledger_manifest_t *secs[2] = { &entry->before, &entry->after };
    for (int s = 0; s < 2; s++) {
        for (size_t i = 0; i < secs[s]->count; i++) {
            if (!is_within(home, secs[s]->paths[i])) {
                char *msg = malloc(256);
                snprintf(msg, 256, "entry references a path outside %s: %s",
                         home, secs[s]->paths[i]);
                return msg;
            }
        }
    }
    return NULL;
}

bool rollback_entry(const char *entry_id, char *out_msg, size_t msg_len) {
    if (out_msg && msg_len) out_msg[0] = '\0';
    skill_ledger_entry_t *entry = get_entry(entry_id);
    if (!entry) {
        if (out_msg) snprintf(out_msg, msg_len, "no ledger entry with id '%s'",
                              entry_id ? entry_id : "");
        return false;
    }
    char *perr = validate_entry_paths(entry);
    if (perr) {
        if (out_msg) snprintf(out_msg, msg_len, "refusing rollback: %s", perr);
        free(perr);
        skill_ledger_entry_free(entry);
        return false;
    }
    /* Pre-check every blob we need so we never fail mid-restore. */
    for (size_t i = 0; i < entry->before.count; i++) {
        size_t bl = 0;
        unsigned char *pre = read_blob(entry->before.sha256s[i], &bl);
        if (pre == NULL) {
            if (out_msg)
                snprintf(out_msg, msg_len,
                         "missing blob %s for %s; rollback aborted, nothing was changed",
                         entry->before.sha256s[i], entry->before.paths[i]);
            skill_ledger_entry_free(entry);
            return false;
        }
        free(pre);  /* pre-check only; real read happens in the restore loop */
    }
    /* Touched paths = union of before/after. Capture CURRENT state as the
     * safety entry so the rollback itself is undoable. FAIL CLOSED. */
    char **touched = NULL; size_t tc = 0;
    for (size_t i = 0; i < entry->before.count; i++) {
        touched = realloc(touched, (tc + 1) * sizeof(char *));
        touched[tc++] = strdup(entry->before.paths[i]);
    }
    for (size_t i = 0; i < entry->after.count; i++) {
        bool dup = false;
        for (size_t j = 0; j < tc; j++)
            if (strcmp(touched[j], entry->after.paths[i]) == 0) { dup = true; break; }
        if (!dup) { touched = realloc(touched, (tc + 1) * sizeof(char *)); touched[tc++] = strdup(entry->after.paths[i]); }
    }
    skill_ledger_manifest_t *safety = manifest_new();
    for (size_t i = 0; i < tc; i++) {
        FILE *f = fopen(touched[i], "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            unsigned char *data = malloc(sz > 0 ? (size_t)sz : 1);
            if (fread(data, 1, sz > 0 ? (size_t)sz : 1, f) == (size_t)(sz > 0 ? sz : 1)) {
                char *sha = store_blob(data, sz > 0 ? (size_t)sz : 0);
                if (sha) { manifest_push(safety, touched[i], sha); free(sha); }
            }
            free(data); fclose(f);
        }
    }
    char *safety_json = manifest_to_json(safety);
    char ev[128];
    snprintf(ev, sizeof(ev), "{\"rollback_target\":\"%s\"}", entry_id);
    char *safety_id = append_entry("pre-rollback", entry->skill, NULL, safety_json,
                                   NULL, safety_json, NULL, ev);
    if (!safety_id) {
        if (out_msg)
            snprintf(out_msg, msg_len,
                     "pre-rollback safety capture failed (ledger disabled or unwritable); "
                     "rollback aborted and current skills were not changed");
        free(safety_json); skill_ledger_manifest_free(safety);
        for (size_t i = 0; i < tc; i++) free(touched[i]);
        free(touched);
        skill_ledger_entry_free(entry);
        return false;
    }
    /* Restore: write every before-file, remove files the mutation created. */
    size_t restored = 0, removed = 0;
    for (size_t i = 0; i < entry->before.count; i++) {
        size_t bl = 0;
        unsigned char *data = read_blob(entry->before.sha256s[i], &bl);
        if (!data) continue; /* already pre-checked */
        char d[4608];
        snprintf(d, sizeof(d), "%s", entry->before.paths[i]);
        char *slash = strrchr(d, '/');
        if (slash) { *slash = '\0'; mkdir(d, 0700); }
        FILE *f = fopen(entry->before.paths[i], "wb");
        if (f) { fwrite(data, 1, bl, f); fclose(f); restored++; }
        free(data);
    }
    for (size_t i = 0; i < entry->after.count; i++) {
        bool in_before = false;
        for (size_t j = 0; j < entry->before.count; j++)
            if (strcmp(entry->after.paths[i], entry->before.paths[j]) == 0) { in_before = true; break; }
        if (!in_before) {
            if (access(entry->after.paths[i], F_OK) == 0) {
                if (remove(entry->after.paths[i]) == 0) removed++;
            }
        }
    }
    char ev2[160];
    snprintf(ev2, sizeof(ev2),
             "{\"rollback_target\":\"%s\",\"restored\":%zu,\"removed\":%zu}",
             entry_id, restored, removed);
    char *before_json = manifest_to_json(&entry->before);
    char *rollback_id = append_entry("rollback", entry->skill, NULL, safety_json, NULL,
                                      before_json, NULL, ev2);
    free(before_json);
    free(rollback_id);

    if (out_msg)
        snprintf(out_msg, msg_len,
                 "rolled back entry %s (%s on '%s'): %zu file(s) restored, %zu removed. "
                 "Safety entry %s captured the pre-rollback state.",
                 entry_id, entry->action, entry->skill, restored, removed, safety_id);
    free(safety_id); free(safety_json); skill_ledger_manifest_free(safety);
    for (size_t i = 0; i < tc; i++) free(touched[i]);
    free(touched);
    skill_ledger_entry_free(entry);
    return true;
}

/* ── free helpers ─────────────────────────────────────────────────────────── */

void skill_ledger_entry_free(skill_ledger_entry_t *e) {
    if (!e) return;
    free(e->id); free(e->ts); free(e->actor); free(e->action); free(e->skill);
    free(e->evidence_json);
    skill_ledger_manifest_free_arrays(&e->before);
    skill_ledger_manifest_free_arrays(&e->after);
    free(e);
}

void skill_ledger_entries_free(skill_ledger_entry_t **arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++) skill_ledger_entry_free(arr[i]);
    free(arr);
}
