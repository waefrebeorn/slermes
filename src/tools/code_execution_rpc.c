/*
 * code_execution_rpc.c — C port of upstream Hermes tools/code_execution_rpc.py.
 *
 * PoP: w11_codeexecut_b29647_private_dirs_cmd @ tools/code_execution_rpc.py:_private_dirs_cmd
 * PoP: w11_codeexecut_b29647_rpc_token_ok @ tools/code_execution_rpc.py:_rpc_token_ok
 * PoP: w11_codeexecut_b29647_result_error @ tools/code_execution_rpc.py:_result_error
 * PoP: w11_codeexecut_b29647_tool_errors_since @ tools/code_execution_rpc.py:tool_errors_since
 * PoP: w11_codeexecut_b29647_execute_checked @ tools/code_execution_rpc.py:_execute_checked
 * PoP: w11_codeexecut_b29647_remote_write @ tools/code_execution_rpc.py:_remote_write
 *
 * env.execute() is a data seam: the fixture supplies its result; no subprocess runs.
 */
#include "noc_w11_codeexecut_b29647.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *raise_obj(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

/* shlex.quote (POSIX): _find_unsafe = [^\w@%+=:,./-] with re.ASCII. */
static int shlex_safe_char(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return 1;
    return strchr("_@%+=:,./-", (int)c) != NULL;
}

static char *shlex_quote(const char *s) {
    if (!s) return strdup("''");
    if (!*s) return strdup("''");
    int unsafe = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if (!shlex_safe_char(*p)) { unsafe = 1; break; }
    if (!unsafe) return strdup(s);
    /* "'" + s.replace("'", "'\"'\"'") + "'" */
    size_t cap = strlen(s) * 6 + 3;
    char *out = (char *)malloc(cap);
    size_t i = 0;
    out[i++] = '\'';
    for (const char *p = s; *p; p++) {
        if (*p == '\'') {
            const char rep[] = "'\"'\"'";
            memcpy(out + i, rep, sizeof(rep) - 1);
            i += sizeof(rep) - 1;
        } else out[i++] = *p;
    }
    out[i++] = '\'';
    out[i] = '\0';
    return out;
}

/* Python repr() of a plain string (fixtures use simple ASCII). */
static char *py_repr(const char *s) {
    if (!s) return strdup("None");
    int has_single = strchr(s, '\'') != NULL;
    int has_double = strchr(s, '"') != NULL;
    char q = (has_single && !has_double) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *out = (char *)malloc(cap);
    size_t i = 0;
    out[i++] = q;
    for (const char *p = s; *p; p++) {
        if (*p == '\\' || *p == q) out[i++] = '\\';
        else if (*p == '\n') { out[i++] = '\\'; out[i++] = 'n'; continue; }
        else if (*p == '\r') { out[i++] = '\\'; out[i++] = 'r'; continue; }
        else if (*p == '\t') { out[i++] = '\\'; out[i++] = 't'; continue; }
        out[i++] = *p;
    }
    out[i++] = q;
    out[i] = '\0';
    return out;
}

json_t *w11_codeexecut_b29647_private_dirs_cmd(const char *root, const struct json_t *subdirs) {
    size_t nsub = (subdirs && subdirs->type == JSON_ARRAY) ? json_len(subdirs) : 0;
    char joined_mkdir[8192], joined_chmod[8192];
    size_t pm = 0, pc = 0;
    joined_mkdir[0] = joined_chmod[0] = '\0';

    if (nsub == 0) {
        char *q = shlex_quote(root ? root : "");
        pm += (size_t)snprintf(joined_mkdir + pm, sizeof(joined_mkdir) - pm, "%s", q);
        free(q);
    } else {
        for (size_t i = 0; i < nsub; i++) {
            json_t *e = json_get(subdirs, i);
            char *q = shlex_quote(e && e->type == JSON_STRING ? e->str_val : "");
            pm += (size_t)snprintf(joined_mkdir + pm, sizeof(joined_mkdir) - pm, "%s%s", i ? " " : "", q);
            free(q);
        }
    }
    {
        char *q = shlex_quote(root ? root : "");
        pc += (size_t)snprintf(joined_chmod + pc, sizeof(joined_chmod) - pc, "%s", q);
        free(q);
    }
    for (size_t i = 0; i < nsub; i++) {
        json_t *e = json_get(subdirs, i);
        char *q = shlex_quote(e && e->type == JSON_STRING ? e->str_val : "");
        pc += (size_t)snprintf(joined_chmod + pc, sizeof(joined_chmod) - pc, " %s", q);
        free(q);
    }
    char buf[16384];
    snprintf(buf, sizeof(buf), "umask 077 && mkdir -p %s && chmod 700 %s", joined_mkdir, joined_chmod);
    return json_string(buf);
}

json_t *w11_codeexecut_b29647_rpc_token_ok(const struct json_t *request, const char *rpc_token) {
    if (!rpc_token || !*rpc_token) return json_bool(false);
    const char *tok = " ";
    json_t *v = json_obj_get(request, "token");
    if (!v || v->type == JSON_NULL) tok = "";
    else if (v->type == JSON_STRING) tok = v->str_val ? v->str_val : "";
    else if (v->type == JSON_BOOL) tok = v->bool_val ? "True" : "False";
    else if (v->type == JSON_NUMBER) {
        /* str(int) form for integral JSON numbers */
        static char nb[64];
        snprintf(nb, sizeof(nb), "%.0f", v->num_val);
        tok = nb;
    }
    return json_bool(strcmp(tok, rpc_token) == 0 ? true : false);
}

