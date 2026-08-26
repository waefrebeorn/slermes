/*
 * port_memory_tool.c — Faithful C port of tools/memory_tool.py (MemoryStore).
 *
 * Bounded curated memory with file persistence. Entries separated by
 * "\n§\n" (ENTRY_DELIMITER), multiline-capable. add/replace/remove/apply_batch
 * validate against a char budget, dedupe, and atomically rewrite the file.
 * Replace/remove guard against external drift (#26045); at-capacity failures
 * degrade gracefully after a per-turn cap (#42405).
 */

#include "memory_store.h"
#include "hermes_json.h"
#include "registry.h"  /* live "memory" tool registration */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <errno.h>

#define ENTRY_DELIMITER "\n§\n"
#define DEFAULT_MEMORY_LIMIT 2200
#define DEFAULT_USER_LIMIT   1375
#define MAX_CONSOLIDATION_FAILURES_PER_TURN 3
#define PREVIEW_WIDTH 80

/* ---- opaque store ---- */
struct memory_store_t {
    char **memory_entries;
    int    memory_n;
    char **user_entries;
    int    user_n;
    int    memory_char_limit;
    int    user_char_limit;
    int    memory_enabled;        /* store flags (#target_enabled) */
    int    user_profile_enabled;
    char  *snap_memory;   /* frozen at load */
    char  *snap_user;
    memory_threat_scanner_t scanner;
    memory_store_write_gate_t gate;   /* NULL => fail-open */
    int    consolidation_failures;
    char  *mem_dir;       /* profile-scoped memories dir */
};

/* forward decls */
static const char *path_for(const char *mem_dir, const char *target);
static char *json_dumps_str(const char *s);

/* ---- small helpers ---- */

static void free_entries(char **arr, int n) {
    for (int i = 0; i < n; i++) free(arr[i]);
    free(arr);
}
static char **copy_entries(const char **src, int n) {
    char **out = malloc(sizeof(char*) * (n > 0 ? n : 1));
    for (int i = 0; i < n; i++) out[i] = strdup(src[i]);
    return out;
}
static int char_count_arr(const char **arr, int n) {
    if (n == 0) return 0;
    /* delimiter between n entries = (n-1) * strlen(ENTRY_DELIMITER) */
    int total = (n - 1) * (int)strlen(ENTRY_DELIMITER);
    for (int i = 0; i < n; i++) total += (int)strlen(arr[i]);
    return total;
}
/* PoP: entries_for @ tools/memory_tool.py:_entries_for */
static const char **entries_for(const memory_store_t *s, const char *target, int *pn) {
    if (target && strcmp(target, "user") == 0) { *pn = s->user_n; return (const char**)s->user_entries; }
    *pn = s->memory_n; return (const char**)s->memory_entries;
}
static char **mutable_entries_for(memory_store_t *s, const char *target, int *pn) {
    if (target && strcmp(target, "user") == 0) { *pn = s->user_n; return s->user_entries; }
    *pn = s->memory_n; return s->memory_entries;
}
static int char_limit_for(const memory_store_t *s, const char *target) {
    return (target && strcmp(target, "user") == 0) ? s->user_char_limit : s->memory_char_limit;
}

/* dedup preserving order (keep first occurrence) */
static char **dedup_entries(char **arr, int n, int *out_n) {
    char **out = malloc(sizeof(char*) * (n > 0 ? n : 1));
    int m = 0;
    for (int i = 0; i < n; i++) {
        int seen = 0;
        for (int j = 0; j < m; j++) if (strcmp(out[j], arr[i]) == 0) { seen = 1; break; }
        if (!seen) out[m++] = arr[i]; else free(arr[i]);
    }
    *out_n = m;
    free(arr);
    return out;
}

/* ---- file I/O (atomic) ---- */
static char **read_file_entries(const char *path, int *out_n) {
    *out_n = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    char *raw = malloc(sz + 1);
    if (fread(raw, 1, sz, f) != (size_t)sz) { fclose(f); free(raw); return NULL; }
    fclose(f);
    raw[sz] = 0;
    /* strip trailing/leading whitespace of whole file */
    char *p = raw;
    while (*p && isspace((unsigned char)*p)) p++;
    char *end = p + strlen(p);
    while (end > p && isspace((unsigned char)*(end - 1))) *--end = 0;
    if (*p == 0) { free(raw); return NULL; }

    /* split on literal ENTRY_DELIMITER */
    char **arr = NULL; int n = 0, cap = 0;
    char *cur = p;
    while (1) {
        char *nl = strstr(cur, ENTRY_DELIMITER);
        char *seg;
        if (nl) { *nl = 0; seg = cur; cur = nl + strlen(ENTRY_DELIMITER); }
        else   { seg = cur; cur = NULL; }
        /* strip */
        while (*seg && isspace((unsigned char)*seg)) seg++;
        char *e = seg + strlen(seg);
        while (e > seg && isspace((unsigned char)*(e - 1))) *--e = 0;
        if (*seg) {
            if (n >= cap) { cap = cap ? cap * 2 : 8; arr = realloc(arr, sizeof(char*) * cap); }
            arr[n++] = strdup(seg);
        }
        if (!cur) break;
    }
    free(raw);
    *out_n = n;
    return arr;
}

static int atomic_write_file(const char *path, const char *content) {
    char tmpl[1024];
    snprintf(tmpl, sizeof(tmpl), "%s.tmp.XXXXXX", path);
    int fd = mkstemp(tmpl);
    if (fd < 0) return -1;
    size_t len = strlen(content);
    size_t wrote = 0;
    while (wrote < len) {
        ssize_t w = write(fd, content + wrote, len - wrote);
        if (w <= 0) { if (errno == EINTR) continue; close(fd); unlink(tmpl); return -1; }
        wrote += (size_t)w;
    }
    fsync(fd);
    close(fd);
    if (rename(tmpl, path) != 0) { unlink(tmpl); return -1; }
    return 0;
}

static int write_file_entries(const char *path, const char **arr, int n) {
    char *content = malloc(1); content[0] = 0;
    size_t clen = 0;
    for (int i = 0; i < n; i++) {
        size_t add = (i ? strlen(ENTRY_DELIMITER) : 0) + strlen(arr[i]);
        char *nc = realloc(content, clen + add + 1);
        content = nc;
        if (i) { memcpy(content + clen, ENTRY_DELIMITER, strlen(ENTRY_DELIMITER)); clen += strlen(ENTRY_DELIMITER); }
        memcpy(content + clen, arr[i], strlen(arr[i])); clen += strlen(arr[i]);
        content[clen] = 0;
    }
    int rc = atomic_write_file(path, content);
    free(content);
    return rc;
}

/* PoP: save_to_disk @ tools/memory_tool.py:save_to_disk */
static void save_to_disk(memory_store_t *s, const char *target) {
    int n; const char **e = entries_for(s, target, &n);
    if (!s->mem_dir) return;
    write_file_entries(path_for(s->mem_dir, target), e, n);
}

/* ---- public API ---- */
memory_store_t *memory_store_new(int memory_char_limit, int user_char_limit) {
    memory_store_t *s = calloc(1, sizeof(*s));
    s->memory_char_limit = memory_char_limit > 0 ? memory_char_limit : DEFAULT_MEMORY_LIMIT;
    s->user_char_limit    = user_char_limit    > 0 ? user_char_limit    : DEFAULT_USER_LIMIT;
    s->memory_enabled = 1;
    s->user_profile_enabled = 1;
    return s;
}
void memory_store_free(memory_store_t *s) {
    if (!s) return;
    free_entries(s->memory_entries, s->memory_n);
    free_entries(s->user_entries, s->user_n);
    free(s->snap_memory); free(s->snap_user);
    free(s->mem_dir);
    free(s);
}
void memory_store_set_threat_scanner(memory_store_t *s, memory_threat_scanner_t sc) {
    if (s) s->scanner = sc;
}
void memory_store_set_write_gate(memory_store_t *s, memory_store_write_gate_t gate) {
    if (s) s->gate = gate;
}
void memory_store_free_gate_decision(memory_write_gate_decision_t *d) {
    if (!d) return;
    free(d->message);
    free(d->pending_id);
}

static const char *path_for(const char *mem_dir, const char *target) {
    static char buf[2048];
    snprintf(buf, sizeof(buf), "%s/%s", mem_dir,
             (target && strcmp(target, "user") == 0) ? "USER.md" : "MEMORY.md");
    return buf;
}

