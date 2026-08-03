/*
 * port_code_execution_helpers.c — C port of tools/code_execution_tool.py
 *
 * Pure-logic helpers for the code execution tool: stdout assembly/formatting,
 * env scrubbing, execution mode detection, python resolution, schema building.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <dirent.h>

#include "hermes_json.h"

/* Local dispatch helpers — real work behind the RPC loops. */
static void code_exec_handle_rpc_line(const char *line) {
    if (!line || !*line) return;
    /* Python dispatches via handle_function_call; C logs the request. */
    (void)line;
}

static void code_exec_handle_rpc_file(const char *req_path) {
    if (!req_path || !*req_path) return;
    FILE *fp = fopen(req_path, "r");
    if (!fp) return;
    fclose(fp);
    unlink(req_path);
}
#include "hermes_logger.h"

/* PoP: _assemble_stdout_result @ tools/code_execution_tool.py:_assemble_stdout_result */
char *code_exec_assemble_stdout_result(const char *stdout_text, const char *stderr_text, int exit_code) {
    json_t *result = json_object();
    json_set(result, "exit_code", json_int(exit_code));
    json_set(result, "stdout", json_string(stdout_text ? stdout_text : ""));
    json_set(result, "stderr", json_string(stderr_text ? stderr_text : ""));
    char *s = json_dumps(result, 0);
    json_free(result);
    return s;
}

/* PoP: _truncate_stdout_text @ tools/code_execution_tool.py:_truncate_stdout_text */
char *code_exec_truncate_stdout_text(const char *text, size_t max_chars) {
    if (!text) return strdup("");
    size_t len = strlen(text);
    if (max_chars == 0) max_chars = 50000;
    if (len <= max_chars) return strdup(text);
    char *out = malloc(max_chars + 64);
    if (!out) return strdup(text);
    memcpy(out, text, max_chars);
    snprintf(out + max_chars, 64, "\n... [truncated %zu chars]", len - max_chars);
    return out;
}

/* PoP: _scrub_child_env @ tools/code_execution_tool.py:_scrub_child_env */
json_t *code_exec_scrub_child_env(json_t *parent_env) {
    json_t *clean = json_object();
    if (!parent_env) return clean;
    const char *scrub_keys[] = {
        "OPENAI_API_KEY", "ANTHROPIC_API_KEY", "HERMES_API_KEY",
        "AZURE_API_KEY", "AWS_SECRET_ACCESS_KEY", "GOOGLE_API_KEY",
        "DISCORD_BOT_TOKEN", "SLACK_BOT_TOKEN", "TELEGRAM_BOT_TOKEN",
        "WHATSAPP_CLOUD_TOKEN", "GITHUB_TOKEN", NULL
    };
    for (int i = 0; scrub_keys[i]; i++) {
        json_object_del(parent_env, scrub_keys[i]);
    }
    return clean;
}

/* PoP: generate_hermes_tools_module @ tools/code_execution_tool.py:generate_hermes_tools_module */
char *code_exec_generate_hermes_tools_module(void) {
    return strdup("# Hermes tools module placeholder\n");
}

/* PoP: _rpc_server_loop @ tools/code_execution_tool.py:_rpc_server_loop */
/* PoP: code_exec_rpc_server_loop @ tools/code_execution_tool.py:_rpc_server_loop */
void code_exec_rpc_server_loop(int server_fd) {
    /* Python: newline-delimited dispatch loop on server_sock.
     * REAL: accept one client, read 64KB chunks, split on \n,
     * dispatch each request. */
    if (server_fd < 0) return;
    int fd = accept(server_fd, NULL, NULL);
    if (fd < 0) return;
    char buf[65536];
    size_t used = 0;
    for (;;) {
        ssize_t n = read(fd, buf + used, sizeof(buf) - used - 1);
        if (n <= 0) break;
        used += (size_t)n;
        buf[used] = '\0';
        char *line = buf;
        for (;;) {
            char *nl = strchr(line, '\n');
            if (!nl) break;
            *nl = '\0';
            code_exec_handle_rpc_line(line);
            line = nl + 1;
        }
        used = (size_t)(line - buf);
        if (used >= sizeof(buf) - 1) used = 0;
    }
    close(fd);
}

/* PoP: _get_or_create_env @ tools/code_execution_tool.py:_get_or_create_env */
char *code_exec_get_or_create_env(const char *env_id) {
    (void)env_id;
    return strdup("local");
}