json_t *w11_codeexecut_b29647_result_error(const char *result) {
    if (!result || result[0] != '{' || !strstr(result, "\"error\"")) return json_string("");
    json_t *body = json_parse(result, NULL);
    if (!body || body->type != JSON_OBJECT) { json_free(body); return json_string(""); }
    json_t *e = json_obj_get(body, "error");
    json_t *out = json_string("");
    if (e && e->type != JSON_NULL) {
        int truthy = 1;
        if (e->type == JSON_BOOL) truthy = e->bool_val ? 1 : 0;
        else if (e->type == JSON_NUMBER) truthy = (e->num_val != 0.0);
        else if (e->type == JSON_STRING) truthy = (e->str_val && *e->str_val);
        else if (e->type == JSON_ARRAY || e->type == JSON_OBJECT) truthy = (e->c.count > 0);
        if (truthy) {
            char buf[512];
            if (e->type == JSON_STRING) snprintf(buf, sizeof(buf), "%.299s", e->str_val ? e->str_val : "");
            else if (e->type == JSON_NUMBER) snprintf(buf, sizeof(buf), "%.0f", e->num_val);
            else if (e->type == JSON_BOOL) snprintf(buf, sizeof(buf), "%s", e->bool_val ? "True" : "False");
            else { char *d = json_dumps(e, 0); snprintf(buf, sizeof(buf), "%.299s", d ? d : ""); free(d); }
            json_free(out);
            out = json_string(buf);
        }
    }
    json_free(body);
    return out;
}

json_t *w11_codeexecut_b29647_tool_errors_since(const struct json_t *log, long long start) {
    json_t *out = json_array();
    if (!log || log->type != JSON_ARRAY) return out;
    long long n = (long long)json_len(log);
    long long s = start;
    if (s < 0) s = n + s;
    if (s < 0) s = 0;
    for (long long i = s; i < n && json_len(out) < 5; i++) {
        json_t *e = json_get(log, (size_t)i);
        if (!e || e->type != JSON_OBJECT) continue;
        json_t *err = json_obj_get(e, "error");
        int truthy = err && err->type != JSON_NULL;
        if (err && err->type == JSON_BOOL) truthy = err->bool_val ? 1 : 0;
        if (err && err->type == JSON_STRING) truthy = err->str_val && *err->str_val;
        if (!truthy) continue;
        json_t *row = json_object();
        json_set(row, "tool", json_copy(json_obj_get(e, "tool")));
        json_set(row, "error", json_copy(err));
        json_append(out, row);
    }
    return out;
}

static int returncode_nonzero(const json_t *result) {
    if (!result || result->type != JSON_OBJECT) return 1;
    json_t *rc = json_obj_get(result, "returncode");
    if (!rc) return 1; /* default 1 */
    if (rc->type == JSON_NUMBER) return rc->num_val != 0.0;
    return 1;
}

static json_t *exec_checked_result(const char *what, const json_t *result) {
    if (!returncode_nonzero(result)) return json_copy(result);
    json_t *o = json_obj_get(result, "output");
    char *repr = NULL;
    if (o && o->type == JSON_STRING) repr = py_repr(o->str_val);
    else if (!o) repr = strdup("None");
    else { char *d = json_dumps(o, 0); repr = d ? d : strdup("None"); if (d) free(d); }
    char msg[4096];
    snprintf(msg, sizeof(msg), "%s failed: %s", what ? what : "", repr ? repr : "None");
    free(repr);
    return raise_obj("RuntimeError", msg);
}

json_t *w11_codeexecut_b29647_execute_checked(const char *cmd, const char *what,
                                              long long timeout,
                                              const struct json_t *execute_result) {
    (void)cmd; (void)timeout; /* env.execute() is the injected result */
    return exec_checked_result(what, execute_result);
}

/* Standard base64 (RFC 4648) encode of a UTF-8 byte string. */
static char *b64_encode(const char *data) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t n = data ? strlen(data) : 0;
    size_t outcap = ((n + 2) / 3) * 4 + 1;
    char *out = (char *)malloc(outcap);
    size_t oi = 0;
    const unsigned char *p = (const unsigned char *)(data ? data : "");
    for (size_t i = 0; i < n; i += 3) {
        unsigned int b = p[i] << 16;
        if (i + 1 < n) b |= p[i + 1] << 8;
        if (i + 2 < n) b |= p[i + 2];
        out[oi++] = tbl[(b >> 18) & 0x3f];
        out[oi++] = tbl[(b >> 12) & 0x3f];
        out[oi++] = (i + 1 < n) ? tbl[(b >> 6) & 0x3f] : '=';
        out[oi++] = (i + 2 < n) ? tbl[b & 0x3f] : '=';
    }
    out[oi] = '\0';
    return out;
}

json_t *w11_codeexecut_b29647_remote_write(const char *remote_path, const char *content,
                                           int atomic, long long timeout, int check,
                                           const struct json_t *execute_result) {
    (void)timeout;
    char *encoded = b64_encode(content ? content : "");
    char *target = shlex_quote(remote_path ? remote_path : "");
    char write[8192];
    if (atomic)
        snprintf(write, sizeof(write), "base64 -d > %s.tmp && mv -f %s.tmp %s", target, target, target);
    else
        snprintf(write, sizeof(write), "base64 -d > %s", target);
    char cmd[16384];
    snprintf(cmd, sizeof(cmd), "umask 077 && %s", write);
    free(target);
    free(encoded);

    if (check) {
        char *rp = py_repr(remote_path ? remote_path : "");
        char what[8192];
        snprintf(what, sizeof(what), "remote file ship for %s", rp);
        free(rp);
        return exec_checked_result(what, execute_result);
    }
    return json_copy(execute_result);
}
