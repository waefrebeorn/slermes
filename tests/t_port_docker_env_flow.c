/*
 * t_port_docker_env_flow.c — oracle harness for the Docker container-lifecycle
 * FLOW cores ported in src/tools/environments/docker_env_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires the universal seam to a SCRIPTED recording stub (in["io"][name]: a
 * single value reused, a bare array consumed per call, or
 * {"__script__":true,"ret":X} for a constant list) that appends one
 * {ev,args,ret} trace event per call, in call order. Method flows run on a
 * COPY of in["self"] and emit the mutated copy as out["self"]. Emits
 * {"func","in","out":{"ret","exc","trace"[,"self"]}} per case; the original
 * "in" is echoed unmutated. tests/oracle_docker_env_flow.py drives the LIVE
 * AST-extracted Python with the same scripts and compares ordered trace +
 * ret + state + exception.
 *
 * Self-contained: #includes the port .c (area-qualified); links only libjson.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "tools/environments/docker_env_flow.c"

typedef struct {
    const json_t *in;
    def_trace_t *trace;
    json_t *counts;   /* object: seam name -> call count (array scripts) */
} script_ctx_t;

/* ── generic scripted recording seam ── */
static json_t *sc_call(void *ctx, const char *name, const json_t *args) {
    script_ctx_t *c = ctx;
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *script = io ? json_obj_get(io, name) : NULL;
    const json_t *val = NULL;
    if (script && script->type == JSON_OBJECT && json_get_bool(script, "__script__", false)) {
        val = json_obj_get(script, "ret");   /* constant (list) result */
    } else if (script && script->type == JSON_ARRAY) {
        double cnt = json_get_num(c->counts, name, 0);
        size_t idx = (size_t)cnt;
        if (script->c.count) {
            if (idx >= script->c.count) idx = script->c.count - 1;
            val = script->c.items[idx];
        }
        json_set(c->counts, name, json_number(cnt + 1));
    } else {
        val = script;   /* single value reused (NULL when absent) */
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    json_set(e, "ret", val ? json_copy(val) : json_null());
    def_trace_add(c->trace, e);
    /* a scripted JSON null returns a NULL pointer (None), not a json_null node */
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

/* ── input readers ── */
static const json_t *in_node(const json_t *in, const char *k) { return json_obj_get(in, k); }
static const char *in_str(const json_t *in, const char *k, const char *def) { return json_get_str(in, k, def); }
static bool in_bool(const json_t *in, const char *k, bool def) { return json_get_bool(in, k, def); }
static double in_num(const json_t *in, const char *k, double def) { return json_get_num(in, k, def); }
static json_t *copy_obj(const json_t *in, const char *k) {
    const json_t *n = json_obj_get(in, k);
    json_t *c = n ? json_copy(n) : json_object();
    if (!c || c->type != JSON_OBJECT) { json_free(c); c = json_object(); }
    return c;
}
static const char *in_str_or_null(const json_t *in, const char *k) {
    const json_t *n = json_obj_get(in, k);
    return (n && n->type == JSON_STRING) ? n->str_val : NULL;
}

/* ── per-case dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, NULL, json_object() };
    def_trace_t trace = { NULL };
    ctx.trace = &trace;
    def_seams_t s = { &ctx, sc_call };
    json_t *exc = NULL;
    json_t *ret = NULL;
    json_t *self = NULL;
    bool has_self = false;
    bool ret_bool = false;
    bool has_bool = false;

    /* reserve envelope positions (ret, exc, trace) before state keys */
    json_set(out, "ret", json_null());
    json_set(out, "exc", json_null());
    json_set(out, "trace", json_array());

    if (strcmp(func, "docker_query") == 0) {
        ret = def_docker_query(in_node(in, "argv"), in_num(in, "timeout", 10),
                               in_str(in, "fail", ""), in_node(in, "fail_args"),
                               in_str_or_null(in, "nonzero"),
                               strcmp(in_str(in, "exc_types", "default"), "subprocess") == 0,
                               &s, &exc);
    } else if (strcmp(func, "is_idle_workspace_container") == 0) {
        ret_bool = def_is_idle_workspace_container(in_str(in, "docker_exe", "docker"),
                                                   in_str(in, "container_id", ""), &s, &exc);
        has_bool = true;
    } else if (strcmp(func, "extra_args_network_mode") == 0) {
        ret = def_extra_args_network_mode(in_node(in, "extra_args"));
    } else if (strcmp(func, "split_volume_spec") == 0) {
        ret = def_split_volume_spec(in_node(in, "spec"));
    } else if (strcmp(func, "container_mount_taken") == 0) {
        ret_bool = def_container_mount_taken(in_node(in, "volume_args"), in_str(in, "mount", ""));
        has_bool = true;
    } else if (strcmp(func, "existing_host_mount") == 0) {
        ret = def_existing_host_mount(in_node(in, "volume_args"),
                                      in_str(in, "host_cwd_abs", ""), &s, &exc);
    } else if (strcmp(func, "free_host_cwd_mount") == 0) {
        ret = def_free_host_cwd_mount(in_node(in, "volume_args"));
    } else if (strcmp(func, "abs_host_cwd") == 0) {
        ret = def_abs_host_cwd(in_str(in, "host_cwd", ""), in_str(in, "os_name", "posix"), &s, &exc);
    } else if (strcmp(func, "host_user_args") == 0) {
        ret = def_host_user_args(in_bool(in, "run_as_host_user", false), &s, &exc);
    } else if (strcmp(func, "attach_existing_container") == 0) {
        self = copy_obj(in, "self");
        has_self = true;
        ret_bool = def_attach_existing_container(self, in_str(in, "task_label", ""),
                                                 in_str(in, "profile_name", ""),
                                                 in_str(in, "egress_label", "off"),
                                                 in_bool(in, "network", true), &s, &exc);
        has_bool = true;
    } else if (strcmp(func, "image_available_locally") == 0) {
        self = copy_obj(in, "self");
        has_self = true;
        ret_bool = def_image_available_locally(self, &s, &exc);
        has_bool = true;
    } else if (strcmp(func, "start_container") == 0) {
        self = copy_obj(in, "self");
        has_self = true;
        ret = def_start_container(self, in_str(in, "container_id", ""), &s, &exc);
    } else if (strcmp(func, "docker_run") == 0) {
        self = copy_obj(in, "self");
        has_self = true;
        ret = def_docker_run(self, in_str(in, "cwd", "/root"), &s, &exc);
    } else if (strcmp(func, "run_command") == 0) {
        self = copy_obj(in, "self");
        has_self = true;
        ret = def_run_command(self, in_str(in, "name", ""), in_str(in, "workdir", "/root"));
    } else if (strcmp(func, "resource_args") == 0) {
        self = copy_obj(in, "self");
        has_self = true;
        ret = def_resource_args(self, in_str(in, "image", ""), in_num(in, "cpu", 0),
                                (long)in_num(in, "memory", 0), (long)in_num(in, "disk", 0),
                                in_bool(in, "network", true), in_node(in, "shm_size"),
                                in_node(in, "extra_args"), in_str(in, "platform", "linux"),
                                &s, &exc);
    } else if (strcmp(func, "mount_args") == 0) {
        self = copy_obj(in, "self");
        has_self = true;
        ret = def_mount_args(self, in_node(in, "volumes"), in_str(in, "host_cwd", ""),
                             in_bool(in, "auto_mount_cwd", false), in_str(in, "task_id", "default"),
                             in_str(in, "os_name", "posix"), &s, &exc);
    } else if (strcmp(func, "egress_and_env_args") == 0) {
        self = copy_obj(in, "self");
        has_self = true;
        ret = def_egress_and_env_args(self, in_node(in, "extra_args"), &s, &exc);
    } else if (strcmp(func, "container_image") == 0) {
        self = copy_obj(in, "self");
        has_self = true;
        ret = def_container_image(self, in_str(in, "container_id", ""), &s, &exc);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    /* ret envelope: on a propagated exception the reference projects ret=None */
    if (exc) {
        json_set(out, "ret", json_null());
        if (ret) json_free(ret);
    } else if (ret) {
        json_set(out, "ret", ret);
    } else if (has_bool) {
        json_set(out, "ret", json_bool(ret_bool));
    } else {
        json_set(out, "ret", json_null());
    }
    json_set(out, "exc", exc ? exc : json_null());
    json_set(out, "trace", trace.events ? trace.events : json_array());
    if (has_self) json_set(out, "self", self);
    json_free(ctx.counts);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
            free(err); json_free(c); continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        json_t *func_node = json_string(func);
        char *func_s = json_serialize(func_node);
        json_free(func_node);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}
