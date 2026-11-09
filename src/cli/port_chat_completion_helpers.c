/*
 * port_chat_completion_helpers.c
 *
 * Pure helpers ported from agent/chat_completion_helpers.py (104 defs,
 * previously 0 ported).
 *
 *   cch_status_code_from_value    <- _status_code_from_value
 *   cch_status_code_from_payload  <- _status_code_from_payload
 *   cch_json_object_from_text     <- _json_object_from_text
 *   cch_payload_has_error_shape   <- _payload_has_error_shape
 *   cch_parse_provider_sse_events <- _parse_provider_sse_events
 *
 * The stream/contextvars half stays in Python - it is asyncio and SDK I/O.
 */

#include "port_chat_completion_helpers.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "../lib/libregex/hermes_regex.h"

/* chat_completion_helpers.py's inline pattern:
 *   re.search(r"(?:HTTP_STATUS/)?\b([1-5]\d\d)\b", value, re.IGNORECASE)
 * REGEX_ICASE is the wrapper's flag 1 (see hermes_regex.c: flags & 1). */
#define CCH_ICASE 1

/* ===========================================================================
 *  _status_code_from_value  (chat_completion_helpers.py:_status_code_from_value)
 *
 *      if isinstance(value, int) and 100 <= value < 600: return value
 *      if not isinstance(value, str): return None
 *      match = re.search(r"(?:HTTP_STATUS/)?\b([1-5]\d\d)\b", value, I)
 *      return int(match.group(1)) if match else None
 *
 *  `kind` carries the Python type: 0 = not int/str, 1 = int, 2 = str.
 *  Returns the code, or -1 for None.
 * =========================================================================== */

/* PoP: _status_code_from_value @ agent/chat_completion_helpers.py:_status_code_from_value */
int cch_status_code_from_value(int kind, long long ival, const char *sval)
{
    if (kind == 1) {
        return (ival >= 100 && ival < 600) ? (int)ival : -1;
    }
    if (kind != 2 || !sval) return -1;

    /* tre (the backend libregex wraps) supports neither (?:...) nor \b, so
     * the optional HTTP_STATUS/ prefix is dropped - it does not affect the
     * captured code - and the word boundaries are checked by hand below.
     *
     * NOTE: regex_match_t.group_count is never populated by hermes_regex.c
     * (it stays 0 from calloc), so the groups must be tested directly. */
    static hregex_t *re = NULL;
    if (!re) re = regex_compile("([1-5][0-9][0-9])", CCH_ICASE);
    if (!re) return -1;

    regex_match_t *m = regex_search(re, sval);
    if (!m || !m->matched) { if (m) regex_match_free(m); return -1; }
    int code = -1;
    if (m->groups[1]) {
        const char *hit = strstr(sval, m->groups[1]);
        size_t clen = strlen(m->groups[1]);
        size_t pos = hit ? (size_t)(hit - sval) : 0;
        int boundary_ok = 1;
        if (hit) {
            if (pos > 0) {
                unsigned char before = (unsigned char)sval[pos - 1];
                if (isalnum(before) || before == '_') boundary_ok = 0;
            }
            unsigned char after = (unsigned char)sval[pos + clen];
            if (after && (isalnum(after) || after == '_')) boundary_ok = 0;
        }
        if (boundary_ok) code = atoi(m->groups[1]);
    }
    regex_match_free(m);
    return code;
}

/* ===========================================================================
 *  _status_code_from_payload  (chat_completion_helpers.py:_status_code_from_payload)
 *
 *      if not isinstance(payload, dict): return None
 *      candidates = [payload.get(k) for k in
 *                    ("status_code", "status", "http_status")]
 *      error_obj = payload.get("error")
 *      if isinstance(error_obj, dict):
 *          candidates.extend(error_obj.get(k) for k in
 *                           ("status_code", "status", "http_status", "code"))
 *      candidates.append(payload.get("code"))
 *      for candidate in candidates:
 *          status_code = _status_code_from_value(candidate)
 *          if status_code is not None: return status_code
 *      return None
 *
 *  `payload` is a json_t object (or NULL for a non-dict).
 * =========================================================================== */

