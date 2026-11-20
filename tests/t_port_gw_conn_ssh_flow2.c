/*
 * t_port_gw_conn_ssh_flow2.c — oracle harness for gw_conn_ssh_flow2.
 * Includes the port .c by area-qualified name. Reads JSON-line cases from
 * stdin (tests/fixtures/gw_conn_ssh_flow2.jsonl), wires the universal seam to
 * SCRIPTED results with a recording stub (one trace event per seam call), and
 * emits {"func":...,"in":...,"out":{"ret":...,"trace":[...]}}.
 * FAIL-CLOSED: a missing seam key -> {"__raise__":"RuntimeError",...}; a
 * present JSON null is a valid None; a JSON array is one result per call.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cli/gw_conn_ssh_flow2.c"

typedef struct { const json_t *in; gcs2_trace_t *trace; json_t *counters; } cs2_ctx_t;

static json_t *cs2_drop_null(const json_t *args) {
    json_t *o = json_object();
    if (args && args->type == JSON_OBJECT)
        for (size_t i=0;i<args->c.count;i++) {
            const json_t *v=args->c.items[i];
            if (v && v->type==JSON_NULL) continue;
            json_set(o, args->c.keys[i], json_copy(v));
        }
    return o;
}
static json_t *cs2_script_call(void *vctx, const char *name, const json_t *args) {
    cs2_ctx_t *ctx=vctx;
    const json_t *sc=json_obj_get(json_obj_get(ctx->in,"seams"),name);
    json_t *result=NULL;
    if (!sc) {
        char *m=gcs2_xsf("unscripted seam: %s",name);
        result=gcs2_raise_spec("RuntimeError",m); free(m);
    } else if (sc->type==JSON_ARRAY) {
        double idx=json_get_num(ctx->counters,name,0);
        size_t i=(size_t)idx,n=json_len(sc);
        if(n) result=json_copy(json_get(sc,i<n?i:n-1));
        json_set(ctx->counters,name,json_number(idx+1));
    } else result=json_copy(sc);
    json_t *e=json_object();
    json_set(e,"ev",json_string(name));
    json_set(e,"args",cs2_drop_null(args));
    json_set(e,"ret",result?json_copy(result):json_null());
    gcs2_trace_add(ctx->trace,e);
    return result;
}
static json_t *cs2_project_ret(json_t *ret) {
    if (gcs2_is_skew(ret)) return gcs2_skew_project(ret);
    return ret;
}
#define S(k) json_get_str(in,k,NULL)
#define J(k) json_obj_get(in,k)

static void run_case(const char *func, const json_t *in, json_t *out) {
    gcs2_trace_t trace={NULL};
    json_t *counters=json_object();
    cs2_ctx_t ctx={in,&trace,counters};
    gcs2_seams_t seams={&ctx,cs2_script_call};
    const gcs2_seams_t *sp=&seams;
    json_t *ret=NULL;

    if (!strcmp(func,"_err")) { ret=gcs2_err(S("kind"), S("message")); }
    if (!strcmp(func,"backend_profile_arg")) { ret=gcs2_backend_profile_arg(J("profile")); }
    if (!strcmp(func,"mint_token")) { ret=gcs2_mint_token(sp); }
    if (!strcmp(func,"fingerprint_token")) { ret=gcs2_fingerprint_token(J("token")); }
    if (!strcmp(func,"validate_ownership_id")) { ret=gcs2_validate_ownership_id(J("ownership_id")); }
    if (!strcmp(func,"validate_spawn_nonce")) { ret=gcs2_validate_spawn_nonce(J("nonce")); }
    if (!strcmp(func,"ownership_directory")) { ret=gcs2_ownership_directory(J("ownership_id")); }
    if (!strcmp(func,"lockfile_path")) { ret=gcs2_lockfile_path(J("ownership_id")); }
    if (!strcmp(func,"connect_reservation_path")) { ret=gcs2_connect_reservation_path(J("ownership_id")); }
    if (!strcmp(func,"spawn_log_path")) { ret=gcs2_spawn_log_path(J("ownership_id"), J("nonce")); }
    if (!strcmp(func,"spawn_token_path")) { ret=gcs2_spawn_token_path(J("ownership_id"), J("nonce")); }
    if (!strcmp(func,"shq")) { ret=gcs2_shq(J("value")); }
    if (!strcmp(func,"validate_remote_path")) { ret=gcs2_validate_remote_path(J("path")); }
    if (!strcmp(func,"expand_remote_path")) { ret=gcs2_expand_remote_path(J("path")); }
    if (!strcmp(func,"assert_safe_remote_home")) { ret=gcs2_assert_safe_remote_home(J("home")); }
    if (!strcmp(func,"remote_install_root")) { ret=gcs2_remote_install_root(J("home")); }
    if (!strcmp(func,"probe_remote_platform")) { ret=gcs2_probe_remote_platform(sp); }
    if (!strcmp(func,"probe_remote_hermes_home")) { ret=gcs2_probe_remote_hermes_home(sp); }
    if (!strcmp(func,"assert_remote_install_update_clear")) { ret=gcs2_assert_remote_install_update_clear(sp, J("hermes_home"), J("hermes_path")); }
    if (!strcmp(func,"_is_executable")) { ret=gcs2_is_executable(sp, J("candidate")) ? json_bool(true) : json_bool(false); }
    if (!strcmp(func,"locate_hermes")) { ret=gcs2_locate_hermes(sp, J("remote_hermes_path")); }
    if (!strcmp(func,"probe_hermes_version")) { ret=gcs2_probe_hermes_version(sp, J("hermes_path")); }
    if (!strcmp(func,"remote_supports_ssh_ownership")) { ret=gcs2_remote_supports_ssh_ownership(sp, J("hermes_path")); }
    if (!strcmp(func,"_valid_int")) { ret=gcs2_valid_int(J("value"), (int)json_get_num(in,"lo",0), (int)json_get_num(in,"hi",0)) ? json_bool(true) : json_bool(false); }
    if (!strcmp(func,"parse_lockfile")) { ret=gcs2_parse_lockfile(J("text"), J("ownership_id")); }
    if (!strcmp(func,"read_lockfile")) { ret=gcs2_read_lockfile(sp, J("ownership_id")); }
    if (!strcmp(func,"_lock_json")) { ret=gcs2_lock_json(J("lock")); }
    if (!strcmp(func,"write_lockfile")) { ret=gcs2_write_lockfile(sp, J("ownership_id"), J("lock")); }
    if (!strcmp(func,"remove_lockfile")) { ret=gcs2_remove_lockfile(sp, J("ownership_id")); }
    if (!strcmp(func,"exec_probe_verdict")) { ret=gcs2_exec_probe_verdict(sp, S("command"), J("sentinels"), S("failure")); }
    if (!strcmp(func,"remote_pid_alive")) { ret=gcs2_remote_pid_alive(sp, J("pid")); }
    if (!strcmp(func,"remote_process_creation_time")) { ret=gcs2_remote_process_creation_time(sp, J("pid")); }
    if (!strcmp(func,"pid_is_our_serve")) { ret=gcs2_pid_is_our_serve(sp, J("lock"), J("ownership_id")); }
    if (!strcmp(func,"_terminate")) { ret=gcs2_terminate(sp, J("pid")); }
    if (!strcmp(func,"cleanup_stale")) { ret=gcs2_cleanup_stale(sp, J("ownership_id"), J("lock"), json_get_bool(in,"pid_alive",true)); }
    if (!strcmp(func,"disconnect_remote")) { ret=gcs2_disconnect_remote(sp, J("ownership_id")); }
    if (!strcmp(func,"with_remote_update_mutex")) { ret=gcs2_with_remote_update_mutex(S("command"), S("marker_word"), S("hermes_word")); }
    if (!strcmp(func,"_spawn_payload_prefix")) { ret=gcs2_spawn_payload_prefix(J("plan")); }
    if (!strcmp(func,"build_spawn_command")) { ret=gcs2_build_spawn_command(J("plan")); }
    if (!strcmp(func,"_remove_remote")) { ret=gcs2_remove_remote(sp, J("path")); }
    if (!strcmp(func,"spawn_remote_serve")) { ret=gcs2_spawn_remote_serve(sp, J("hermes_path"), S("profile"), J("token"), J("ownership_id"), J("hermes_home")); }
    if (!strcmp(func,"_now_iso")) { ret=gcs2_now_iso(sp); }
    if (!strcmp(func,"remote_profile_missing_error")) { ret=gcs2_remote_profile_missing_error(J("output")); }
    if (!strcmp(func,"scrape_ready_port")) { ret=gcs2_scrape_ready_port(sp, J("log_path"), json_get_num(in,"timeout_s",120.0)); }
    if (!strcmp(func,"classify_ssh_reuse_proof")) { ret=gcs2_classify_ssh_reuse_proof(J("proof"), J("spawn_nonce")); }
    if (!strcmp(func,"lock_serves_connect")) { ret=gcs2_lock_serves_connect(J("lock"), S("profile"), J("reuse_token"), S("hermes_path"), S("hermes_home")); }
    if (!strcmp(func,"is_forward_bind_collision")) { ret=gcs2_is_forward_bind_collision(S("error")) ? json_bool(true) : json_bool(false); }
    if (!strcmp(func,"open_forward")) { ret=gcs2_open_forward(sp, (int)json_get_num(in,"remote_port",0), (int)json_get_num(in,"attempts",3)); }
    if (!strcmp(func,"wait_for_remote_spawn_completion")) { ret=gcs2_wait_for_remote_spawn_completion(sp, J("ownership_id"), json_get_num(in,"timeout_s",120.0)); }
    if (!strcmp(func,"_skew_error")) { ret=gcs2_skew_error(J("ownership_id"), J("skew")); }
    if (!strcmp(func,"_try_reuse")) { ret=gcs2_try_reuse(sp, J("deps"), J("remote"), json_copy(J("lock"))); }
    if (!strcmp(func,"_spawn_fresh")) { ret=gcs2_spawn_fresh(sp, J("deps"), J("remote")); }
    if (!strcmp(func,"_rollback_spawn")) { ret=gcs2_rollback_spawn(sp, S("ownership_id"), J("owned"), J("spawned"), (int)json_get_num(in,"local_port",0), (int)json_get_num(in,"remote_port",0)); }
    if (!strcmp(func,"_probe_remote")) { ret=gcs2_probe_remote(sp, J("deps")); }
    if (!strcmp(func,"connect")) { ret=gcs2_connect(sp, J("deps")); }
    else fprintf(stderr,"unknown func: %s\n",func);

    ret=cs2_project_ret(ret);
    json_set(out,"ret",ret?ret:json_null());
    json_set(out,"trace",trace.events?json_copy(trace.events):json_array());
    gcs2_trace_free(&trace);
    json_free(counters);
}

int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    char *line=NULL; size_t cap=0; ssize_t len;
    while ((len=getline(&line,&cap,stdin))>0) {
        if (len && line[len-1]=='\n') line[len-1]='\0';
        if (line[0]!='{') continue;
        char *err=NULL;
        json_t *c=json_parse(line,&err);
        if (err||!c||c->type!=JSON_OBJECT) { fprintf(stderr,"bad case\n"); free(err); if(c)json_free(c); continue; }
        free(err);
        const char *func=json_get_str(c,"func","");
        const json_t *in=json_obj_get(c,"in");
        json_t *empty=json_object(), *out=json_object();
        run_case(func,in?in:empty,out);
        json_free(empty);
        char *is=json_serialize(in?in:empty);
        char *os=json_serialize(out);
        char *fs=json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n",fs,is?is:"null",os?os:"null");
        free(is); free(os); free(fs);
        json_free(out); json_free(c);
    }
    free(line); return 0;
}