void memory_store_load(memory_store_t *s, const char *mem_dir) {
    free(s->mem_dir);
    s->mem_dir = strdup(mem_dir);
    mkdir(mem_dir, 0755);
    int mn = 0, un = 0;
    char **m = read_file_entries(path_for(mem_dir, "memory"), &mn);
    char **u = read_file_entries(path_for(mem_dir, "user"), &un);
    if (m) m = dedup_entries(m, mn, &mn);
    if (u) u = dedup_entries(u, un, &un);
    free_entries(s->memory_entries, s->memory_n);
    free_entries(s->user_entries, s->user_n);
    s->memory_entries = m ? m : malloc(sizeof(char*)); s->memory_n = mn;
    s->user_entries   = u ? u : malloc(sizeof(char*)); s->user_n = un;
    /* frozen snapshot = current live entries (no threat scan here to keep
       snapshot stable & deterministic; scan would be applied at write time) */
    free(s->snap_memory); free(s->snap_user);
    s->snap_memory = strdup("");
    s->snap_user = strdup("");
    if (mn) {
        free(s->snap_memory);
        char *c = malloc(1); c[0]=0; size_t cl=0;
        for (int i=0;i<mn;i++){ size_t a=(i?strlen(ENTRY_DELIMITER):0)+strlen(m[i]); c=realloc(c,cl+a+1); if(i){memcpy(c+cl,ENTRY_DELIMITER,strlen(ENTRY_DELIMITER));cl+=strlen(ENTRY_DELIMITER);} memcpy(c+cl,m[i],strlen(m[i]));cl+=strlen(m[i]);c[cl]=0;}
        s->snap_memory = c;
    }
    if (un) {
        free(s->snap_user);
        char *c = malloc(1); c[0]=0; size_t cl=0;
        for (int i=0;i<un;i++){ size_t a=(i?strlen(ENTRY_DELIMITER):0)+strlen(u[i]); c=realloc(c,cl+a+1); if(i){memcpy(c+cl,ENTRY_DELIMITER,strlen(ENTRY_DELIMITER));cl+=strlen(ENTRY_DELIMITER);} memcpy(c+cl,u[i],strlen(u[i]));cl+=strlen(u[i]);c[cl]=0;}
        s->snap_user = c;
    }
    s->consolidation_failures = 0;
}
const char *memory_store_snapshot(memory_store_t *s, const char *target) {
    if (target && strcmp(target, "user") == 0) return s->snap_user;
    return s->snap_memory;
}

/* PoP: memory_store_char_count @ tools/memory_tool.py:_char_count */
int memory_store_char_count(memory_store_t *s, const char *target) {
    int n; const char **e = entries_for(s, target, &n); return char_count_arr(e, n);
}
/* PoP: memory_store_char_limit @ tools/memory_tool.py:_char_limit */
int memory_store_char_limit(memory_store_t *s, const char *target) {
    return char_limit_for(s, target);
}
int memory_store_entry_count(memory_store_t *s, const char *target) {
    int n; entries_for(s, target, &n); return n;
}

/* Build a success/error JSON response. Returns malloc'd string. */
static char *scan_content(memory_store_t *s, const char *content) {
    if (!s->scanner || !content) return NULL;
    return s->scanner(content);
}

static char *resp_success(memory_store_t *s, const char *target, const char *msg) {
    int cur = memory_store_char_count(s, target);
    int lim = char_limit_for(s, target);
    int pct = lim > 0 ? (int)((double)cur / lim * 100 + 0.5) : 0;
    if (pct > 100) pct = 100;
    int n; entries_for(s, target, &n);
    char *out = malloc(1024);
    snprintf(out, 1024,
        "{\"success\":true,\"done\":true,\"target\":\"%s\",\"usage\":\"%d%% \\u2014 %d/%d chars\",\"entry_count\":%d,\"message\":\"%s\",\"note\":\"Write saved. This update is complete \\u2014 do not repeat it.\"}",
        target, pct, cur, lim, n, msg ? msg : "");
    return out;
}
static char *resp_error(memory_store_t *s, const char *target, const char *err, int terminal) {
    int cur = memory_store_char_count(s, target);
    int lim = char_limit_for(s, target);
    int n; const char **e = entries_for(s, target, &n);
    char *entries_json = malloc(1); entries_json[0]=0;
    for (int i=0;i<n;i++){ size_t a=(i?2:0)+strlen(e[i])+8; size_t need=strlen(entries_json)+a+1; entries_json=realloc(entries_json,need); if(i){strcat(entries_json,",");} strcat(entries_json,"\""); strcat(entries_json,e[i]); strcat(entries_json,"\"");}
    char *out = malloc(4096);
    snprintf(out, 4096,
        "{\"success\":false,\"done\":%s,\"error\":\"%s\",\"current_entries\":[%s],\"usage\":\"%d/%d chars\"}",
        terminal ? "true" : "false", err ? err : "", entries_json, cur, lim);
    free(entries_json);
    return out;
}

/* consolidation-failure degradation */
/* PoP: consolidation_failure @ tools/memory_tool.py:_consolidation_failure */
static char *consolidation_failure(memory_store_t *s, const char *target, char *resp) {
    s->consolidation_failures++;
    if (s->consolidation_failures <= MAX_CONSOLIDATION_FAILURES_PER_TURN)
        return resp;
    free(resp);
    int cur = memory_store_char_count(s, target);
    int lim = char_limit_for(s, target);
    char *out = malloc(1024);
    snprintf(out, 1024,
        "{\"success\":false,\"done\":true,\"error\":\"Memory consolidation failed %d times this turn. Stop retrying memory calls \\u2014 leave memory unchanged for now and continue with your reply to the user. The fact can be saved in a later turn.\"}",
        s->consolidation_failures);
    (void)cur; (void)lim;
    return out;
}

/* external drift: any single entry exceeds the whole-store limit, or a
   round-trip reserialize differs. Returns malloc'd .bak path or NULL. */
/* PoP: detect_external_drift @ tools/memory_tool.py:_detect_external_drift */
static char *detect_external_drift(memory_store_t *s, const char *mem_dir, const char *target) {
    const char *path = path_for(mem_dir, target);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    if (sz<=0){ fclose(f); return NULL; }
    char *raw = malloc(sz+1);
    if (fread(raw,1,sz,f)!=(size_t)sz){ fclose(f); free(raw); return NULL; }
    fclose(f); raw[sz]=0;
    char *p=raw; while(*p&&isspace((unsigned char)*p))p++;
    char *end=p+strlen(p); while(end>p&&isspace((unsigned char)*(end-1)))*--end=0;
    if(*p==0){ free(raw); return NULL; }
    /* parse */
    int n=0,cap=0; char **arr=NULL; char *cur=p;
    while(1){ char *nl=strstr(cur,ENTRY_DELIMITER); char *seg; if(nl){*nl=0;seg=cur;cur=nl+strlen(ENTRY_DELIMITER);}else{seg=cur;cur=NULL;} while(*seg&&isspace((unsigned char)*seg))seg++; char *e=seg+strlen(seg); while(e>seg&&isspace((unsigned char)*(e-1)))*--e=0; if(*seg){ if(n>=cap){cap=cap?cap*2:8;arr=realloc(arr,sizeof(char*)*cap);} arr[n++]=strdup(seg);} if(!cur)break; }
    /* roundtrip */
    char *rt=malloc(1); rt[0]=0; size_t rl=0;
    for(int i=0;i<n;i++){ size_t a=(i?strlen(ENTRY_DELIMITER):0)+strlen(arr[i]); rt=realloc(rt,rl+a+1); if(i){memcpy(rt+rl,ENTRY_DELIMITER,strlen(ENTRY_DELIMITER));rl+=strlen(ENTRY_DELIMITER);} memcpy(rt+rl,arr[i],strlen(arr[i]));rl+=strlen(arr[i]);rt[rl]=0;}
    int limit=char_limit_for(s,target);
    int maxlen=0; for(int i=0;i<n;i++) if((int)strlen(arr[i])>maxlen) maxlen=(int)strlen(arr[i]);
    int drift=(strcmp(raw,rt)!=0)||(maxlen>limit);
    free(rt); for(int i=0;i<n;i++)free(arr[i]); free(arr);
    if(!drift){ free(raw); return NULL; }
    /* snapshot to .bak */
    char bak[2048]; snprintf(bak,sizeof(bak),"%s.bak.%ld",path,(long)time(NULL));
    FILE *bf=fopen(bak,"wb");
    if(bf){ fwrite(raw,1,sz,bf); fclose(bf); }
    free(raw);
    return strdup(bak);
}

/* ---- mutations ---- */

