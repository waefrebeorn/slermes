/* t_port_kanban_plugin_api_flow2.c — harness for the plugins/kanban/dashboard/
 * plugin_api.py FLOW port (src/plugins/kanban/dashboard/kanban_plugin_api_flow2.c).
 * Reads {"func","in"} JSONL from stdin; emits {"func","in","out":{ret,trace,state}}.
 * Seam scripts live in in.io; missing/exhausted -> RuntimeError (fail closed).
 */
#include "hermes_json.h"
#include "plugins/kanban/dashboard/kanban_plugin_api_flow2.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { json_t *io; json_t *cursors; } kp2_ctx_t;

static json_t *kp2_next(kp2_ctx_t *sc, const char *name) {
    if (!sc || !sc->io) return NULL;
    json_t *entry = json_obj_get(sc->io, name);
    if (!entry) return NULL;
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return NULL;
        json_set(sc->cursors, name, json_number(cur + 1));
        return json_get(entry, idx);
    }
    return entry;
}
static json_t *kp2_dispatch(kpa2_io_t *io, const char *name, json_t *args) {
    (void)args;
    kp2_ctx_t *sc = (kp2_ctx_t *)io->ctx;
    json_t *result = kp2_next(sc, name);
    if (!result) { char *m=kpa2_asprintf("no script for %s",name); json_t*e=kpa2_make_raise("RuntimeError",m); free(m); return e; }
    if (result->type == JSON_OBJECT) {
        json_t *r = json_obj_get(result,"raise");
        if (r && r->type==JSON_OBJECT) return kpa2_raise_from_spec(r);
        json_t *v = json_obj_get(result,"ret");
        return v ? json_copy(v) : json_null();
    }
    return json_copy(result);
}
static json_t *kp2_run_case(const char *func, json_t *in) {
    kpa2_io_t io; kp2_ctx_t sc;
    io.call=kp2_dispatch; io.trace=json_array();
    sc.io=json_obj_get(in,"io"); sc.cursors=json_object(); io.ctx=&sc;
    json_t *ret=kpa2_run(&io,func,in);
    json_t *out=json_object();
    json_set(out,"ret",ret?ret:json_null());
    json_set(out,"trace",json_copy(io.trace));
    json_set(out,"state",json_object());
    json_free(io.trace); json_free(sc.cursors);
    return out;
}
int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    char line[1<<20];
    while (fgets(line,sizeof line,stdin)) {
        if (line[0]!='{') continue;
        char *err=NULL; json_t *rec=json_parse(line,&err);
        if (!rec) { free(err); continue; }
        const char *func=json_get_str(rec,"func","");
        json_t *in=json_obj_get(rec,"in"); if(!in) in=json_object();
        json_t *out=kp2_run_case(func,in);
        json_t *lo=json_object();
        json_set(lo,"func",json_string(func));
        json_set(lo,"in",json_copy(in));
        json_set(lo,"out",out);
        char *ser=json_serialize(lo);
        printf("%s\n",ser?ser:"{}"); free(ser); json_free(lo); json_free(rec);
    }
    return 0;
}
