/*
 * projects_db.c — Per-profile first-class Project store (faithful C11 port of
 * hermes_cli/projects_db.py). See projects_db.h.
 */

#include "projects_db.h"
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/random.h>

struct projects_db {
    sqlite3 *conn;
};

/* Schema (idempotent; additive migrations applied on open). */
static const char *SCHEMA_SQL =
"CREATE TABLE IF NOT EXISTS projects ("
"    id            TEXT PRIMARY KEY,"
"    slug          TEXT NOT NULL UNIQUE,"
"    name          TEXT NOT NULL,"
"    description   TEXT,"
"    icon          TEXT,"
"    color         TEXT,"
"    board_slug    TEXT,"
"    primary_path  TEXT,"
"    created_at    INTEGER NOT NULL,"
"    archived      INTEGER NOT NULL DEFAULT 0"
");"
"CREATE TABLE IF NOT EXISTS project_folders ("
"    project_id  TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,"
"    path        TEXT NOT NULL,"
"    label       TEXT,"
"    is_primary  INTEGER NOT NULL DEFAULT 0,"
"    added_at    INTEGER NOT NULL,"
"    PRIMARY KEY (project_id, path)"
");"
"CREATE INDEX IF NOT EXISTS idx_project_folders_path ON project_folders(path);"
"CREATE TABLE IF NOT EXISTS project_meta ("
"    key    TEXT PRIMARY KEY,"
"    value  TEXT"
");"
"CREATE TABLE IF NOT EXISTS discovered_repos ("
"    root          TEXT PRIMARY KEY,"
"    label         TEXT,"
"    last_seen     INTEGER NOT NULL"
");";

/* _SLUG_RE = ^[a-z0-9][a-z0-9\-_]{0,63}$ */
static bool valid_slug_chars(const char *s) {
    if (!s || !*s) return false;
    for (size_t i = 0; s[i]; i++)
        if (!islower((unsigned char)s[i]) && !isdigit((unsigned char)s[i]) &&
            s[i] != '-' && s[i] != '_') return false;
    return true;
}

static char *xstrdup(const char *s) { return s ? strdup(s) : NULL; }

/* forward: defined later, used by add_folder/set_primary */
static void set_primary_locked(projects_db_t *db, const char *pid, const char *path);

/* ── helpers ── */

char *projects_db_slugify(const char *name) {
    char *s = xstrdup(name ? name : "");
    /* strip + lowercase */
    for (size_t i = 0; s[i]; i++) s[i] = (char)tolower((unsigned char)s[i]);
    /* collapse non [a-z0-9]+ into '-' */
    char *out = (char*)malloc(strlen(s) + 1);
    size_t o = 0; bool sep = false;
    for (size_t i = 0; s[i]; i++) {
        if (islower((unsigned char)s[i]) || isdigit((unsigned char)s[i])) {
            out[o++] = s[i]; sep = false;
        } else {
            if (o > 0 && !sep) { out[o++] = '-'; sep = true; }
        }
    }
    out[o] = '\0';
    /* strip leading/trailing '-' '_' */
    while (out[0] == '-' || out[0] == '_') memmove(out, out+1, strlen(out));
    size_t L = strlen(out);
    while (L > 0 && (out[L-1] == '-' || out[L-1] == '_')) out[--L] = '\0';
    /* cap 64 */
    if (strlen(out) > 64) { out[64] = '\0'; while (out[0]=='-'||out[0]=='_') memmove(out,out+1,strlen(out)); L=strlen(out); while(L>0 && (out[L-1]=='-'||out[L-1]=='_')) out[--L]='\0'; }
    free(s);
    if (out[0] == '\0') { free(out); return strdup("project"); }
    return out;
}

/* PoP: projects_db_normalize_slug @ hermes_cli/projects_db.py:normalize_slug */
char *projects_db_normalize_slug(const char *slug) {
    if (!slug) return NULL;
    char *s = xstrdup(slug);
    for (size_t i = 0; s[i]; i++) s[i] = (char)tolower((unsigned char)s[i]);
    /* strip outer whitespace */
    while (s[0] == ' ' || s[0] == '\t') memmove(s, s+1, strlen(s)+1);
    size_t L = strlen(s);
    while (L > 0 && (s[L-1]==' '||s[L-1]=='\t')) s[--L]='\0';
    if (s[0] == '\0') { free(s); return NULL; }
    /* validate ^[a-z0-9][a-z0-9\-_]{0,63}$ */
    if (strlen(s) > 64 || (!islower((unsigned char)s[0]) && !isdigit((unsigned char)s[0])) || !valid_slug_chars(s)) {
        free(s); return NULL;
    }
    return s;
}