/* PoP: _status_code_from_payload @ agent/chat_completion_helpers.py:_status_code_from_payload */
int cch_status_code_from_payload(const json_t *payload)
{
    if (!payload || payload->type != JSON_OBJECT) return -1;

    static const char *const keys[] = { "status_code", "status", "http_status" };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        json_t *v = json_object_get((json_t *)payload, keys[i]);
        int code = cch_status_code_from_value(
            v ? (v->type == JSON_NUMBER ? 1 : (v->type == JSON_STRING ? 2 : 0)) : 0,
            v ? (long long)v->num_val : 0,
            (v && v->type == JSON_STRING) ? v->str_val : NULL);
        if (code >= 0) return code;
    }

    json_t *err = json_object_get((json_t *)payload, "error");
    if (err && err->type == JSON_OBJECT) {
        static const char *const ekeys[] = { "status_code", "status", "http_status", "code" };
        for (size_t i = 0; i < sizeof(ekeys) / sizeof(ekeys[0]); i++) {
            json_t *v = json_object_get(err, ekeys[i]);
            int code = cch_status_code_from_value(
                v ? (v->type == JSON_NUMBER ? 1 : (v->type == JSON_STRING ? 2 : 0)) : 0,
                v ? (long long)v->num_val : 0,
                (v && v->type == JSON_STRING) ? v->str_val : NULL);
            if (code >= 0) return code;
        }
    }

    json_t *c = json_object_get((json_t *)payload, "code");
    return cch_status_code_from_value(
        c ? (c->type == JSON_NUMBER ? 1 : (c->type == JSON_STRING ? 2 : 0)) : 0,
        c ? (long long)c->num_val : 0,
        (c && c->type == JSON_STRING) ? c->str_val : NULL);
}

/* ===========================================================================
 *  _json_object_from_text  (chat_completion_helpers.py:_json_object_from_text)
 *
 *      stripped = (text or "").strip()
 *      with contextlib.suppress(json.JSONDecodeError, TypeError):
 *          if stripped.startswith("{"):
 *              decoded = json.loads(stripped)
 *              return decoded if isinstance(decoded, dict) else None
 *      return None
 *
 *  Caller frees the returned json_t.
 * =========================================================================== */

/* PoP: _json_object_from_text @ agent/chat_completion_helpers.py:_json_object_from_text */
json_t *cch_json_object_from_text(const char *text)
{
    if (!text) return NULL;
    while (*text && isspace((unsigned char)*text)) text++;
    if (*text != '{') return NULL;

    json_t *decoded = json_parse(text, NULL);
    if (!decoded) return NULL;                       /* JSONDecodeError -> None */
    if (decoded->type != JSON_OBJECT) { json_free(decoded); return NULL; }
    return decoded;
}

/* ===========================================================================
 *  _payload_has_error_shape  (chat_completion_helpers.py:_payload_has_error_shape)
 *
 *      if not isinstance(payload, dict): return False
 *      if isinstance(payload.get("error"), (dict, str)): return True
 *      return bool(payload.get("message")) and bool(
 *          payload.get("code") or payload.get("error_code")
 *          or _status_code_from_payload(payload) is not None)
 *
 *  Note the truthiness chain: a message ALONE is not enough; it needs a code,
 *  an error_code, or a parseable status code.
 * =========================================================================== */

