/*
 * PoP: w9_statuslock_ce2ce2_unusable              @ gateway/status_lock_dir.py:unusable_lock_dir
 * PoP: w9_statuslock_ce2ce2_recovered             @ gateway/status_lock_dir.py:unusable_lock_dir_recovered
 * PoP: w9_statuslock_ce2ce2_reset                 @ gateway/status_lock_dir.py:reset_unusable_lock_dir_reports
 * PoP: w9_statuslock_ce2ce2_reported              @ gateway/status_lock_dir.py:unusable_lock_dir_reported
 *
 * The module-level `_warned` set is a process-global; the harness processes a
 * fixture's cases in order in one process, so the C set mirrors its lifecycle.
 */
#include "hermes_json.h"
#include "noc_w9_statuslock_ce2ce2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char **warned = NULL;
static size_t warned_n = 0, warned_cap = 0;

static long find_dir(const char *p){
    if(!p) return -1;
    for(size_t i=0;i<warned_n;i++) if(warned[i] && !strcmp(warned[i],p)) return (long)i;
    return -1;
}
static void add_dir(const char *p){
    if(!p || find_dir(p)>=0) return;
    if(warned_n==warned_cap){
        warned_cap = warned_cap?warned_cap*2:8;
        char **nw=(char**)realloc(warned,warned_cap*sizeof(char*));
        if(!nw) return;
        warned=nw;
    }
    warned[warned_n++]=strdup(p);
}
void w9_statuslock_ce2ce2_reset(void){
    for(size_t i=0;i<warned_n;i++) free(warned[i]);
    warned_n=0;
}
int w9_statuslock_ce2ce2_reported(const char *lock_dir){
    return find_dir(lock_dir)>=0;
}
void w9_statuslock_ce2ce2_recovered(const char *lock_dir){
    long i=find_dir(lock_dir);
    if(i<0) return;
    free(warned[i]);
    for(size_t j=(size_t)i+1;j<warned_n;j++) warned[j-1]=warned[j];
    warned_n--;
}
json_t *w9_statuslock_ce2ce2_unusable(const char *lock_dir, int err_num,
                                      const char *strerror_text, const char *filename){
    add_dir(lock_dir);
    char msg[2048];
    const char *fn = filename?filename:"";
    /* str(OSError) uses repr(filename): backslashes are doubled inside quotes. */
    char *frepr=(char*)malloc(strlen(fn)*2+1);
    if(frepr){ size_t k=0; for(const char *q=fn;*q;q++){ if(*q=='\\') frepr[k++]='\\'; frepr[k++]=*q; } frepr[k]=0; }
    snprintf(msg,sizeof(msg),"gateway lock directory is not usable: [Errno %d] %s: '%s'",
             err_num, strerror_text?strerror_text:"", frepr?frepr:fn);
    free(frepr);
    /* ScopedLockDirUnusable's `.filename` is str(Path(lock_dir)); on Windows
     * pathlib renders forward slashes as backslashes. */
    char *wpath=strdup(lock_dir?lock_dir:"");
    for(char *q=wpath;q&&*q;q++) if(*q=='/') *q='\\';
    json_t *o=json_object();
    json_set(o,"errno",json_number((double)err_num));
    json_set(o,"message",json_string(msg));
    json_set(o,"filename",json_string(wpath?wpath:""));
    json_set(o,"type",json_string("ScopedLockDirUnusable"));
    free(wpath);
    return o;
}