/* PoP: projects_db_normalize_path @ hermes_cli/projects_db.py:_normalize_path */
char *projects_db_normalize_path(const char *path) {
    if (!path) return NULL;
    char *p = xstrdup(path);
    /* strip + expand: best-effort expanduser */
    if (p[0] == '~') {
        const char *home = getenv("HOME");
        if (home) {
            size_t need = strlen(home) + strlen(p+1) + 1;
            char *e = (char*)malloc(need);
            snprintf(e, need, "%s%s", home, p+1);
            free(p); p = e;
        }
    }
    /* collapse duplicate slashes + make absolute via realpath if possible */
    char *abs = realpath(p, NULL);
    if (!abs) {
        /* fall back to manual absolute + cleanup */
        if (p[0] != '/') {
            char cwd[4096]; if (getcwd(cwd, sizeof(cwd))) {
                size_t need = strlen(cwd)+1+strlen(p)+1;
                char *j = (char*)malloc(need);
                snprintf(j, need, "%s/%s", cwd, p);
                free(p); p = j;
            }
        }
        /* collapse multiple slashes */
        char *clean = (char*)malloc(strlen(p)+1);
        size_t o=0; bool slash=false;
        for (size_t i=0;p[i];i++){
            if (p[i]=='/'){ if(!slash){clean[o++]='/';slash=true;} }
            else { clean[o++]=p[i]; slash=false; }
        }
        clean[o]='\0';
        free(p); p=clean;
    } else { free(p); p = abs; }
    /* strip trailing sep */
    size_t L = strlen(p);
    while (L > 1 && (p[L-1]=='/'||p[L-1]=='\\')) p[--L]='\0';
    return p;
}

/* ── connection ── */

projects_db_t *projects_db_connect(const char *db_dir) {
    if (!db_dir) return NULL;
    size_t need = strlen(db_dir) + 1 + 12 + 1;
    char *path = (char*)malloc(need);
    snprintf(path, need, "%s/projects.db", db_dir);
    /* ensure dir exists */
    mkdir(db_dir, 0755);
    projects_db_t *db = (projects_db_t*)calloc(1, sizeof(projects_db_t));
    if (sqlite3_open(path, &db->conn) != SQLITE_OK) {
        free(path); projects_db_close(db); return NULL;
    }
    sqlite3_exec(db->conn, "PRAGMA foreign_keys=ON", 0,0,0);
    char *err = NULL;
    sqlite3_exec(db->conn, SCHEMA_SQL, 0, 0, &err);
    if (err) { sqlite3_free(err); }
    free(path);
    return db;
}

void projects_db_close(projects_db_t *db) {
    if (!db) return;
    if (db->conn) sqlite3_close(db->conn);
    free(db);
}

/* ── internal row -> project ── */

static project_folder_t *load_folders(projects_db_t *db, const char *pid, int *out_n) {
    const char *sql =
        "SELECT path, label, is_primary, added_at FROM project_folders "
        "WHERE project_id = ? ORDER BY is_primary DESC, added_at ASC";
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db->conn, sql, -1, &st, 0) != SQLITE_OK) { *out_n=0; return NULL; }
    sqlite3_bind_text(st, 1, pid, -1, SQLITE_TRANSIENT);
    project_folder_t *arr = NULL; int n=0, cap=0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (n>=cap){ cap=cap?cap*2:8; arr=(project_folder_t*)realloc(arr,cap*sizeof(project_folder_t)); }
        const unsigned char *p = sqlite3_column_text(st,0);
        const unsigned char *l = sqlite3_column_text(st,1);
        arr[n].path = xstrdup((const char*)p);
        arr[n].label = l ? xstrdup((const char*)l) : NULL;
        arr[n].is_primary = sqlite3_column_int(st,2) != 0;
        arr[n].added_at = sqlite3_column_int64(st,3);
        n++;
    }
    sqlite3_finalize(st);
    *out_n = n; return arr;
}