/* PoP: _payload_has_error_shape @ agent/chat_completion_helpers.py:_payload_has_error_shape */
int cch_payload_has_error_shape(const json_t *payload)
{
    if (!payload || payload->type != JSON_OBJECT) return 0;

    json_t *err = json_object_get((json_t *)payload, "error");
    if (err && (err->type == JSON_OBJECT || err->type == JSON_STRING)) return 1;

    json_t *msg = json_object_get((json_t *)payload, "message");
    int has_message = (msg && msg->type == JSON_STRING && msg->str_val && msg->str_val[0]);
    if (!has_message) return 0;

    json_t *code = json_object_get((json_t *)payload, "code");
    json_t *ecode = json_object_get((json_t *)payload, "error_code");
    int has_code = (code && ((code->type == JSON_NUMBER && code->num_val != 0)
                          || (code->type == JSON_STRING && code->str_val && code->str_val[0])))
                || (ecode && ((ecode->type == JSON_NUMBER && ecode->num_val != 0)
                           || (ecode->type == JSON_STRING && ecode->str_val && ecode->str_val[0])));
    if (has_code) return 1;
    return cch_status_code_from_payload(payload) >= 0;
}

/* ===========================================================================
 *  _parse_provider_sse_events  (chat_completion_helpers.py:_parse_provider_sse_events)
 *
 *  SSE with the WHATWG field rules:
 *    - an empty line flushes the current event
 *    - a line starting with ":" is a comment (line[1:].strip())
 *    - otherwise partition on the FIRST ":"; no colon means field -> ""
 *    - the value has ONE leading space removed (removeprefix(" "))
 *    - field names are lowercased
 *    - "event" and "data" are special; everything else lands in fields
 *    - a flush emits only when any of event/data/comments/fields is non-empty
 *    - status_code is the first candidate (comments, then fields
 *      status/status_code/http_status) that parses as a status
 *
 *  Returns a malloc'd NULL-terminated array of json_t* events.
 *  Caller frees each event and the array.
 * =========================================================================== */

static void cch_sse_flush(json_t **events, int *n, const char *event,
                          char *const *data, int ndata,
                          char *const *comments, int ncomments,
                          char *const *fkeys, char *const *fvals, int nfields)
{
    if (!event && ndata == 0 && ncomments == 0 && nfields == 0) return;

    json_t *e = json_object();
    if (event) json_set(e, "event", json_string(event)); else json_set(e, "event", json_null());

    /* "\n".join(data) */
    size_t cap = 1;
    for (int i = 0; i < ndata; i++) cap += strlen(data[i]) + 1;
    char *joined = (char *)malloc(cap);
    if (joined) {
        joined[0] = '\0';
        for (int i = 0; i < ndata; i++) {
            if (i) strcat(joined, "\n");
            strcat(joined, data[i]);
        }
        json_set(e, "data", json_string(joined));
        free(joined);
    } else {
        json_set(e, "data", json_string(""));
    }

    json_t *arr = json_array();
    for (int i = 0; i < ncomments; i++) json_append(arr, json_string(comments[i]));
    json_set(e, "comments", arr);

    json_t *fields = json_object();
    for (int i = 0; i < nfields; i++) json_set(fields, fkeys[i], json_string(fvals[i]));
    json_set(e, "fields", fields);

    /* status_code: comments first, then the three known field names */
    int status = -1;
    for (int i = 0; i < ncomments && status < 0; i++) {
        status = cch_status_code_from_value(2, 0, comments[i]);
    }
    static const char *const skeys[] = { "status", "status_code", "http_status" };
    for (size_t k = 0; k < sizeof(skeys) / sizeof(skeys[0]) && status < 0; k++) {
        for (int i = 0; i < nfields; i++) {
            if (strcmp(fkeys[i], skeys[k]) == 0) {
                status = cch_status_code_from_value(2, 0, fvals[i]);
                break;
            }
        }
    }
    json_set(e, "status_code", status >= 0 ? json_number(status) : json_null());

    events[*n] = e;
    (*n)++;
}