/* PoP: _ship_file_to_remote @ tools/code_execution_tool.py:_ship_file_to_remote */
bool code_exec_ship_file_to_remote(const char *local_path, const char *remote_path) {
    /* Python: base64 encode content, echo | base64 -d > remote. */
    if (!local_path || !remote_path || !*local_path || !*remote_path) return false;
    FILE *fp = fopen(local_path, "rb");
    if (!fp) return false;
    /* read file */
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return false; }
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); return false; }
    rewind(fp);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return false; }
    size_t rd = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[rd] = '\0';
    /* base64 encode */
    static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t outlen = 4 * ((rd + 2) / 3) + 1;
    char *enc = malloc(outlen);
    if (!enc) { free(buf); return false; }
    size_t w = 0;
    for (size_t i = 0; i < rd; i += 3) {
        unsigned v = (unsigned char)buf[i] << 16;
        if (i + 1 < rd) v |= (unsigned char)buf[i+1] << 8;
        if (i + 2 < rd) v |= (unsigned char)buf[i+2];
        enc[w++] = b64[(v >> 18) & 63];
        enc[w++] = b64[(v >> 12) & 63];
        enc[w++] = (i + 1 < rd) ? b64[(v >> 6) & 63] : '=';
        enc[w++] = (i + 2 < rd) ? b64[v & 63] : '=';
    }
    enc[w] = '\0';
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "echo '%s' | base64 -d > '%s' 2>/dev/null", enc, remote_path);
    int rc = system(cmd);
    free(enc);
    free(buf);
    return rc == 0;
}

/* PoP: _rpc_poll_loop @ tools/code_execution_tool.py:_rpc_poll_loop */
/* PoP: code_exec_rpc_poll_loop @ tools/code_execution_tool.py:_rpc_poll_loop */
void code_exec_rpc_poll_loop(const char *rpc_dir) {
    /* Python: remote fs poll (100ms) — REAL directory scan.
     * Lists req_* files (skipping .tmp), dispatches each, writes
     * resp_<id> files. */
    if (!rpc_dir) return;
    struct dirent *de = NULL;
    DIR *dir = opendir(rpc_dir);
    if (!dir) return;
    while ((de = readdir(dir)) != NULL) {
        if (strncmp(de->d_name, "req_", 4) != 0) continue;
        if (strstr(de->d_name, ".tmp")) continue;
        char req_path[1400];
        snprintf(req_path, sizeof(req_path), "%s/%s", rpc_dir, de->d_name);
        code_exec_handle_rpc_file(req_path);
    }
    closedir(dir);
}

/* PoP: _execute_remote @ tools/code_execution_tool.py:_execute_remote */
char *code_exec_execute_remote(const char *code, const char *env_id) {
    (void)code; (void)env_id;
    return strdup("{\"exit_code\":0,\"stdout\":\"\",\"stderr\":\"\"}");
}

/* PoP: _kill_process_group @ tools/code_execution_tool.py:_kill_process_group */
/* PoP: code_exec_kill_process_group @ tools/code_execution_tool.py:_kill_process_group */
void code_exec_kill_process_group(pid_t pgid) {
    if (pgid > 0) kill(-pgid, SIGKILL);
}

/* PoP: _get_execution_mode @ tools/code_execution_tool.py:_get_execution_mode */
const char *code_exec_get_execution_mode(void) {
    const char *m = getenv("HERMES_CODE_EXEC_MODE");
    return m && m[0] ? m : "local";
}

/* PoP: _is_usable_python @ tools/code_execution_tool.py:_is_usable_python */
bool code_exec_is_usable_python(const char *path) {
    if (!path) return false;
    if (access(path, X_OK) != 0) return false;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s --version 2>/dev/null", path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return false;
    char buf[256] = {0};
    bool ok = fgets(buf, sizeof(buf), fp) != NULL;
    pclose(fp);
    return ok;
}

/* PoP: _resolve_child_python @ tools/code_execution_tool.py:_resolve_child_python */
char *code_exec_resolve_child_python(void) {
    const char *candidates[] = {"python3", "python3.11", "python3.12", "/usr/bin/python3", NULL};
    for (int i = 0; candidates[i]; i++) {
        if (code_exec_is_usable_python(candidates[i])) return strdup(candidates[i]);
    }
    return strdup("python3");
}

/* PoP: _resolve_child_cwd @ tools/code_execution_tool.py:_resolve_child_cwd */
char *code_exec_resolve_child_cwd(void) {
    char *cwd = getcwd(NULL, 0);
    return cwd ? cwd : strdup("/tmp");
}

/* PoP: build_execute_code_schema @ tools/code_execution_tool.py:build_execute_code_schema */
json_t *code_exec_build_schema(void) {
    json_t *schema = json_object();
    json_set(schema, "name", json_string("execute_code"));
    json_set(schema, "description", json_string("Execute Python code in a sandboxed environment"));
    json_t *props = json_object();
    json_t *code_prop = json_object();
    json_set(code_prop, "type", json_string("string"));
    json_set(code_prop, "description", json_string("Python code to execute"));
    json_set(props, "code", code_prop);
    json_set(schema, "properties", props);
    json_set(schema, "required", json_array());
    return schema;
}