/* ── internal: fetch full project (with folders) by id or slug ── */
static project_t *get_project_full(projects_db_t *db, const char *id_or_slug, bool by_id) {
    const char *sql =
        "SELECT id, slug, name, description, icon, color, board_slug, "
        "primary_path, created_at, archived FROM projects WHERE ";
    char q[512];
    snprintf(q, sizeof(q), "%s%s = ?", sql, by_id ? "id" : "slug");
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db->conn, q, -1, &st, 0) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, id_or_slug, -1, SQLITE_TRANSIENT);
    project_t *p = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        p = (project_t*)calloc(1, sizeof(project_t));
        p->id = xstrdup((const char*)sqlite3_column_text(st,0));
        p->slug = xstrdup((const char*)sqlite3_column_text(st,1));
        p->name = xstrdup((const char*)sqlite3_column_text(st,2));
        const unsigned char *d = sqlite3_column_text(st,3);
        const unsigned char *ic = sqlite3_column_text(st,4);
        const unsigned char *co = sqlite3_column_text(st,5);
        const unsigned char *bs = sqlite3_column_text(st,6);
        const unsigned char *pp = sqlite3_column_text(st,7);
        p->description = d ? xstrdup((const char*)d) : NULL;
        p->icon = ic ? xstrdup((const char*)ic) : NULL;
        p->color = co ? xstrdup((const char*)co) : NULL;
        p->board_slug = bs ? xstrdup((const char*)bs) : NULL;
        p->primary_path = pp ? xstrdup((const char*)pp) : NULL;
        p->created_at = sqlite3_column_int64(st,8);
        p->archived = sqlite3_column_int(st,9) != 0;
        p->folders = load_folders(db, p->id, &p->n_folders);
    }
    sqlite3_finalize(st);
    return p;
}

/* unique slug */
/* PoP: unique_slug @ hermes_cli/projects_db.py:_unique_slug */
static char *unique_slug(projects_db_t *db, const char *candidate) {
    char *base = xstrdup(candidate);
    char *slug = xstrdup(base);
    int n = 1;
    for (;;) {
        sqlite3_stmt *st;
        sqlite3_prepare_v2(db->conn, "SELECT 1 FROM projects WHERE slug = ?", -1, &st, 0);
        sqlite3_bind_text(st, 1, slug, -1, SQLITE_TRANSIENT);
        bool taken = (sqlite3_step(st) == SQLITE_ROW);
        sqlite3_finalize(st);
        if (!taken) break;
        n++;
        char suffix[16]; snprintf(suffix, sizeof(suffix), "-%d", n);
        size_t room = 64 - strlen(suffix);
        char *tmp = (char*)malloc(65);
        strncpy(tmp, base, room); tmp[room]='\0';
        /* strip trailing '-' '_' */
        size_t L=strlen(tmp);
        while(L>0 && (tmp[L-1]=='-'||tmp[L-1]=='_')) tmp[--L]='\0';
        char *next = (char*)malloc(strlen(tmp)+strlen(suffix)+1);
        sprintf(next, "%s%s", tmp, suffix);
        free(tmp); free(slug); slug = next;
    }
    free(base);
    return slug;
}