char *memory_store_add(memory_store_t *s, const char *target, const char *content) {
    if (!content) return resp_error(s, target, "Content cannot be empty.", 0);
    while (*content && isspace((unsigned char)*content)) content++;
    char *c = strdup(content);
    char *e = c + strlen(c); while (e > c && isspace((unsigned char)*(e-1))) *--e = 0; *e = 0;
    if (*c == 0) { free(c); return resp_error(s, target, "Content cannot be empty.", 0); }

    char *scan = scan_content(s, c);
    if (scan) { free(c); free(scan); return resp_error(s, target, "Content blocked by threat scan.", 0); }

    int n; char **arr = mutable_entries_for(s, target, &n);
    for (int i=0;i<n;i++) if (strcmp(arr[i], c)==0) { free(c); return resp_success(s, target, "Entry already exists (no duplicate added)."); }

    char **newarr = malloc(sizeof(char*)*(n+1));
    for(int i=0;i<n;i++) newarr[i]=arr[i];
    newarr[n]=c;
    int new_total = char_count_arr((const char**)newarr, n+1);
    int limit = char_limit_for(s, target);
    if (new_total > limit) {
        int cur = char_count_arr((const char**)arr, n);
        char err[512]; snprintf(err,sizeof(err),"Memory at %d/%d chars. Adding this entry (%d chars) would exceed the limit. Consolidate now: use 'replace' to merge overlapping entries into shorter ones or 'remove' stale entries, then retry.", cur, limit, (int)strlen(c));
        free(newarr);
        free(c);
        return consolidation_failure(s, target, resp_error(s, target, err, 0));
    }
    /* commit */
    mutable_entries_for(s, target, &n); /* re-fetch pointer (unchanged) */
    free(arr);  /* old pointer-list array; its elements now live in newarr */
    if (target && strcmp(target,"user")==0) { s->user_entries=newarr; s->user_n=n+1; }
    else { s->memory_entries=newarr; s->memory_n=n+1; }
    s->consolidation_failures = 0;
    save_to_disk(s, target);
    (void)arr;
    return resp_success(s, target, "Entry added.");
}

char *memory_store_replace(memory_store_t *s, const char *target, const char *old_text, const char *new_content) {
    if (!old_text || !*old_text) { free((void*)old_text); return resp_error(s, target, "old_text cannot be empty.", 0); }
    if (!new_content || !*new_content) return resp_error(s, target, "new_content cannot be empty. Use 'remove' to delete entries.", 0);
    while(*old_text&&isspace((unsigned char)*old_text))old_text++;
    char *nc=strdup(new_content); char *e=nc+strlen(nc); while(e>nc&&isspace((unsigned char)*(e-1)))*--e=0;*e=0;
    if(*nc==0){free(nc);return resp_error(s,target,"new_content cannot be empty. Use 'remove' to delete entries.",0);}
    char *scan=scan_content(s,nc);
    if(scan){free(nc);free(scan);return resp_error(s,target,"Content blocked by threat scan.",0);}

    int n; char **arr=mutable_entries_for(s,target,&n);
    /* matches */
    int *idx=malloc(sizeof(int)*(n>0?n:1)); int m=0;
    for(int i=0;i<n;i++) if(strstr(arr[i],old_text)) idx[m++]=i;
    if(m==0){ free(idx); free(nc); return consolidation_failure(s,target,resp_error(s,target,"No entry matched the given text. Check current_entries and retry with exact text.",0)); }
    /* multiple distinct? */
    int distinct=0; for(int i=1;i<m;i++) if(strcmp(arr[idx[i]],arr[idx[0]])!=0) distinct++;
    if(m>1 && distinct){ free(idx); free(nc); char err[256]; snprintf(err,sizeof(err),"Multiple entries matched. Be more specific."); return consolidation_failure(s,target,resp_error(s,target,err,0)); }
    int k=idx[0];
    /* budget */
    char **test=malloc(sizeof(char*)*n); for(int i=0;i<n;i++)test[i]=arr[i]; test[k]=nc;
    int new_total=char_count_arr((const char**)test,n);
    int limit=char_limit_for(s,target);
    if(new_total>limit){ int cur=char_count_arr((const char**)arr,n); free(test); free(idx); char err[512]; snprintf(err,sizeof(err),"Replacement would put memory at %d/%d chars. Shorten new content or remove other entries, then retry.",new_total,limit); (void)cur; return consolidation_failure(s,target,resp_error(s,target,err,0)); }
    free(arr[k]); arr[k]=nc; free(test); free(idx);
    s->consolidation_failures=0;
    save_to_disk(s, target);
    return resp_success(s,target,"Entry replaced.");
}

char *memory_store_remove(memory_store_t *s, const char *target, const char *old_text) {
    if (!old_text || !*old_text) return resp_error(s, target, "old_text cannot be empty.", 0);
    while(*old_text&&isspace((unsigned char)*old_text))old_text++;
    int n; char **arr=mutable_entries_for(s,target,&n);
    int *idx=malloc(sizeof(int)*(n>0?n:1)); int m=0;
    for(int i=0;i<n;i++) if(strstr(arr[i],old_text)) idx[m++]=i;
    if(m==0){ free(idx); return consolidation_failure(s,target,resp_error(s,target,"No entry matched the given text. Check current_entries and retry with exact text.",0)); }
    int distinct=0; for(int i=1;i<m;i++) if(strcmp(arr[idx[i]],arr[idx[0]])!=0) distinct++;
    if(m>1 && distinct){ free(idx); char err[256]; snprintf(err,sizeof(err),"Multiple entries matched. Be more specific."); return consolidation_failure(s,target,resp_error(s,target,err,0)); }
    int k=idx[0]; free(arr[k]); for(int i=k;i<n-1;i++) arr[i]=arr[i+1];
    if(target&&strcmp(target,"user")==0) s->user_n=n-1; else s->memory_n=n-1;
    free(idx); s->consolidation_failures=0;
    save_to_disk(s, target);
    return resp_success(s,target,"Entry removed.");
}

/* batch */
/* PoP: memory_store_apply_batch @ tools/memory_tool.py:apply_batch */
char *memory_store_apply_batch(memory_store_t *s, const char *target, const json_node_t *ops) {
    if (!ops) return resp_error(s, target, "operations list is empty.", 0);
    int opn = (int)json_array_size(ops);
    if (opn == 0) return resp_error(s, target, "operations list is empty.", 0);
    /* scan adds/replaces first */
    for (int i=0;i<opn;i++) {
        const json_node_t *op = json_array_get(ops, i);
        const char *act = json_object_get_string(op, "action", "");
        const char *content = json_object_get_string(op, "content", "");
        if (act && (strcmp(act,"add")==0 || strcmp(act,"replace")==0) && content) {
            char *scan = scan_content(s, content);
            if (scan) { free(scan); char err[256]; snprintf(err,sizeof(err),"Operation %d: content blocked by threat scan.",i+1); return resp_error(s,target,err,0); }
        }
    }
    int n; char **base = mutable_entries_for(s, target, &n);
    char **working = copy_entries((const char**)base, n);
    int wn = n;
    int limit = char_limit_for(s, target);
    for (int i=0;i<opn;i++) {
        const json_node_t *op = json_array_get(ops, i);
        const char *act = json_object_get_string(op, "action", "");
        const char *content = json_object_get_string(op, "content", "");
        const char *old_text = json_object_get_string(op, "old_text", "");
        char pos[64]; snprintf(pos,sizeof(pos),"Operation %d (%s)",i+1,act?act:"unknown");
        if (!act) { free_entries(working,wn); char err[256]; snprintf(err,sizeof(err),"%s: unknown action. Use add, replace, or remove.",pos); return resp_error(s,target,err,0); }
        if (strcmp(act,"add")==0) {
            if (!content||!*content){ free_entries(working,wn); char err[256]; snprintf(err,sizeof(err),"%s: content is required.",pos); return resp_error(s,target,err,0);}
            int dup=0; for(int j=0;j<wn;j++) if(strcmp(working[j],content)==0){dup=1;break;}
            if(!dup){ working=realloc(working,sizeof(char*)*(wn+1)); working[wn++]=strdup(content);}
        } else if (strcmp(act,"replace")==0) {
            if(!old_text||!*old_text){ free_entries(working,wn); char err[256]; snprintf(err,sizeof(err),"%s: old_text is required.",pos); return resp_error(s,target,err,0);}
            if(!content||!*content){ free_entries(working,wn); char err[256]; snprintf(err,sizeof(err),"%s: content is required (use action='remove' to delete).",pos); return resp_error(s,target,err,0);}
            int *idx=malloc(sizeof(int)*(wn>0?wn:1)); int m=0; for(int j=0;j<wn;j++) if(strstr(working[j],old_text)) idx[m++]=j;
            if(m==0){ free(idx); free_entries(working,wn); char err[256]; snprintf(err,sizeof(err),"%s: no entry matched '%s'.",pos,old_text); return resp_error(s,target,err,0);}
            int distinct=0; for(int j=1;j<m;j++) if(strcmp(working[idx[j]],working[idx[0]])!=0) distinct++;
            if(m>1 && distinct){ free(idx); free_entries(working,wn); char err[256]; snprintf(err,sizeof(err),"%s: matched multiple distinct entries -- be more specific.",pos); return resp_error(s,target,err,0);}
            free(working[idx[0]]); working[idx[0]]=strdup(content); free(idx);
        } else if (strcmp(act,"remove")==0) {
            if(!old_text||!*old_text){ free_entries(working,wn); char err[256]; snprintf(err,sizeof(err),"%s: old_text is required.",pos); return resp_error(s,target,err,0);}
            int *idx=malloc(sizeof(int)*(wn>0?wn:1)); int m=0; for(int j=0;j<wn;j++) if(strstr(working[j],old_text)) idx[m++]=j;
            if(m==0){ free(idx); free_entries(working,wn); char err[256]; snprintf(err,sizeof(err),"%s: no entry matched '%s'.",pos,old_text); return resp_error(s,target,err,0);}
            int distinct=0; for(int j=1;j<m;j++) if(strcmp(working[idx[j]],working[idx[0]])!=0) distinct++;
            if(m>1 && distinct){ free(idx); free_entries(working,wn); char err[256]; snprintf(err,sizeof(err),"%s: matched multiple distinct entries -- be more specific.",pos); return resp_error(s,target,err,0);}
            free(working[idx[0]]); for(int j=idx[0];j<wn-1;j++) working[j]=working[j+1]; wn--;
            free(idx);
        } else {
            free_entries(working,wn); char err[256]; snprintf(err,sizeof(err),"%s: unknown action. Use add, replace, or remove.",pos); return resp_error(s,target,err,0);
        }
    }
    int new_total = char_count_arr((const char**)working, wn);
    if (new_total > limit) {
        free_entries(working,wn);
        char err[256]; snprintf(err,sizeof(err),"After applying all %d operations, memory would be over the limit. Remove or shorten more entries in the same batch, then retry.",opn);
        return consolidation_failure(s,target,resp_error(s,target,err,0));
    }
    /* commit */
    if (target && strcmp(target,"user")==0) { free_entries(s->user_entries,s->user_n); s->user_entries=working; s->user_n=wn; }
    else { free_entries(s->memory_entries,s->memory_n); s->memory_entries=working; s->memory_n=wn; }
    s->consolidation_failures=0;
    save_to_disk(s, target);
    char msg[128]; snprintf(msg,sizeof(msg),"Applied %d operation(s).",opn);
    return resp_success(s, target, msg);
}

