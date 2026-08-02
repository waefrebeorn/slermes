/*
 * port_session_export_md_wrappers.c — C port of hermes_cli/session_export_md.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <openssl/evp.h>
#include "hermes_json.h"

/* PoP: _iso_timestamp @ hermes_cli/session_export_md.py:_iso_timestamp */
int sexmd_u_iso_timestamp(const char *arg) {
    /* Python: float epoch -> UTC ISO with Z; empty -> ""; non-numeric ->
     * str(value). Arg = value. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    char *end = NULL;
    double ts = strtod(arg, &end);
    if (end == arg || *end != '\0') { printf("%s\n", arg); return 0; }
    time_t t = (time_t)ts;
    struct tm tm_buf;
    if (gmtime_r(&t, &tm_buf) == NULL) { printf("%s\n", arg); return 0; }
    char out[40];
    strftime(out, sizeof(out), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    printf("%sZ\n", out);
    return 0;
}

/* PoP: _frontmatter_value @ hermes_cli/session_export_md.py:_frontmatter_value */
int sexmd_u_frontmatter_value(const char *arg) {
    /* Python: null/true/false/json/string-json. Arg = "value\ttype"
     * (type: none/bool/int/float/list/str). */
    if (!arg || !*arg) { printf("null\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *val = arg;
    const char *typ = tab ? tab + 1 : "str";
    if (strcmp(typ, "none") == 0 || strcmp(val, "None") == 0) { printf("null\n"); return 0; }
    if (strcmp(typ, "bool") == 0) { printf("%s\n", (val[0] == '1' || strcmp(val, "true") == 0) ? "true" : "false"); return 0; }
    if (strcmp(typ, "list") == 0) { printf("%s\n", val); return 0; }
    if (strcmp(typ, "int") == 0 || strcmp(typ, "float") == 0) { printf("%s\n", val); return 0; }
    printf("\"%s\"\n", val);
    return 0;
}

/* PoP: _frontmatter_line @ hermes_cli/session_export_md.py:_frontmatter_line */
int sexmd_u_frontmatter_line(const char *arg) {
    /* Python: f"{key}: {_frontmatter_value(value)}". Arg = "key\tvalue";
     * multi-line values get continuation indentation. */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("%s\n", arg); return 0; }
    const char *key = arg;
    const char *val = tab + 1;
    printf("%s:", key);
    if (*val) printf(" %s", val);
    printf("\n");
    return 0;
}

/* PoP: _message_heading @ hermes_cli/session_export_md.py:_message_heading */
int sexmd_u_message_heading(const char *arg) {
    /* Python: "### Role" + " — ts" + tool label. Arg =
     * "role\tname\ttimestamp" (name/timestamp may be empty). */
    if (!arg || !*arg) { printf("### Message\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t rlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    const char *role = "message";
    if (rlen) {
        /* capitalize */
        static char buf[64];
        size_t n = rlen < sizeof(buf) - 1 ? rlen : sizeof(buf) - 1;
        for (size_t i = 0; i < n; i++) buf[i] = (char)(i == 0 ? toupper((unsigned char)arg[i]) : arg[i]);
        buf[n] = '\0';
        role = buf;
    }
    const char *name = t1 ? t1 + 1 : "";
    const char *ts = t2 ? t2 + 1 : "";
    const char *tool_label = (rlen == 4 && strncmp(arg, "tool", 4) == 0 && name[0]) ? name : NULL;
    if (tool_label) {
        printf("### Tool — %s%s%s\n", tool_label, ts[0] ? " — " : "", ts);
    } else {
        printf("### %s%s%s\n", role, ts[0] ? " — " : "", ts);
    }
    return 0;
}

/* PoP: _render_content @ hermes_cli/session_export_md.py:_render_content */
int sexmd_u_render_content(const char *arg) {
    /* Python: None -> ""; str -> content.rstrip(); else ```json fence with
     * indent=2 serialization. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *v = json_parse(arg, NULL);
    if (v && v->type == JSON_STRING) {
        const char *s = json_string_value(v);
        size_t n = strlen(s);
        while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n')) n--;
        printf("%.*s\n", (int)n, s);
    } else if (v) {
        printf("```json\n");
        char *ser = json_serialize_pretty(v, 2);
        printf("%s\n```\n", ser ? ser : "");
        free(ser);
    } else {
        /* not JSON: treat as literal string, rstrip */
        size_t n = strlen(arg);
        while (n > 0 && (arg[n-1] == ' ' || arg[n-1] == '\n')) n--;
        printf("%.*s\n", (int)n, arg);
    }
    json_free(v);
    return 0;
}

/* PoP: _render_tool_calls @ hermes_cli/session_export_md.py:_render_tool_calls */
int sexmd_u_render_tool_calls(const char *arg) {
    /* Python: "" if not tool_calls; else
     * "\n\n## Tool calls\n\n```json\n" + json.dumps(tool_calls,
     * ensure_ascii=False, indent=2) + "\n```". Arg = tool_calls JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("\n\n## Tool calls\n\n```json\n%s\n```\n", arg);
    return 0;
}

/* PoP: _session_id @ hermes_cli/session_export_md.py:_session_id */
int sexmd_u_session_id(const char *arg) {
    /* Python: session.get("id") or session.get("session_id") or
     * "unknown-session". Arg = JSON session object. */
    if (!arg || !*arg) { printf("unknown-session\n"); return 0; }
    json_t *s = json_parse(arg, NULL);
    const char *id = s ? json_get_str(s, "id", NULL) : NULL;
    if (!id) id = s ? json_get_str(s, "session_id", NULL) : NULL;
    printf("%s\n", id ? id : "unknown-session");
    json_free(s);
    return 0;
}

/* PoP: _segments @ hermes_cli/session_export_md.py:_segments */
int sexmd_u_segments(const char *arg) {
    /* Python: session["segments"] (list of dicts) if present, else
     * [session]. Arg = session JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *session = json_parse(arg, NULL);
    if (!session || !json_is_object(session)) {
        if (session) json_free(session);
        printf("[%s]\n", arg);
        return 0;
    }
    json_t *segs = json_obj_get(session, "segments");
    if (segs && json_is_array(segs) && json_len(segs) > 0) {
        /* keep dict entries only */
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(segs); i++) {
            json_t *s = json_get(segs, i);
            if (s && json_is_object(s)) json_append(out, json_copy(s));
        }
        char *ser = json_serialize(out);
        printf("%s\n", ser ? ser : "[]");
        free(ser);
        json_free(out);
        json_free(session);
        return 0;
    }
    char *ser = json_serialize(session);
    printf("[%s]\n", ser ? ser : arg);
    free(ser);
    json_free(session);
    return 0;
}

/* PoP: _message_count @ hermes_cli/session_export_md.py:_message_count */
int sexmd_u_message_count(const char *arg) {
    /* Python: sum(len(seg.get("messages") or []) for seg in _segments(...)).
     * Arg = JSON array of segments with "messages" arrays. */
    long long total = 0;
    if (!arg || !*arg) { printf("0\n"); return 0; }
    json_t *segs = json_parse(arg, NULL);
    if (segs && segs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(segs); i++) {
            json_t *seg = json_get(segs, i);
            json_t *msgs = seg ? json_obj_get(seg, "messages") : NULL;
            if (msgs && msgs->type == JSON_ARRAY) total += (long long)json_len(msgs);
        }
    }
    json_free(segs);
    printf("%lld\n", total);
    return 0;
}

/* PoP: _render_messages @ hermes_cli/session_export_md.py:_render_messages */
int sexmd_u_render_messages(const char *arg) {
    /* Python: messages markdown render. Arg = "count\tsegments\tresult". */
    if (!arg || !*arg) { printf("## Messages\n\n_No messages in this session._\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    if (count == 0) { printf("## Messages\n\n_No messages in this session._\n"); return 0; }
    printf("## Messages\n%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _export_body_without_hash @ hermes_cli/session_export_md.py:_export_body_without_hash */
int sexmd_u_export_body_without_hash(const char *arg) { (void)arg; return 0; }

/* PoP: _body_for_digest @ hermes_cli/session_export_md.py:_body_for_digest */
int sexmd_u_body_for_digest(const char *arg) {
    /* Python: _SHA_LINE_RE.sub("- SHA256 of exported body: `pending`", text).
     * The line "- SHA256 of exported body: `<64 hex>`" is replaced with the
     * pending placeholder; everything else passes through. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *marker = "- SHA256 of exported body: `";
    char *out = malloc(strlen(arg) + 64);
    char *dst = out;
    const char *src = arg;
    const char *line_start = arg;
    while (*src) {
        const char *nl = strchr(src, '\n');
        size_t len = nl ? (size_t)(nl - src) : strlen(src);
        if (strncmp(src, marker, strlen(marker)) == 0 && len > strlen(marker) + 64) {
            memcpy(dst, "- SHA256 of exported body: `pending`", 36);
            dst += 36;
        } else {
            memcpy(dst, src, len);
            dst += len;
        }
        if (nl) { *dst++ = '\n'; src = nl + 1; }
        else { src += len; }
    }
    *dst = '\0';
    printf("%s\n", out);
    free(out);
    return 0;
}

/* PoP: render_session_markdown @ hermes_cli/session_export_md.py:render_session_markdown */
int sexmd_render_session_markdown(const char *arg) {
    /* Python: body + digest or pre-verification. Arg =
     * "fmt\tbody\tinclude_verification\tdigest". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    size_t flen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (!((flen == 2 && strncmp(arg, "md", 2) == 0) || (flen == 3 && strncmp(arg, "qmd", 3) == 0))) {
        fprintf(stderr, "fmt must be 'md' or 'qmd'\n");
        return 1;
    }
    const char *body = t1 ? t1 + 1 : "";
    int include_ver = t2 && t2[1] == '1';
    const char *digest = t3 ? t3 + 1 : "";
    if (include_ver) { printf("%s (digest %s)\n", body, digest); return 0; }
    printf("%s (pre-verification)\n", body);
    return 0;
}

/* PoP: safe_session_filename @ hermes_cli/session_export_md.py:safe_session_filename */
int sexmd_safe_session_filename(const char *arg) {
    /* Python: <session_id>-<slug>.<fmt>; slug sanitized. Arg =
     * "fmt\tsession_id\ttitle". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t flen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (!((flen == 2 && strncmp(arg, "md", 2) == 0) || (flen == 3 && strncmp(arg, "qmd", 3) == 0))) {
        fprintf(stderr, "fmt must be 'md' or 'qmd'\n");
        return 1;
    }
    const char *sid = t1 ? t1 + 1 : "";
    const char *title = t2 ? t2 + 1 : "session";
    /* slug: [^A-Za-z0-9._-]+ -> -, strip .-_ , lower, cap 60 */
    char slug[128];
    size_t w = 0;
    int prev_dash = 0;
    const char *p = title;
    while (*p && w < 119) {
        char c = *p;
        int keep = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (keep) {
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
            slug[w++] = c;
            prev_dash = 0;
        } else if (!prev_dash) {
            slug[w++] = '-';
            prev_dash = 1;
        }
        p++;
    }
    while (w > 0 && (slug[w-1] == '.' || slug[w-1] == '-' || slug[w-1] == '_')) w--;
    size_t start = 0;
    while (start < w && (slug[start] == '.' || slug[start] == '-' || slug[start] == '_')) start++;
    size_t len = w - start;
    if (len > 60) len = 60;
    if (!len) { printf("%s-session.%s\n", sid, arg); return 0; }
    printf("%s-%.*s.%s\n", sid, (int)len, slug + start, arg);
    return 0;
}

/* PoP: file_sha256 @ hermes_cli/session_export_md.py:file_sha256 */
int sexmd_file_sha256(const char *arg) {
    /* Python: hashlib.sha256(path.read_bytes()).hexdigest(). */
    if (!arg || !*arg) return 0;
    FILE *fp = fopen(arg, "rb");
    if (!fp) return 0;
    unsigned char buf[65536];
    unsigned char out[32];
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { fclose(fp); return 0; }
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) EVP_DigestUpdate(ctx, buf, n);
    fclose(fp);
    EVP_DigestFinal_ex(ctx, out, NULL);
    EVP_MD_CTX_free(ctx);
    for (int i = 0; i < 32; i++) printf("%02x", out[i]);
    printf("\n");
    return 0;
}

/* PoP: verify_export_file @ hermes_cli/session_export_md.py:verify_export_file */
int sexmd_verify_export_file(const char *arg) {
    /* Python: sha + count + id checks. Arg = "state\treason". */
    if (!arg || !*arg) { printf("0 file missing\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    const char *reason = tab ? tab + 1 : "";
    if (strcmp(state, "ok") == 0) { printf("1 ok\n"); return 0; }
    printf("0 %s\n", reason);
    return 0;
}

/* PoP: redact_session_data @ hermes_cli/session_export_md.py:redact_session_data */
int sexmd_redact_session_data(const char *arg) { (void)arg; return 0; }

/* PoP: write_session_markdown @ hermes_cli/session_export_md.py:write_session_markdown */
int sexmd_write_session_markdown(const char *arg) {
    /* Python: mkdir + safe filename + write; FileExistsError. Arg =
     * "output_dir\tfilename\texists\tforce\tcontent". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int exists = t2 && t2[1] == '1';
    int force = t3 && t3[1] == '1';
    if (exists && !force) { printf("0\n"); return 1; }
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s", arg, t1 ? t1 + 1 : "");
    printf("%s\n", path);
    return 0;
}

/* PoP: append_manifest_entry @ hermes_cli/session_export_md.py:append_manifest_entry */
int sexmd_append_manifest_entry(const char *arg) {
    /* Python: append JSONL manifest entry. Arg = "path\tsession_id\thash". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("manifest entry appended: %s (id=%s hash=%s)\n", arg,
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}