/* ── create ── */
/* PoP: projects_db_create_project @ hermes_cli/projects_db.py:create_project */
char *projects_db_create_project(projects_db_t *db, const char *name,
                                 const char *slug, char **folders, int n_folders,
                                 const char *primary_path, const char *description,
                                 const char *icon, const char *color,
                                 const char *board_slug) {
    char *nm = xstrdup(name ? name : "");
    /* strip */
    while (nm[0]==' '||nm[0]=='\t') memmove(nm,nm+1,strlen(nm)+1);
    size_t L=strlen(nm); while(L>0&&(nm[L-1]==' '||nm[L-1]=='\t')) nm[--L]='\0';
    if (nm[0]=='\0') { free(nm); return NULL; }

    char *slug_candidate = slug ? projects_db_normalize_slug(slug) : NULL;
    if (!slug_candidate) { free(slug_candidate); slug_candidate = projects_db_slugify(name); }

    /* collect normalized folder paths (dedup) */
    char **fpaths = NULL; int nf=0, fcap=0;
    for (int i=0;i<n_folders;i++){
        char *norm = projects_db_normalize_path(folders[i]);
        if (!norm || !*norm) { free(norm); continue; }
        bool dup=false; for(int j=0;j<nf;j++) if(strcmp(fpaths[j],norm)==0){dup=true;break;}
        if(!dup){ if(nf>=fcap){fcap=fcap?fcap*2:8; fpaths=(char**)realloc(fpaths,fcap*sizeof(char*));} fpaths[nf++]=norm; } else free(norm);
    }
    char *primary = primary_path ? projects_db_normalize_path(primary_path) : NULL;
    if (primary){
        bool dup=false; for(int j=0;j<nf;j++) if(strcmp(fpaths[j],primary)==0){dup=true;break;}
        if(!dup){ if(nf>=fcap){fcap=fcap?fcap*2:8; fpaths=(char**)realloc(fpaths,fcap*sizeof(char*));} fpaths[nf++]=primary; }
        else { /* move primary to front already present; still set primary var */ free(primary); primary = xstrdup(fpaths[0]); }
    }
    if (primary == NULL && nf>0){ primary = xstrdup(fpaths[0]); }

    char pid[64]; { unsigned char rb[4]; if (getrandom(rb,4,0)==4) snprintf(pid,sizeof(pid),"p_%02x%02x%02x%02x",rb[0],rb[1],rb[2],rb[3]); else snprintf(pid,sizeof(pid),"p_%ld",(long)time(NULL)); }
    long now = (long)time(NULL);
    char *uniq = unique_slug(db, slug_candidate);

    sqlite3_exec(db->conn, "BEGIN", 0,0,0);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(db->conn,
        "INSERT INTO projects (id, slug, name, description, icon, color, board_slug, primary_path, created_at, archived) "
        "VALUES (?,?,?,?,?,?,?,?,?,0)", -1, &st, 0);
    sqlite3_bind_text(st,1,pid,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,2,uniq,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,3,nm,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,4,description?description:(char*)"",-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,5,icon?icon:"",-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,6,color?color:"",-1,SQLITE_TRANSIENT);
    char *bs = board_slug ? projects_db_normalize_slug(board_slug) : NULL;
    sqlite3_bind_text(st,7,bs?bs:"",-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,8,primary?primary:"",-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(st,9,now);
    sqlite3_step(st); sqlite3_finalize(st);
    if (bs) free(bs);

    for (int i=0;i<nf;i++){
        sqlite3_stmt *s2;
        sqlite3_prepare_v2(db->conn,
            "INSERT INTO project_folders (project_id, path, label, is_primary, added_at) VALUES (?,?,?,?,?)", -1, &s2, 0);
        sqlite3_bind_text(s2,1,pid,-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(s2,2,fpaths[i],-1,SQLITE_TRANSIENT);
        sqlite3_bind_null(s2,3);
        sqlite3_bind_int(s2,4, (primary && strcmp(fpaths[i],primary)==0)?1:0);
        sqlite3_bind_int64(s2,5,now);
        sqlite3_step(s2); sqlite3_finalize(s2);
    }
    sqlite3_exec(db->conn, "COMMIT", 0,0,0);

    /* cleanup */
    for (int i=0;i<nf;i++) free(fpaths[i]);
    free(fpaths);
    free(primary); free(uniq); free(slug_candidate); free(nm);
    return strdup(pid);
}

/* ── list ── */
project_t *projects_db_list_projects(projects_db_t *db, bool include_archived, int *out_count) {
    const char *sql =
        "SELECT id, slug, name, description, icon, color, board_slug, "
        "primary_path, created_at, archived FROM projects";
    char q[600];
    snprintf(q, sizeof(q), "%s%s ORDER BY created_at ASC",
             sql, include_archived ? "" : " WHERE archived = 0");
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db->conn, q, -1, &st, 0) != SQLITE_OK) { *out_count=0; return NULL; }
    project_t *arr=NULL; int n=0, cap=0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (n>=cap){cap=cap?cap*2:8; arr=(project_t*)realloc(arr,cap*sizeof(project_t));}
        project_t *p=&arr[n];
        memset(p,0,sizeof(*p));
        p->id=xstrdup((const char*)sqlite3_column_text(st,0));
        p->slug=xstrdup((const char*)sqlite3_column_text(st,1));
        p->name=xstrdup((const char*)sqlite3_column_text(st,2));
        const unsigned char *d=sqlite3_column_text(st,3);
        const unsigned char *ic=sqlite3_column_text(st,4);
        const unsigned char *co=sqlite3_column_text(st,5);
        const unsigned char *bs=sqlite3_column_text(st,6);
        const unsigned char *pp=sqlite3_column_text(st,7);
        p->description=d?xstrdup((const char*)d):NULL;
        p->icon=ic?xstrdup((const char*)ic):NULL;
        p->color=co?xstrdup((const char*)co):NULL;
        p->board_slug=bs?xstrdup((const char*)bs):NULL;
        p->primary_path=pp?xstrdup((const char*)pp):NULL;
        p->created_at=sqlite3_column_int64(st,8);
        p->archived=sqlite3_column_int(st,9)!=0;
        p->folders=load_folders(db,p->id,&p->n_folders);
        n++;
    }
    sqlite3_finalize(st);
    *out_count=n; return arr;
}

project_t *projects_db_get_project(projects_db_t *db, const char *id_or_slug) {
    project_t *p = get_project_full(db, id_or_slug, true);
    if (!p) {
        /* by slug (case-insensitive via lower) */
        char *low = xstrdup(id_or_slug);
        for (size_t i=0;low[i];i++) low[i]=(char)tolower((unsigned char)low[i]);
        p = get_project_full(db, low, false);
        free(low);
    }
    return p;
}