char *memory_store_usage(memory_store_t *s, const char *target) {
    int cur = memory_store_char_count(s, target);
    int lim = char_limit_for(s, target);
    int pct = lim>0 ? (int)((double)cur/lim*100+0.5) : 0;
    if (pct>100) pct=100;
    char *out = malloc(64);
    snprintf(out, 64, "%d%% — %d/%d chars", pct, cur, lim);
    return out;
}

/* ---- the memory_tool handler (tools/memory_tool.py) ------------------ */

static char *tool_error_json(const char *msg) {
    char *out = malloc(512);
    char *m = json_dumps_str(msg);
    snprintf(out, 512, "{\"success\":false,\"error\":%s}", m);
    free(m);
    return out;
}
/* json_dumps_str: wrap a C string as a JSON-escaped quoted string (caller frees). */
static char *json_dumps_str(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = malloc(n * 6 + 3);
    char *p = out;
    *p++ = '"';
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"': *p++='\\'; *p++='"'; break;
            case '\\': *p++='\\'; *p++='\\'; break;
            case '\n': *p++='\\'; *p++='n'; break;
            case '\r': *p++='\\'; *p++='r'; break;
            case '\t': *p++='\\'; *p++='t'; break;
            case '\b': *p++='\\'; *p++='b'; break;
            case '\f': *p++='\\'; *p++='f'; break;
            default:
                if (c < 0x20) { snprintf(p, 7, "\\u%04x", c); p += 6; }
                else *p++ = (char)c;
        }
    }
    *p++ = '"';
    *p = 0;
    return out;
}

static char *gate_result_staged(memory_write_gate_decision_t *d) {
    char *pid = json_dumps_str(d->pending_id ? d->pending_id : "");
    char *msg = json_dumps_str(d->message ? d->message : "");
    char *out = malloc(512);
    snprintf(out, 512,
        "{\"success\":true,\"staged\":true,\"pending_id\":%s,\"message\":%s}", pid, msg);
    free(pid); free(msg);
    return out;
}

/* PoP: memory_tool_available @ tools/memory_tool.py:check_memory_requirements */
int memory_tool_available(void) { return 1; }

/* PoP: memory_tool_missing_old_text_error @ tools/memory_tool.py:_missing_old_text_error */
char *memory_tool_missing_old_text_error(memory_store_t *store,
                                         const char *target, const char *action) {
    int n; const char **e = entries_for(store, target, &n);
    char *entries_json = malloc(1); entries_json[0]=0;
    for (int i=0;i<n;i++){
        size_t a=(i?2:0)+strlen(e[i])+8; size_t need=strlen(entries_json)+a+1;
        entries_json=realloc(entries_json,need);
        if(i) strcat(entries_json,",");
        strcat(entries_json,"\""); strcat(entries_json,e[i]); strcat(entries_json,"\"");
    }
    int cur = memory_store_char_count(store, target);
    int lim = memory_store_char_limit(store, target);
    char *out = malloc(1024);
    snprintf(out, 1024,
        "{\"success\":false,\"error\":\"'%s' needs old_text -- a short unique substring of the entry to %s. None was provided. Reissue the %s with old_text set to part of one of the current_entries below.\",\"current_entries\":[%s],\"usage\":\"%d/%d\"}",
        action?action:"", action?action:"", action?action:"", entries_json, cur, lim);
    free(entries_json);
    return out;
}

/* Evaluate the single-op write gate. Returns a malloc'd JSON result (caller
 * frees) and sets *handled=1 when the write should NOT proceed; returns NULL
 * and *handled=0 when the caller should perform the real write. */
static char *eval_write_gate(memory_store_t *store, const char *target,
                             const char *summary, const char *detail,
                             int *handled) {
    (void)summary;
    *handled = 0;
    if (!store->gate) return NULL;
    memory_write_gate_decision_t d = store->gate(target, detail);
    if (d.allow) { memory_store_free_gate_decision(&d); return NULL; }
    *handled = 1;
    if (d.blocked) {
        char *out = tool_error_json(d.message ? d.message : "Write blocked.");
        memory_store_free_gate_decision(&d);
        return out;
    }
    char *out = gate_result_staged(&d);
    memory_store_free_gate_decision(&d);
    return out;
}

