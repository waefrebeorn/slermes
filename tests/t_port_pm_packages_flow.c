/* t_port_pm_packages_flow.c -- harness for the pm/packages.py FLOW port
 * (src/hermes_cli/pm_packages_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state", the release-digest cache).
 * The echoed "in" is the ORIGINAL input (the oracle re-drives the live
 * reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * in.args carries the method parameters plus "self" (the class-attribute
 * object); in.state carries the mutable adapter state.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "hermes_cli/pm_packages_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ppf_fn_t)(pp_ctx_t *, const json_t *, const json_t *);
typedef struct { const char *func; ppf_fn_t fn; } ppf_case_t;

/* _probe_env takes no args; adapt it to the uniform 3-arg case shape. */
static json_t *ppf_probe_env_case(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)args;
    return pp_bp_probe_env(ctx, self);
}

static const ppf_case_t CASES[] = {
    { "npm_env", pp_npm_env },
    { "pnpm_env", pp_pnpm_env },
    { "uv_cache_path", pp_uv_cache_path },
    { "_github_release_digests", pp_github_release_digests },
    { "BinaryPackage._rel", pp_bp_rel },
    { "BinaryPackage.stage", pp_bp_stage },
    { "BinaryPackage.binary", pp_bp_binary },
    { "BinaryPackage.verify", pp_bp_verify },
    { "BinaryPackage._probe_env", ppf_probe_env_case },
    { "Dmgbuild.fetch_url", pp_dmgbuild_fetch_url },
    { "Dmgbuild.verify", pp_dmgbuild_verify },
    { "_BionicDebArm.verify", pp_bionic_verify },
    { "Uv.fetch_url", pp_uv_fetch_url },
    { "Uv.latest_versions", pp_uv_latest_versions },
    { "Python.fetch_url", pp_python_fetch_url },
    { "Python.latest_versions", pp_python_latest_versions },
    { "Python.verify", pp_python_verify },
    { "Nodejs.fetch_url", pp_nodejs_fetch_url },
    { "Nodejs.latest_versions", pp_nodejs_latest_versions },
    { "Nodejs.repair_staged_verification", pp_nodejs_repair },
    { "TermuxDocker.missing_reason", pp_termuxdocker_missing_reason },
    { "TermuxDocker.fetch_url", pp_termuxdocker_fetch_url },
    { "Npm.latest_versions", pp_npm_latest_versions },
    { "Pnpm.fetch_urls", pp_pnpm_fetch_urls },
    { "Pnpm.latest_versions", pp_pnpm_latest_versions },
    { "Git.fetch_url", pp_git_fetch_url },
    { "Git.latest_versions", pp_git_latest_versions },
    { "Git.env", pp_git_env },
    { "Gh.fetch_url", pp_gh_fetch_url },
    { "Gh.latest_versions", pp_gh_latest_versions },
    { "Ffmpeg.fetch_url", pp_ffmpeg_fetch_url },
    { "Ffmpeg.latest_versions", pp_ffmpeg_latest_versions },
    { "Ripgrep.fetch_url", pp_ripgrep_fetch_url },
    { "Ripgrep.latest_versions", pp_ripgrep_latest_versions },
    { "Just.fetch_url", pp_just_fetch_url },
    { "Just.known_sha256", pp_just_known_sha256 },
    { "Just.latest_versions", pp_just_latest_versions },
    { "CuaDriver.fetch_url", pp_cuadriver_fetch_url },
    { "CuaDriver.latest_versions", pp_cuadriver_latest_versions },
    { "Chromium.store_entry", pp_chromium_store_entry },
    { "Chromium.fetch_url", pp_chromium_fetch_url },
    { "LlamaCpp._asset_names", pp_llamacpp_asset_names },
    { "LlamaCpp.fetch_urls", pp_llamacpp_fetch_urls },
    { "LlamaCpp.fetch_url", pp_llamacpp_fetch_url },
    { "LlamaCpp.latest_versions", pp_llamacpp_latest_versions },
    { "LlamaCpp.known_sha256", pp_llamacpp_known_sha256 },
    { "LlamaCppCuda._asset_names", pp_llamacppcuda_asset_names },
    { "WhisperCppCpu.fetch_urls", pp_whispercpp_fetch_urls },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)ch;
    }
    if (ch == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    json_t *empty_args = json_object();
    char *line;
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) in = NULL;
        json_t *out = json_object();

        const ppf_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *self = args ? json_obj_get(args, "self") : NULL;
            if (!self || self->type != JSON_OBJECT) self = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            pp_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, self ? self : empty_args, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", ctx.state);
            json_free(ctx.cursors);
        }
        json_t *echo = json_object();
        json_set_hdr(echo, "func", func);
        json_set(echo, "in", in ? json_copy(in) : json_null());
        json_set(echo, "out", out);
        char *s = json_serialize(echo);
        if (s) { printf("%s\n", s); free(s); }
        json_free(echo);
        json_free(rec);
    }
    json_free(empty_args);
    return 0;
}
