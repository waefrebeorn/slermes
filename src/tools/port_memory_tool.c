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

/* PoP: check_memory_requirements @ tools/memory_tool.py:check_memory_requirements */
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