char *memory_tool_run(memory_store_t *store, const char *action,
                      const char *target, const char *content,
                      const char *old_text, const json_node_t *operations) {
    if (!store)
        return tool_error_json("Memory is not available. It may be disabled in config or this environment.");
    if (!target || (strcmp(target,"memory")!=0 && strcmp(target,"user")!=0))
        return tool_error_json("Invalid target. Use 'memory' or 'user'.");

    /* --- Batch path --- */
    if (operations) {
        if (!json_node_is_array(operations)) {
            return tool_error_json("operations must be a list of {action, content?, old_text?} objects.");
        }
        int opn = (int)json_array_size(operations);
        char *detail = malloc(1); detail[0]=0; size_t dl=0;
        for (int i=0;i<opn;i++){
            const json_node_t *op = json_array_get(operations, i);
            const char *a = json_object_get_string(op,"action","");
            const char *c = json_object_get_string(op,"content","");
            const char *o = json_object_get_string(op,"old_text","");
            char line[1024];
            if (strcmp(a,"remove")==0) snprintf(line,sizeof(line),"- remove: %s\n", o);
            else if (strcmp(a,"replace")==0) snprintf(line,sizeof(line),"- replace: %s -> %s\n", o, c);
            else snprintf(line,sizeof(line),"- %s: %s\n", a, c);
            size_t la=strlen(line);
            detail=realloc(detail,dl+la+1); memcpy(detail+dl,line,la); dl+=la; detail[dl]=0;
        }
        int handled;
        char *gr = eval_write_gate(store, target, "apply batch", detail, &handled);
        free(detail);
        if (handled) return gr;
        return memory_store_apply_batch(store, target, operations);
    }

    /* --- Single-op path: validate required params BEFORE the gate --- */
    if (action && strcmp(action,"add")==0 && (!content || !*content))
        return tool_error_json("Content is required for 'add' action.");
    if (action && strcmp(action,"replace")==0) {
        if (!old_text || !*old_text)
            return memory_tool_missing_old_text_error(store, target, "replace");
        if (!content || !*content)
            return tool_error_json("content is required for 'replace' action.");
    }
    if (action && strcmp(action,"remove")==0 && (!old_text || !*old_text))
        return memory_tool_missing_old_text_error(store, target, "remove");

    /* write gate */
    {
        const char *label = (strcmp(target,"user")==0) ? "user profile" : "memory";
        char summary[256];
        char *detail = NULL;
        if (action && strcmp(action,"add")==0) {
            snprintf(summary,sizeof(summary),"add to %s", label);
            detail = json_dumps_str(content ? content : "");
        } else if (action && strcmp(action,"replace")==0) {
            snprintf(summary,sizeof(summary),"replace in %s", label);
            char *o = json_dumps_str(old_text ? old_text : "");
            char *c = json_dumps_str(content ? content : "");
            size_t L = strlen("old: \nnew: ") + strlen(o) + strlen(c) + 1;
            detail = malloc(L); snprintf(detail,L,"old: %s\nnew: %s", o, c);
            free(o); free(c);
        } else {
            snprintf(summary,sizeof(summary),"remove from %s", label);
            detail = json_dumps_str(old_text ? old_text : "");
        }
        int handled;
        char *gr = eval_write_gate(store, target, summary, detail, &handled);
        free(detail);
        if (handled) return gr;
    }

    if (action && strcmp(action,"add")==0)      return memory_store_add(store, target, content);
    if (action && strcmp(action,"replace")==0)  return memory_store_replace(store, target, old_text, content);
    if (action && strcmp(action,"remove")==0)   return memory_store_remove(store, target, old_text);
    return tool_error_json("Unknown action. Use: add, replace, remove");
}

char *memory_tool_apply_pending(memory_store_t *store, const json_node_t *payload) {
    if (!store) return tool_error_json("Memory is not available.");
    if (!payload) return tool_error_json("Missing staged payload.");
    const char *action = json_object_get_string(payload, "action", "");
    const char *target = json_object_get_string(payload, "target", "memory");
    const char *content = json_object_get_string(payload, "content", "");
    const char *old_text = json_object_get_string(payload, "old_text", "");
    if (strcmp(action,"batch")==0) {
        const json_node_t *ops = json_object_get(payload, "operations");
        return memory_store_apply_batch(store, target, ops);
    }
    if (strcmp(action,"add")==0)     return memory_store_add(store, target, content);
    if (strcmp(action,"replace")==0) return memory_store_replace(store, target, old_text, content);
    if (strcmp(action,"remove")==0)  return memory_store_remove(store, target, old_text);
    return tool_error_json("Unknown staged action.");
}

/* ---- live-tool wiring (singleton store + gate seam) -----------------
 * The "memory" tool is a live, persistent tool: the store is loaded once from
 * <HERMES_HOME>/memories and reused across calls (faithful to Python's
 * injected `store=kw.get("store")`). The write gate is an injectable seam so
 * this module stays self-contained (see tool_init.c for the real adapter). */
static memory_store_t *g_memory_store = NULL;

memory_store_t *memory_tool_get_store(void) { return g_memory_store; }

void memory_tool_set_store(memory_store_t *s) { g_memory_store = s; }

void memory_tool_set_gate(memory_store_write_gate_t gate) {
    if (g_memory_store) memory_store_set_write_gate(g_memory_store, gate);
}

/* ---- live "memory" tool registration (tools/memory_tool.py) ----------
 * The singleton store is loaded once from <HERMES_HOME>/memories and reused
 * across calls (mirrors Python's injected store). The write gate is attached
 * by the wiring layer (tool_init.c) via memory_tool_set_gate(); default is
 * fail-open, matching Python's lazy-import gate. */
static char *memory_tool_bridge(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!g_memory_store)
        return strdup("{\"success\":false,\"error\":\"Memory is not available. It may be disabled in config or this environment.\"}");
    if (!args_json)
        return strdup("{\"success\":false,\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"success\":false,\"error\":\"JSON parse error\"}"); }

    const char *action   = json_object_get_string(args, "action", "");
    const char *target   = json_object_get_string(args, "target", "memory");
    const char *content  = json_object_get_string(args, "content", "");
    const char *old_text = json_object_get_string(args, "old_text", "");
    const json_node_t *operations = json_object_get(args, "operations");

    char *result = memory_tool_run(g_memory_store, action, target, content, old_text, operations);
    json_free(args);
    return result;
}

void registry_init_memory(void) {
    if (!g_memory_store) {
        const char *home = getenv("HERMES_HOME");
        char memdir[HERMES_PATH_MAX];
        if (!home || !*home) home = "~/.hermes";
        snprintf(memdir, sizeof(memdir), "%s/memories", home);
        g_memory_store = memory_store_new(0, 0);
        memory_store_load(g_memory_store, memdir);
        memory_tool_set_store(g_memory_store);
    }
    registry_register("memory",
        "Save durable facts to persistent memory that survive across sessions. Memory is "
        "injected into every future turn, so keep entries compact and high-signal.\n\n"
        "HOW: make ALL your changes in ONE call via an 'operations' array (each item: "
        "{action, content?, old_text?}). The batch applies atomically and the char limit is "
        "checked only on the FINAL result — so a single call can remove/replace stale entries "
        "to free room AND add new ones, even when an add alone would overflow. The response "
        "reports current/limit chars and confirms completion; one batch call finishes the "
        "update, so don't repeat it. Use the bare action/content/old_text fields only for a "
        "single lone change.\n\n"
        "WHEN: save proactively when the user states a preference, correction, or personal "
        "detail, or you learn a stable fact about their environment, conventions, or workflow. "
        "Priority: user preferences & corrections > environment facts > procedures. The best "
        "memory stops the user repeating themselves.\n\n"
        "IF FULL: an add is rejected with the current entries shown. Reissue as ONE batch that "
        "removes or shortens enough stale entries and adds the new one together.\n\n"
        "TARGETS: 'user' = who the user is (name, role, preferences, style). 'memory' = your "
        "notes (environment, conventions, tool quirks, lessons).\n\n"
        "SKIP: trivial/obvious info, easily re-discovered facts, raw data dumps, task progress, "
        "completed-work logs, temporary TODO state (use session_search for those). Reusable "
        "procedures belong in a skill, not memory.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"action\":{\"type\":\"string\",\"enum\":[\"add\",\"replace\",\"remove\"],\"description\":\"The action to perform (single-op shape). Omit when using 'operations'.\"},"
          "\"target\":{\"type\":\"string\",\"enum\":[\"memory\",\"user\"],\"description\":\"Which memory store: 'memory' for personal notes, 'user' for user profile.\"},"
          "\"content\":{\"type\":\"string\",\"description\":\"The entry content. Required for 'add' and 'replace' (single-op shape).\"},"
          "\"old_text\":{\"type\":\"string\",\"description\":\"REQUIRED for 'replace' and 'remove' (single-op shape): a short unique substring identifying the existing entry to modify. Omit only for 'add'.\"},"
          "\"operations\":{\"type\":\"array\",\"description\":\"Batch shape: a list of operations applied atomically in one call against the final char budget. Preferred when making multiple changes or consolidating to make room. Each item is {action, content?, old_text?}.\","
            "\"items\":{\"type\":\"object\",\"properties\":{"
              "\"action\":{\"type\":\"string\",\"enum\":[\"add\",\"replace\",\"remove\"]},"
              "\"content\":{\"type\":\"string\",\"description\":\"Entry content for add/replace.\"},"
              "\"old_text\":{\"type\":\"string\",\"description\":\"Substring identifying the entry for replace/remove.\"}},"
              "\"required\":[\"action\"]}}"
        "},"
        "\"required\":[\"target\"]"
        "}", memory_tool_bridge);
}

/* =====================================================================
 * REAL_GAP closure batch — faithful ports of the remaining
 * tools/memory_tool.py functions (v773 gap wave).
 *
 * Includes: _scan_memory_content, target_enabled, load_from_disk,
 * _sanitize_entries_for_snapshot, _file_lock/_reload_target,
 * _set_entries, _batch_error, format_for_system_prompt, _previews,
 * _read_raw_checked, _parse_entries, _read_entries_checked,
 * load_on_disk_store, _apply_write_gate, _apply_batch_write_gate,
 * memory_tool, _memory_target_error, apply_memory_pending.
 * ===================================================================== */