/* PoP: projects_db_update_project @ hermes_cli/projects_db.py:update_project */
bool projects_db_update_project(projects_db_t *db, const char *project_id,
                                const char *name, const char *description,
                                const char *icon, const char *color,
                                const char *board_slug) {
    char *sets[8]; const char *params[8]; int ns=0;
    if (name){
        char *nm=xstrdup(name);
        while(nm[0]==' '||nm[0]=='\t') memmove(nm,nm+1,strlen(nm)+1);
        size_t L=strlen(nm); while(L>0&&(nm[L-1]==' '||nm[L-1]=='\t')) nm[--L]='\0';
        if(nm[0]=='\0'){ free(nm); return false; }
        sets[ns]="name = ?"; params[ns]=nm; ns++;
    }
    if (description){ sets[ns]="description = ?"; params[ns]=description; ns++; }
    if (icon){ sets[ns]="icon = ?"; params[ns]=*icon?icon:NULL; ns++; }
    if (color){ sets[ns]="color = ?"; params[ns]=*color?color:NULL; ns++; }
    if (board_slug){
        char *bs=xstrdup(board_slug);
        while(bs[0]==' '||bs[0]=='\t') memmove(bs,bs+1,strlen(bs)+1);
        size_t L=strlen(bs); while(L>0&&(bs[L-1]==' '||bs[L-1]=='\t')) bs[--L]='\0';
        sets[ns]="board_slug = ?"; params[ns]=L?projects_db_normalize_slug(bs):NULL; ns++;
        free(bs);
    }
    if (ns==0) return false;
    char q[512]; int o=snprintf(q,sizeof(q),"UPDATE projects SET ");
    for (int i=0;i<ns;i++){ o+=snprintf(q+o,sizeof(q)-o,"%s%s", i?", ":"", sets[i]); }
    o+=snprintf(q+o,sizeof(q)-o," WHERE id = ?");
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db->conn,q,-1,&st,0)!=SQLITE_OK) return false;
    for (int i=0;i<ns;i++) {
        if (params[i]) sqlite3_bind_text(st,i+1,params[i],-1,SQLITE_TRANSIENT);
        else sqlite3_bind_null(st,i+1);
    }
    sqlite3_bind_text(st,ns+1,project_id,-1,SQLITE_TRANSIENT);
    sqlite3_step(st);
    int rc = sqlite3_changes(db->conn);
    sqlite3_finalize(st);
    /* free name copy if allocated */
    if (name) free((void*)params[0]); /* nm allocated above */
    return rc > 0;
}