/* PoP: _parse_provider_sse_events @ agent/chat_completion_helpers.py:_parse_provider_sse_events */
json_t **cch_parse_provider_sse_events(const char *text, int *out_count)
{
    json_t **events = (json_t **)calloc(64, sizeof(json_t *));
    if (!events) { if (out_count) *out_count = 0; return NULL; }
    int n = 0;

    const char *event = NULL;
    const char *data[256];   int ndata = 0;
    const char *comments[64]; int ncomments = 0;
    const char *fkeys[128];  const char *fvals[128]; int nfields = 0;

    /* the caller owns these; they live for the duration of the parse */
    char *event_buf = NULL;
    char *data_buf[256];
    char *comment_buf[64];
    char *fkey_buf[128], *fval_buf[128];

    if (!text) text = "";
    const char *p = text;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        char *line = (char *)malloc(len + 1);
        if (!line) break;
        memcpy(line, p, len);
        line[len] = '\0';
        /* line.rstrip("\r") */
        size_t ll = strlen(line);
        while (ll > 0 && line[ll - 1] == '\r') line[--ll] = '\0';

        if (line[0] == '\0') {
            cch_sse_flush(events, &n, event_buf, data_buf, ndata, comment_buf, ncomments,
                          fkey_buf, fval_buf, nfields);
            free(event_buf); event_buf = NULL;
            for (int i = 0; i < ndata; i++) free(data_buf[i]);
            for (int i = 0; i < ncomments; i++) free(comment_buf[i]);
            for (int i = 0; i < nfields; i++) { free(fkey_buf[i]); free(fval_buf[i]); }
            ndata = ncomments = nfields = 0;
        } else if (line[0] == ':') {
            /* line[1:].strip() */
            char *c = line + 1;
            while (*c && isspace((unsigned char)*c)) c++;
            size_t cl = strlen(c);
            while (cl > 0 && isspace((unsigned char)c[cl - 1])) c[--cl] = '\0';
            comment_buf[ncomments++] = strdup(c);
        } else {
            char *sep = strchr(line, ':');
            if (!sep) {
                /* field with no colon -> "" */
                char *f = strdup(line);
                size_t fl = strlen(f);
                while (fl > 0 && isspace((unsigned char)f[fl - 1])) f[--fl] = '\0';
                for (size_t i = 0; f[i]; i++) f[i] = (char)tolower((unsigned char)f[i]);
                fkey_buf[nfields] = f;
                fval_buf[nfields] = strdup("");
                nfields++;
            } else {
                *sep = '\0';
                char *field = line;
                char *value = sep + 1;
                size_t fl = strlen(field);
                while (fl > 0 && isspace((unsigned char)field[fl - 1])) field[--fl] = '\0';
                for (size_t i = 0; field[i]; i++) field[i] = (char)tolower((unsigned char)field[i]);
                /* removeprefix(" ") - exactly ONE leading space */
                if (*value == ' ') value++;

                if (strcmp(field, "event") == 0) {
                    while (*value && isspace((unsigned char)*value)) value++;
                    size_t vl = strlen(value);
                    while (vl > 0 && isspace((unsigned char)value[vl - 1])) value[--vl] = '\0';
                    free(event_buf);
                    event_buf = strdup(value);
                } else if (strcmp(field, "data") == 0) {
                    data_buf[ndata++] = strdup(value);
                } else {
                    fkey_buf[nfields] = strdup(field);
                    fval_buf[nfields] = strdup(value);
                    nfields++;
                }
            }
        }
        free(line);
        if (!eol) break;
        p = eol + 1;
    }
    cch_sse_flush(events, &n, event_buf, data_buf, ndata, comment_buf, ncomments,
                  fkey_buf, fval_buf, nfields);

    free(event_buf);
    for (int i = 0; i < ndata; i++) free(data_buf[i]);
    for (int i = 0; i < ncomments; i++) free(comment_buf[i]);
    for (int i = 0; i < nfields; i++) { free(fkey_buf[i]); free(fval_buf[i]); }

    if (out_count) *out_count = n;
    return events;
}