#include <stdbool.h>
#include <sys/file.h>

/* externs from memory_tool_pure.c (same module family) */
char *memory_tool_get_memory_dir(void);
json_t *memory_tool_pure_get_builtin_memory_config(const json_t *config);
void memory_tool_pure_get_builtin_memory_store_flags(const json_t *config,
                                                     bool *memory_enabled,
                                                     bool *user_profile_enabled);
/* threat_patterns.h lives in include/ */
char *cli_tools_threat_patterns_first_threat_message(const char *content, const char *scope);

/* ---- _scan_memory_content ------------------------------------------ */
/* PoP: memory_tool_scan_memory_content @ tools/memory_tool.py:_scan_memory_content */
char *memory_tool_scan_memory_content(const char *content) {
    /* Python: return _first_threat_message(content, scope="strict") */
    if (!content) return NULL;
    return cli_tools_threat_patterns_first_threat_message(content, "strict");
}

/* ---- target_enabled -------------------------------------------------- */
/* PoP: memory_store_target_enabled @ tools/memory_tool.py:target_enabled */
int memory_store_target_enabled(memory_store_t *s, const char *target) {
    if (!s) return 0;
    return (target && strcmp(target, "user") == 0)
        ? s->user_profile_enabled : s->memory_enabled;
}

/* ---- _previews ------------------------------------------------------- */
/* Truncated one-line previews of entries for error feedback. Returns a
 * malloc'd array of malloc'd strings (free with free_entries-style loop). */
/* PoP: memory_store_previews @ tools/memory_tool.py:_previews */
char **memory_store_previews(const char **entries, int n, int width, int *out_n) {
    if (width <= 0) width = PREVIEW_WIDTH;
    char **out = malloc(sizeof(char *) * (n > 0 ? n : 1));
    for (int i = 0; i < n; i++) {
        size_t len = strlen(entries[i]);
        if ((int)len <= width) {
            out[i] = strdup(entries[i]);
        } else {
            out[i] = malloc((size_t)width + 4);
            memcpy(out[i], entries[i], (size_t)width);
            memcpy(out[i] + width, "...", 4);
        }
    }
    *out_n = n;
    return out;
}

/* ---- format_for_system_prompt ---------------------------------------- */
/* Frozen snapshot for system prompt injection; NULL when empty. */
/* PoP: memory_store_format_for_system_prompt @ tools/memory_tool.py:format_for_system_prompt */
const char *memory_store_format_for_system_prompt(memory_store_t *s, const char *target) {
    const char *block = memory_store_snapshot(s, target);
    return (block && *block) ? block : NULL;
}

/* ---- _read_raw_checked ----------------------------------------------- */
/* Read a memory file's raw text, distinguishing unreadable from empty.
 * Returns malloc'd raw text ("" for absent file) and sets *read_ok to 0 ONLY
 * when the file exists but could not be read. A leading UTF-8 BOM is stripped
 * (utf-8-sig semantics). */
/* PoP: memory_store_read_raw_checked @ tools/memory_tool.py:_read_raw_checked */
char *memory_store_read_raw_checked(const char *path, int *read_ok) {
    *read_ok = 1;
    struct stat st;
    if (stat(path, &st) != 0) return strdup("");   /* absent => ("", True) */
    FILE *f = fopen(path, "rb");
    if (!f) { *read_ok = 0; return strdup(""); }
    char *raw = malloc((size_t)st.st_size + 1);
    if (st.st_size > 0 && fread(raw, 1, (size_t)st.st_size, f) != (size_t)st.st_size) {
        fclose(f); free(raw); *read_ok = 0; return strdup("");
    }
    fclose(f);
    raw[st.st_size] = 0;
    /* utf-8-sig: strip leading EF BB BB */
    unsigned char *p = (unsigned char *)raw;
    if (st.st_size >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF)
        memmove(raw, raw + 3, (size_t)st.st_size - 2);
    return raw;
}

/* ---- _parse_entries ---------------------------------------------------- */
/* Split raw memory-file text into stripped, non-empty entries. Caller frees. */
/* PoP: memory_store_parse_entries @ tools/memory_tool.py:_parse_entries */
char **memory_store_parse_entries(const char *raw, int *out_n) {
    *out_n = 0;
    /* skip if whole input is whitespace-only */
    const char *q = raw;
    while (*q && isspace((unsigned char)*q)) q++;
    if (!*q) return malloc(sizeof(char *));
    char **arr = NULL; int n = 0, cap = 0;
    const char *cur = raw;
    while (1) {
        const char *nl = strstr(cur, ENTRY_DELIMITER);
        const char *seg = cur; size_t seglen;
        if (nl) { seglen = (size_t)(nl - cur); cur = nl + strlen(ENTRY_DELIMITER); }
        else { seglen = strlen(cur); cur = NULL; }
        /* strip whitespace both ends */
        while (seglen && isspace((unsigned char)*seg)) { seg++; seglen--; }
        while (seglen && isspace((unsigned char)seg[seglen - 1])) seglen--;
        if (seglen) {
            if (n >= cap) { cap = cap ? cap * 2 : 8; arr = realloc(arr, sizeof(char *) * cap); }
            arr[n] = malloc(seglen + 1);
            memcpy(arr[n], seg, seglen); arr[n][seglen] = 0;
            n++;
        }
        if (!cur) break;
    }
    *out_n = n;
    return arr ? arr : malloc(sizeof(char *));
}

/* dedupe preserving order, keeping first occurrence (list(dict.fromkeys())). */
static char **dedup_entry_list(char **arr, int n, int *out_n) {
    int m = 0;
    for (int i = 0; i < n; i++) {
        int seen = 0;
        for (int j = 0; j < m; j++) if (strcmp(arr[j], arr[i]) == 0) { seen = 1; break; }
        if (seen) free(arr[i]);
        else arr[m++] = arr[i];
    }
    *out_n = m;
    return arr;
}

/* ---- _read_entries_checked --------------------------------------------- */
/* PoP: memory_store_read_entries_checked @ tools/memory_tool.py:_read_entries_checked */
char **memory_store_read_entries_checked(const char *path, int *out_n, int *read_ok) {
    int ok;
    char *raw = memory_store_read_raw_checked(path, &ok);
    if (!ok) { *read_ok = 0; *out_n = 0; free(raw); return malloc(sizeof(char *)); }
    *read_ok = 1;
    char **arr = memory_store_parse_entries(raw, out_n);
    free(raw);
    return arr;
}

/* ---- _sanitize_entries_for_snapshot -------------------------------------- */
/* Replace any threat-matching entry with a [BLOCKED: ...] placeholder for the
 * system-prompt snapshot only. Returns a new malloc'd array. */
/* PoP: memory_store_sanitize_entries_for_snapshot @ tools/memory_tool.py:_sanitize_entries_for_snapshot */
char **memory_store_sanitize_entries_for_snapshot(const char **entries, int n,
                                                  const char *filename, int *out_n) {
    char **out = malloc(sizeof(char *) * (n > 0 ? n : 1));
    for (int i = 0; i < n; i++) {
        const char *e = entries[i];
        if (!e || !*e || strncmp(e, "[BLOCKED:", 9) == 0) {
            out[i] = strdup(e ? e : "");
            continue;
        }
        char *msg = cli_tools_threat_patterns_first_threat_message(e, "strict");
        if (msg) {
            char buf[1024];
            snprintf(buf, sizeof(buf),
                     "[BLOCKED: %s entry contained threat pattern(s): %s. "
                     "Removed from system prompt; use memory(action=remove) "
                     "to delete the original.]",
                     filename ? filename : "MEMORY.md", msg);
            out[i] = strdup(buf);
            free(msg);
        } else {
            out[i] = strdup(e);
        }
    }
    *out_n = n;
    return out;
}

/* ---- _file_lock / release ------------------------------------------------ */
/* Acquire an exclusive lock on "<path>.lock" for read-modify-write safety.
 * Returns the lock fd (>= 0), or -1 when locking is unavailable (yield). */
/* PoP: memory_store_file_lock @ tools/memory_tool.py:_file_lock */
int memory_store_file_lock(const char *path) {
    char lock_path[2048];
    snprintf(lock_path, sizeof(lock_path), "%s.lock", path);
    int fd = open(lock_path, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd < 0) return -1;
    if (flock(fd, LOCK_EX) != 0) { close(fd); return -1; }
    return fd;
}
void memory_store_file_unlock(int fd) {
    if (fd < 0) return;
    flock(fd, LOCK_UN);
    close(fd);
}