/* PoP: projects_db_add_folder @ hermes_cli/projects_db.py:add_folder */
char *projects_db_add_folder(projects_db_t *db, const char *project_id,
                             const char *path, const char *label, bool is_primary) {
    char *norm = projects_db_normalize_path(path);
    if (!norm || !*norm) { free(norm); return NULL; }
    if (!projects_db_get_project(db, project_id)) { free(norm); return NULL; }
    long now = (long)time(NULL);
    sqlite3_exec(db->conn,"BEGIN",0,0,0);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(db->conn,
        "INSERT OR IGNORE INTO project_folders (project_id, path, label, is_primary, added_at) VALUES (?,?,?,0,?)",-1,&st,0);
    sqlite3_bind_text(st,1,project_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,2,norm,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,3,label?label:"",-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(st,4,now);
    sqlite3_step(st); sqlite3_finalize(st);
    if (label){
        sqlite3_stmt *s2;
        sqlite3_prepare_v2(db->conn,"UPDATE project_folders SET label = ? WHERE project_id = ? AND path = ?",-1,&s2,0);
        sqlite3_bind_text(s2,1,label,-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(s2,2,project_id,-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(s2,3,norm,-1,SQLITE_TRANSIENT);
        sqlite3_step(s2); sqlite3_finalize(s2);
    }
    if (is_primary) set_primary_locked(db, project_id, norm); /* helper below */
    else {
        sqlite3_stmt *s3;
        sqlite3_prepare_v2(db->conn,"SELECT 1 FROM project_folders WHERE project_id = ? AND is_primary = 1",-1,&s3,0);
        sqlite3_bind_text(s3,1,project_id,-1,SQLITE_TRANSIENT);
        bool has_primary = (sqlite3_step(s3)==SQLITE_ROW);
        sqlite3_finalize(s3);
        if (!has_primary) set_primary_locked(db, project_id, norm);
    }
    sqlite3_exec(db->conn,"COMMIT",0,0,0);
    return norm; /* caller owns */
}

/* PoP: set_primary_locked @ hermes_cli/projects_db.py:_set_primary_locked */
static void set_primary_locked(projects_db_t *db, const char *pid, const char *path) {
    sqlite3_stmt *s1;
    sqlite3_prepare_v2(db->conn,"UPDATE project_folders SET is_primary = 0 WHERE project_id = ?",-1,&s1,0);
    sqlite3_bind_text(s1,1,pid,-1,SQLITE_TRANSIENT); sqlite3_step(s1); sqlite3_finalize(s1);
    sqlite3_stmt *s2;
    sqlite3_prepare_v2(db->conn,"UPDATE project_folders SET is_primary = 1 WHERE project_id = ? AND path = ?",-1,&s2,0);
    sqlite3_bind_text(s2,1,pid,-1,SQLITE_TRANSIENT); sqlite3_bind_text(s2,2,path,-1,SQLITE_TRANSIENT);
    sqlite3_step(s2); sqlite3_finalize(s2);
    sqlite3_stmt *s3;
    sqlite3_prepare_v2(db->conn,"UPDATE projects SET primary_path = ? WHERE id = ?",-1,&s3,0);
    sqlite3_bind_text(s3,1,path,-1,SQLITE_TRANSIENT); sqlite3_bind_text(s3,2,pid,-1,SQLITE_TRANSIENT);
    sqlite3_step(s3); sqlite3_finalize(s3);
}


/* PoP: projects_db_set_primary @ hermes_cli/projects_db.py:set_primary */
bool projects_db_set_primary(projects_db_t *db, const char *project_id, const char *path) {
    char *norm = projects_db_normalize_path(path);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(db->conn,"SELECT 1 FROM project_folders WHERE project_id = ? AND path = ?",-1,&st,0);
    sqlite3_bind_text(st,1,project_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,2,norm,-1,SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(st)==SQLITE_ROW);
    sqlite3_finalize(st);
    if (!exists) { free(norm); return false; }
    set_primary_locked(db, project_id, norm);
    free(norm);
    return true;
}

/* PoP: projects_db_remove_folder @ hermes_cli/projects_db.py:remove_folder */
bool projects_db_remove_folder(projects_db_t *db, const char *project_id, const char *path) {
    char *norm = projects_db_normalize_path(path);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(db->conn,"SELECT is_primary FROM project_folders WHERE project_id = ? AND path = ?",-1,&st,0);
    sqlite3_bind_text(st,1,project_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,2,norm,-1,SQLITE_TRANSIENT);
    int was_primary = 0;
    if (sqlite3_step(st)==SQLITE_ROW) was_primary = sqlite3_column_int(st,0);
    sqlite3_finalize(st);
    sqlite3_stmt *d;
    sqlite3_prepare_v2(db->conn,"DELETE FROM project_folders WHERE project_id = ? AND path = ?",-1,&d,0);
    sqlite3_bind_text(d,1,project_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(d,2,norm,-1,SQLITE_TRANSIENT);
    sqlite3_step(d); int rc=sqlite3_changes(db->conn); sqlite3_finalize(d);
    if (was_primary) {
        sqlite3_stmt *nxt;
        sqlite3_prepare_v2(db->conn,"SELECT path FROM project_folders WHERE project_id = ? ORDER BY added_at ASC LIMIT 1",-1,&nxt,0);
        sqlite3_bind_text(nxt,1,project_id,-1,SQLITE_TRANSIENT);
        if (sqlite3_step(nxt)==SQLITE_ROW) {
            const unsigned char *np = sqlite3_column_text(nxt,0);
            set_primary_locked(db, project_id, (const char*)np);
        } else {
            sqlite3_stmt *u;
            sqlite3_prepare_v2(db->conn,"UPDATE projects SET primary_path = NULL WHERE id = ?",-1,&u,0);
            sqlite3_bind_text(u,1,project_id,-1,SQLITE_TRANSIENT);
            sqlite3_step(u); sqlite3_finalize(u);
        }
        sqlite3_finalize(nxt);
    }
    free(norm);
    return rc > 0;
}

/* PoP: projects_db_archive_project @ hermes_cli/projects_db.py:archive_project */
bool projects_db_archive_project(projects_db_t *db, const char *project_id) {
    sqlite3_stmt *st;
    sqlite3_prepare_v2(db->conn,"UPDATE projects SET archived = 1 WHERE id = ?",-1,&st,0);
    sqlite3_bind_text(st,1,project_id,-1,SQLITE_TRANSIENT);
    sqlite3_step(st); int rc=sqlite3_changes(db->conn); sqlite3_finalize(st);
    return rc>0;
}
/* PoP: projects_db_restore_project @ hermes_cli/projects_db.py:restore_project */
bool projects_db_restore_project(projects_db_t *db, const char *project_id) {
    sqlite3_stmt *st;
    sqlite3_prepare_v2(db->conn,"UPDATE projects SET archived = 0 WHERE id = ?",-1,&st,0);
    sqlite3_bind_text(st,1,project_id,-1,SQLITE_TRANSIENT);
    sqlite3_step(st); int rc=sqlite3_changes(db->conn); sqlite3_finalize(st);
    return rc>0;
}
/* PoP: projects_db_delete_project @ hermes_cli/projects_db.py:delete_project */
bool projects_db_delete_project(projects_db_t *db, const char *project_id) {
    sqlite3_stmt *st;
    sqlite3_prepare_v2(db->conn,"DELETE FROM projects WHERE id = ?",-1,&st,0);
    sqlite3_bind_text(st,1,project_id,-1,SQLITE_TRANSIENT);
    sqlite3_step(st); int rc=sqlite3_changes(db->conn); sqlite3_finalize(st);
    return rc>0;
}

/* ── active pointer ── */
/* PoP: projects_db_set_active @ hermes_cli/projects_db.py:set_active */
void projects_db_set_active(projects_db_t *db, const char *project_id) {
    if (!project_id) {
        sqlite3_exec(db->conn,"DELETE FROM project_meta WHERE key = 'active_id'",0,0,0);
        return;
    }
    sqlite3_stmt *st;
    sqlite3_prepare_v2(db->conn,
        "INSERT INTO project_meta (key, value) VALUES ('active_id', ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value",-1,&st,0);
    sqlite3_bind_text(st,1,project_id,-1,SQLITE_TRANSIENT);
    sqlite3_step(st); sqlite3_finalize(st);
}
/* PoP: projects_db_get_active_id @ hermes_cli/projects_db.py:get_active_id */
char *projects_db_get_active_id(projects_db_t *db) {
    sqlite3_stmt *st;
    sqlite3_prepare_v2(db->conn,"SELECT value FROM project_meta WHERE key = 'active_id'",-1,&st,0);
    char *r=NULL;
    if (sqlite3_step(st)==SQLITE_ROW){ const unsigned char *v=sqlite3_column_text(st,0); if(v) r=xstrdup((const char*)v); }
    sqlite3_finalize(st);
    return r;
}

/* ── discovered repos ── */
/* PoP: projects_db_record_discovered_repos @ hermes_cli/projects_db.py:record_discovered_repos */
int projects_db_record_discovered_repos(projects_db_t *db, const char **roots, const char **labels, int n, bool replace) {
    int written=0;
    sqlite3_exec(db->conn,"BEGIN",0,0,0);
    if (replace) sqlite3_exec(db->conn,"DELETE FROM discovered_repos",0,0,0);
    for (int i=0;i<n;i++){
        char *norm = projects_db_normalize_path(roots[i]);
        if (!norm || !*norm){ free(norm); continue; }
        const char *lab = labels && labels[i] && *labels[i] ? labels[i] : NULL;
        char *fallback = NULL;
        if (!lab){ const char *b = strrchr(norm,'/'); fallback = xstrdup(b?b+1:norm); lab = fallback; }
        long now=(long)time(NULL);
        sqlite3_stmt *st;
        sqlite3_prepare_v2(db->conn,
            "INSERT INTO discovered_repos (root, label, last_seen) VALUES (?,?,?) "
            "ON CONFLICT(root) DO UPDATE SET label = excluded.label, last_seen = excluded.last_seen",-1,&st,0);
        sqlite3_bind_text(st,1,norm,-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(st,2,lab,-1,SQLITE_TRANSIENT);
        sqlite3_bind_int64(st,3,now);
        sqlite3_step(st); sqlite3_finalize(st);
        written++;
        free(fallback); free(norm);
    }
    sqlite3_exec(db->conn,"COMMIT",0,0,0);
    return written;
}
discovered_repo_t *projects_db_list_discovered_repos(projects_db_t *db, int *out_count) {
    sqlite3_stmt *st;
    sqlite3_prepare_v2(db->conn,"SELECT root, label, last_seen FROM discovered_repos ORDER BY last_seen DESC",-1,&st,0);
    discovered_repo_t *arr=NULL; int n=0,cap=0;
    while (sqlite3_step(st)==SQLITE_ROW){
        if(n>=cap){cap=cap?cap*2:8; arr=(discovered_repo_t*)realloc(arr,cap*sizeof(discovered_repo_t));}
        arr[n].root=xstrdup((const char*)sqlite3_column_text(st,0));
        const unsigned char *l=sqlite3_column_text(st,1);
        arr[n].label=l?xstrdup((const char*)l):NULL;
        arr[n].last_seen=sqlite3_column_int64(st,2);
        n++;
    }
    sqlite3_finalize(st);
    *out_count=n; return arr;
}

/* ── resolution + naming ── */
project_t *projects_db_project_for_path(projects_db_t *db, const char *path, bool include_archived) {
    if (!path || !*path || !*path) return NULL;
    char *target = projects_db_normalize_path(path);
    if (!target || !*target){ free(target); return NULL; }
    const char *sql =
        "SELECT pf.project_id AS pid, pf.path AS folder FROM project_folders pf "
        "JOIN projects p ON p.id = pf.project_id";
    char q[600];
    snprintf(q,sizeof(q),"%s%s",sql, include_archived?"":" WHERE p.archived = 0");
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db->conn,q,-1,&st,0)!=SQLITE_OK){ free(target); return NULL; }
    char *best_pid=NULL; size_t best_len=0;
    while (sqlite3_step(st)==SQLITE_ROW){
        const unsigned char *pid=(const unsigned char*)sqlite3_column_text(st,0);
        const unsigned char *folder=(const unsigned char*)sqlite3_column_text(st,1);
        /* folder owns target when target == folder or nested under folder + sep */
        size_t fl=strlen((const char*)folder);
        bool owns = (strcmp(target,(const char*)folder)==0);
        if (!owns){
            /* strip trailing sep from folder */
            char *fb=xstrdup((const char*)folder);
            size_t fl2=strlen(fb);
            while(fl2>1 && (fb[fl2-1]=='/'||fb[fl2-1]=='\\')) fb[--fl2]='\0';
            if (strncmp(target,fb,fl2)==0 && (target[fl2]=='/'||target[fl2]=='\\')) owns=true;
            free(fb);
        }
        if (owns && fl > best_len){ best_len=fl; free(best_pid); best_pid=xstrdup((const char*)pid); }
    }
    sqlite3_finalize(st);
    free(target);
    if (!best_pid) return NULL;
    project_t *p = projects_db_get_project(db, best_pid);
    free(best_pid);
    return p;
}

/* PoP: projects_db_branch_name_for @ hermes_cli/projects_db.py:branch_name_for */
char *projects_db_branch_name_for(const project_t *proj, const char *task_id, const char *title) {
    char *slug = proj && proj->slug ? xstrdup(proj->slug) : projects_db_slugify(proj?proj->name:"");
    char *base = (char*)malloc(strlen(slug)+1+strlen(task_id?task_id:"")+1);
    sprintf(base,"%s/%s",slug,task_id?task_id:"");
    if (title && *title){
        char *low=xstrdup(title);
        for(size_t i=0;low[i];i++) low[i]=(char)tolower((unsigned char)low[i]);
        /* collapse non [a-z0-9._-]+ into '-' */
        char *ts=(char*)malloc(strlen(low)+1);
        size_t o=0,boolsep=false;
        for(size_t i=0;low[i];i++){
            char c=low[i];
            if(islower((unsigned char)c)||isdigit((unsigned char)c)||c=='.'||c=='_'||c=='-'){ts[o++]=c;boolsep=false;}
            else { if(o>0&&!boolsep){ts[o++]='-';boolsep=true;} }
        }
        ts[o]='\0';
        while(ts[0]=='-') memmove(ts,ts+1,strlen(ts)+1);
        size_t L=strlen(ts); while(L>0&&ts[L-1]=='-') ts[--L]='\0';
        if(strlen(ts)>40){ ts[40]='\0'; while(ts[0]=='-') memmove(ts,ts+1,strlen(ts)+1); L=strlen(ts); while(L>0&&ts[L-1]=='-') ts[--L]='\0'; }
        if(ts[0]!='\0'){
            char *next=(char*)malloc(strlen(base)+1+strlen(ts)+1);
            sprintf(next,"%s-%s",base,ts);
            free(base); base=next;
        }
        free(ts); free(low);
    }
    free(slug);
    return base;
}

/* ── free helpers ── */
void projects_db_free_folders(project_folder_t *arr, int n) {
    if (!arr) return;
    for (int i=0;i<n;i++){ free(arr[i].path); free(arr[i].label); }
    free(arr);
}
void projects_db_free_project_fields(project_t *p) {
    if (!p) return;
    free(p->id); free(p->slug); free(p->name); free(p->description);
    free(p->icon); free(p->color); free(p->board_slug); free(p->primary_path);
    projects_db_free_folders(p->folders, p->n_folders);
    p->folders = NULL; p->n_folders = 0;
}
void projects_db_free_project(project_t *p) {
    if (!p) return;
    projects_db_free_project_fields(p);
    free(p);
}
void projects_db_free_projects(project_t *arr, int n) {
    if (!arr) return;
    for (int i=0;i<n;i++) projects_db_free_project_fields(&arr[i]);
    free(arr);
}
void projects_db_free_repos(discovered_repo_t *arr, int n) {
    if (!arr) return;
    for (int i=0;i<n;i++){ free(arr[i].root); free(arr[i].label); }
    free(arr);
}