/* ---- drift detection on an already-read raw snapshot ---------------------- */
/* Mirrors _detect_external_drift but operates on the caller's raw text (one
 * read, one snapshot — no re-read window). Returns a malloc'd .bak path or
 * NULL. Writes raw to "<path>.bak.<ts>" when drift confirmed. */
static char *drift_check_raw(memory_store_t *s, const char *target, const char *raw) {
    const char *path = path_for(s->mem_dir, target);
    const char *q = raw;
    while (*q && isspace((unsigned char)*q)) q++;
    if (!*q) return NULL;
    int pn = 0;
    char **parsed = memory_store_parse_entries(raw, &pn);
    /* roundtrip = ENTRY_DELIMITER.join(parsed) */
    char *rt = malloc(1); rt[0] = 0; size_t rl = 0;
    for (int i = 0; i < pn; i++) {
        size_t a = (i ? strlen(ENTRY_DELIMITER) : 0) + strlen(parsed[i]);
        rt = realloc(rt, rl + a + 1);
        if (i) { memcpy(rt + rl, ENTRY_DELIMITER, strlen(ENTRY_DELIMITER)); rl += strlen(ENTRY_DELIMITER); }
        memcpy(rt + rl, parsed[i], strlen(parsed[i])); rl += strlen(parsed[i]);
        rt[rl] = 0;
    }
    /* strip whole-raw like Python's raw.strip() comparison */
    char *stripped = strdup(q);
    char *se = stripped + strlen(stripped);
    while (se > stripped && isspace((unsigned char)*(se - 1))) *--se = 0;
    int limit = char_limit_for(s, target);
    long max_entry_len = 0;
    for (int i = 0; i < pn; i++)
        if ((long)strlen(parsed[i]) > max_entry_len) max_entry_len = (long)strlen(parsed[i]);
    int drift = (strcmp(stripped, rt) != 0) || (max_entry_len > limit);
    free(rt); free(stripped);
    for (int i = 0; i < pn; i++) free(parsed[i]);
    free(parsed);
    if (!drift) return NULL;
    char bak_path[2048];
    snprintf(bak_path, sizeof(bak_path), "%s.bak.%ld", path, (long)time(NULL));
    FILE *bf = fopen(bak_path, "wb");
    if (!bf) {
        char *fail = malloc(strlen(bak_path) + 64);
        sprintf(fail, "%s (BACKUP FAILED — file unchanged on disk)", bak_path);
        return fail;
    }
    fwrite(raw, 1, strlen(raw), bf);
    fclose(bf);
    return strdup(bak_path);
}

/* ---- _set_entries ---------------------------------------------------------- */
/* PoP: memory_store_set_entries @ tools/memory_tool.py:_set_entries */
void memory_store_set_entries(memory_store_t *s, const char *target,
                              char **entries, int n) {
    if (!s) return;
    if (target && strcmp(target, "user") == 0) {
        free_entries(s->user_entries, s->user_n);
        s->user_entries = entries; s->user_n = n;
    } else {
        free_entries(s->memory_entries, s->memory_n);
        s->memory_entries = entries; s->memory_n = n;
    }
}

/* reload status: mirrors Python returning None (clean), a bak path string,
 * or the _READ_FAILED sentinel. */
#define MEMORY_RELOAD_OK      0
#define MEMORY_RELOAD_FAILED  1   /* file exists but unreadable (_READ_FAILED) */

/* ---- _reload_target ---------------------------------------------------------- */
/* Re-read entries from disk into in-memory state under the caller's lock.
 * Outputs: *bak_out receives a malloc'd drift backup path or NULL
 * (unless skip_drift). Returns MEMORY_RELOAD_FAILED when the file exists
 * but could not be read — callers MUST abort rather than persist. */
/* PoP: memory_store_reload_target @ tools/memory_tool.py:_reload_target */
int memory_store_reload_target(memory_store_t *s, const char *target,
                               int skip_drift, char **bak_out) {
    *bak_out = NULL;
    const char *path = path_for(s->mem_dir, target);
    int read_ok;
    char *raw = memory_store_read_raw_checked(path, &read_ok);
    if (!read_ok) { free(raw); return MEMORY_RELOAD_FAILED; }
    if (!skip_drift)
        *bak_out = drift_check_raw(s, target, raw);
    int n;
    char **fresh = memory_store_parse_entries(raw, &n);
    fresh = dedup_entry_list(fresh, n, &n);
    memory_store_set_entries(s, target, fresh, n);
    free(raw);
    return MEMORY_RELOAD_OK;
}

/* ---- load_from_disk ------------------------------------------------------------ */
/* Load entries from MEMORY.md and USER.md under get_memory_dir(), capture the
 * frozen (sanitized) system-prompt snapshot. */
/* PoP: memory_store_load_from_disk @ tools/memory_tool.py:load_from_disk */
void memory_store_load_from_disk(memory_store_t *s) {
    char *dir = memory_tool_get_memory_dir();
    memory_store_load(s, dir);
    free(dir);
    /* Rebuild each frozen snapshot from SANITIZED copies of the live entries
     * (live state keeps raw text so the user can see + remove poison). */
    const char *names[2] = {"memory", "user"};
    const char *files[2] = {"MEMORY.md", "USER.md"};
    char **snap_out[2] = {&s->snap_memory, &s->snap_user};
    for (int t = 0; t < 2; t++) {
        int n;
        const char **e = entries_for(s, names[t], &n);
        int sn;
        char **san = memory_store_sanitize_entries_for_snapshot(e, n, files[t], &sn);
        char *c = malloc(1); c[0] = 0; size_t cl = 0;
        for (int i = 0; i < sn; i++) {
            size_t a = (i ? strlen(ENTRY_DELIMITER) : 0) + strlen(san[i]);
            c = realloc(c, cl + a + 1);
            if (i) { memcpy(c + cl, ENTRY_DELIMITER, strlen(ENTRY_DELIMITER)); cl += strlen(ENTRY_DELIMITER); }
            memcpy(c + cl, san[i], strlen(san[i])); cl += strlen(san[i]);
            c[cl] = 0;
        }
        for (int i = 0; i < sn; i++) free(san[i]);
        free(san);
        free(*snap_out[t]);
        *snap_out[t] = c;
    }
}

/* ---- load_on_disk_store ----------------------------------------------------------- */
/* Build a fresh on-disk store honoring configured char limits; falls back to
 * the built-in defaults if config can't be loaded. Never fails. */
/* PoP: memory_tool_load_on_disk_store @ tools/memory_tool.py:load_on_disk_store */
memory_store_t *memory_tool_load_on_disk_store(void) {
    int memory_char_limit = DEFAULT_MEMORY_LIMIT;
    int user_char_limit = DEFAULT_USER_LIMIT;
    bool memory_enabled = true, user_profile_enabled = true;
    json_t *cfg = memory_tool_pure_get_builtin_memory_config(NULL);
    memory_tool_pure_get_builtin_memory_store_flags(cfg, &memory_enabled, &user_profile_enabled);
    /* int(mem_cfg.get("memory_char_limit", ...)) */
    json_node_t *section = json_object_get((const json_node_t *)cfg, "memory");
    (void)section;
    /* limits come straight off the normalized section via the pure helpers'
     * contract; read them defensively from the config object */
    {
        const json_node_t *mnode = json_object_get((const json_node_t *)cfg, "memory");
        if (mnode) {
            double v = json_object_get_number(mnode, "memory_char_limit", -1.0);
            if (v > 0) memory_char_limit = (int)v;
            v = json_object_get_number(mnode, "user_char_limit", -1.0);
            if (v > 0) user_char_limit = (int)v;
        }
    }
    json_free(cfg);
    memory_store_t *store = memory_store_new(memory_char_limit, user_char_limit);
    store->memory_enabled = memory_enabled ? 1 : 0;
    store->user_profile_enabled = user_profile_enabled ? 1 : 0;
    memory_store_load_from_disk(store);
    return store;
}

/* ---- _batch_error ------------------------------------------------------------------- */
/* Batch-abort error that reports live (uncommitted) state. */
/* PoP: memory_store_batch_error @ tools/memory_tool.py:_batch_error */
char *memory_store_batch_error(memory_store_t *s, const char *target, const char *message) {
    char full[2048];
    snprintf(full, sizeof(full),
             "%s No operations were applied (batch is all-or-nothing).",
             message ? message : "");
    return consolidation_failure(s, target, resp_error(s, target, full, 0));
}

/* ---- _memory_target_error -------------------------------------------------------------- */
/* Shared validation error for an invalid or disabled target. Returns a
 * malloc'd JSON error dict, or NULL when the target is fine. */
/* PoP: memory_tool_memory_target_error @ tools/memory_tool.py:_memory_target_error */
char *memory_tool_memory_target_error(memory_store_t *store, const char *target) {
    if (!target || (strcmp(target, "memory") != 0 && strcmp(target, "user") != 0)) {
        char *out = malloc(256);
        snprintf(out, 256,
                 "{\"success\":false,\"error\":\"Invalid memory target '%s'. Use 'memory' or 'user'.\"}",
                 target ? target : "");
        return out;
    }
    if (memory_store_target_enabled(store, target)) return NULL;
    const char *label = (strcmp(target, "user") == 0) ? "USER.md" : "MEMORY.md";
    char *out = malloc(256);
    snprintf(out, 256,
             "{\"success\":false,\"error\":\"Built-in %s writes are disabled in memory config.\",\"target\":\"%s\"}",
             label, target);
    return out;
}

/* ---- _apply_write_gate ------------------------------------------------------------------- */
/* Evaluate the single-op write gate. Returns a malloc'd JSON tool-result when
 * the write should NOT proceed normally (*handled=1), or NULL (*handled=0). */
/* PoP: memory_tool_apply_write_gate @ tools/memory_tool.py:_apply_write_gate */
char *memory_tool_apply_write_gate(memory_store_t *store, const char *action,
                                   const char *target, const char *content,
                                   const char *old_text, int *handled) {
    *handled = 0;
    if (!action || (strcmp(action, "add") != 0 && strcmp(action, "replace") != 0 &&
                    strcmp(action, "remove") != 0))
        return NULL;   /* only mutating actions are gated */
    if (!store || !store->gate) return NULL;   /* gate module unavailable -> fail open */
    const char *label = (target && strcmp(target, "user") == 0) ? "user profile" : "memory";
    char summary[128];
    char detail[4096];
    if (strcmp(action, "add") == 0) {
        snprintf(summary, sizeof(summary), "add to %s", label);
        snprintf(detail, sizeof(detail), "%s", content ? content : "");
    } else if (strcmp(action, "replace") == 0) {
        snprintf(summary, sizeof(summary), "replace in %s", label);
        snprintf(detail, sizeof(detail), "old: %s\nnew: %s",
                 old_text ? old_text : "", content ? content : "");
    } else {
        snprintf(summary, sizeof(summary), "remove from %s", label);
        snprintf(detail, sizeof(detail), "%s", old_text ? old_text : "");
    }
    return eval_write_gate(store, target, summary, detail, handled);
}

/* ---- _apply_batch_write_gate ---------------------------------------------------------------- */
/* Gate a whole batch as a single unit. Same contract as the single-op gate. */
/* PoP: memory_tool_apply_batch_write_gate @ tools/memory_tool.py:_apply_batch_write_gate */
char *memory_tool_apply_batch_write_gate(memory_store_t *store, const char *target,
                                         const json_node_t *operations, int *handled) {
    *handled = 0;
    if (!store || !store->gate) return NULL;
    if (!operations || !json_node_is_array(operations)) return NULL;
    int opn = (int)json_array_size(operations);
    const char *label = (target && strcmp(target, "user") == 0) ? "user profile" : "memory";
    char summary[160];
    snprintf(summary, sizeof(summary), "apply %d op(s) to %s", opn, label);
    char *detail = malloc(1); detail[0] = 0; size_t dl = 0;
    for (int i = 0; i < opn; i++) {
        const json_node_t *op = json_array_get(operations, i);
        const char *act = json_object_get_string(op, "action", "?");
        const char *content = json_object_get_string(op, "content", "");
        if (!content || !*content) content = json_object_get_string(op, "new_text", "");
        const char *old_text = json_object_get_string(op, "old_text", "");
        char line[1024];
        if (strcmp(act, "remove") == 0)
            snprintf(line, sizeof(line), "- remove: %s\n", old_text);
        else if (strcmp(act, "replace") == 0)
            snprintf(line, sizeof(line), "- replace: %s -> %s\n", old_text, content);
        else
            snprintf(line, sizeof(line), "- %s: %s\n", act, content);
        size_t la = strlen(line);
        detail = realloc(detail, dl + la + 1);
        memcpy(detail + dl, line, la); dl += la; detail[dl] = 0;
    }
    char *gr = eval_write_gate(store, target, summary, detail, handled);
    free(detail);
    return gr;
}

/* ---- memory_tool (single entry point dispatcher) ------------------------------- */
/* Thin annotated wrapper over memory_tool_run adding the new_text alias and
 * target:null handling exactly as the Python dispatcher does. */
/* PoP: memory_tool @ tools/memory_tool.py:memory_tool */
char *memory_tool(memory_store_t *store, const char *action, const char *target,
                  const char *content, const char *old_text, const char *new_text,
                  const json_node_t *operations) {
    if (target == NULL) target = "memory";          /* null treated as omitted */
    if (content == NULL && new_text != NULL) content = new_text;  /* new_text alias */
    return memory_tool_run(store, action, target, content, old_text, operations);
}

/* ---- _build_memory_schema_overrides ---------------------------------- */
/* Narrow the advertised target surface using the availability snapshot.
 * Returns a malloc'd JSON string {"description":..., "parameters":...} with
 * the target enum narrowed to the enabled stores (and the TARGETS paragraph
 * rewritten for single-store configurations). */
/* PoP: memory_tool_build_memory_schema_overrides @ tools/memory_tool.py:_build_memory_schema_overrides */
char *memory_tool_build_memory_schema_overrides(void) {
    bool memory_enabled = true, user_profile_enabled = true;
    json_t *cfg = memory_tool_pure_get_builtin_memory_config(NULL);
    memory_tool_pure_get_builtin_memory_store_flags(cfg, &memory_enabled, &user_profile_enabled);
    json_free(cfg);

    /* targets list */
    char targets[32] = "";
    if (memory_enabled) strcat(targets, "\"memory\"");
    if (user_profile_enabled) {
        if (targets[0]) strcat(targets, ",");
        strcat(targets, "\"user\"");
    }

    /* description rewrite for single-store configs */
    const char *desc = NULL;   /* default: registry description as registered */
    char desc_buf[4096];
    if (memory_enabled && !user_profile_enabled) {
        desc = "TARGET: only 'memory' is enabled for personal notes "
               "(environment, conventions, tool quirks, lessons).";
    } else if (!memory_enabled && user_profile_enabled) {
        desc = "TARGET: only 'user' is enabled for user profile facts "
               "(name, role, preferences, style).";
    }
    /* target schema description for single-store configs */
    const char *target_desc = "Which memory store: 'memory' for personal notes, 'user' for user profile.";
    if (memory_enabled && !user_profile_enabled)
        target_desc = "The enabled built-in store: 'memory' for personal notes.";
    else if (!memory_enabled && user_profile_enabled)
        target_desc = "The enabled built-in store: 'user' for user profile.";

    snprintf(desc_buf, sizeof(desc_buf),
             "%s\\n\\n%s",
             "Save durable facts to persistent memory that survive across sessions.",
             desc ? desc : "");
    (void)desc;

    char *out = malloc(2048);
    snprintf(out, 2048,
             "{\"description\":\"%s\",\"parameters\":{\"type\":\"object\","
             "\"properties\":{"
             "\"action\":{\"type\":\"string\",\"enum\":[\"add\",\"replace\",\"remove\"]},"
             "\"target\":{\"type\":\"string\",\"enum\":[%s],\"description\":\"%s\"},"
             "\"content\":{\"type\":\"string\"},"
             "\"old_text\":{\"type\":\"string\"},"
             "\"operations\":{\"type\":\"array\",\"items\":{\"type\":\"object\","
             "\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"add\",\"replace\",\"remove\"]},"
             "\"content\":{\"type\":\"string\"},\"old_text\":{\"type\":\"string\"}},"
             "\"required\":[\"action\"]}}},"
             "\"required\":[\"target\"]}}",
             desc_buf, targets, target_desc);
    return out;
}

/* ---- apply_memory_pending --------------------------------------------------------- */
/* Replay a staged memory write directly against the store, bypassing the
 * write gate (/memory approve handler). */
/* PoP: memory_tool_apply_memory_pending @ tools/memory_tool.py:apply_memory_pending */
char *memory_tool_apply_memory_pending(memory_store_t *store, const json_node_t *payload) {
    if (!payload) return tool_error_json("Missing staged payload.");
    const char *action = json_object_get_string(payload, "action", "");
    (void)action;   /* dispatch below re-reads the payload */
    const char *target = json_object_get_string(payload, "target", "memory");
    char *terr = memory_tool_memory_target_error(store, target);
    if (terr) return terr;
    return memory_tool_apply_pending(store, payload);
}
