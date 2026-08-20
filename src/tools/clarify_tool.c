/*
 * clarify_tool.c — Interactive clarifying-question schema, validation, and
 * batch dispatch for the clarify tool.
 * PoP: clarify_tool @ tools/clarify_tool.py
 *
 * Faithful port of tools/clarify_tool.py. The platform UI callback is injected
 * by the caller (mirrors the Python `callback` parameter); all schema /
 * validation / normalization / result-JSON assembly is implemented in C11.
 */

 #ifndef _POSIX_C_SOURCE
 #define _POSIX_C_SOURCE 200809L
 #endif

 #include "clarify_tool.h"

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <strings.h>
 #include <ctype.h>

 #include "json.h"

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void str_strip_inplace(char *s) {
    if (!s) return;
    char *a = s, *b = s + strlen(s);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    if (a != s) memmove(s, a, b - a);
    s[b - a] = '\0';
}

/* PoP: _flatten_choice @ tools/clarify_tool.py:_flatten_choice */
/* Coerce one choice (a JSON-encoded value: str | dict | list) into its
 * user-facing display string. Returns a heap string (caller frees) or NULL. */
char *clarify_flatten_choice(const char *choice_json) {
    if (!choice_json) return xstrdup("");
    char *dup = xstrdup(choice_json);
    char *err = NULL;
    json_t *node = json_parse(dup, &err);
    free(dup);
    if (err) { free(err); }
    if (!node) return xstrdup("");

    char *result = NULL;
    if (node->type == JSON_STRING) {
        result = xstrdup(node->str_val ? node->str_val : "");
    } else if (node->type == JSON_OBJECT) {
        static const char *keys[] = {"label", "description", "text", "title"};
        for (size_t i = 0; i < 4; i++) {
            json_t *v = json_obj_get(node, keys[i]);
            if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) {
                result = xstrdup(v->str_val);
                break;
            }
        }
        if (!result) result = xstrdup("");
    } else if (node->type == JSON_ARRAY) {
        /* join each element's flatten with a space */
        size_t n = json_len(node);
        char *joined = NULL;
        size_t cap = 0, len = 0;
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_get(node, i);
            char buf[64];
            const char *es = NULL;
            if (e && e->type == JSON_STRING) es = e->str_val;
            else if (e) { snprintf(buf, sizeof buf, "%g", e->num_val); es = buf; }
            if (!es) continue;
            size_t add = strlen(es);
            if (len + add + 2 > cap) { cap = len + add + 64; joined = realloc(joined, cap); }
            if (len) joined[len++] = ' ';
            memcpy(joined + len, es, add);
            len += add;
        }
        if (!joined) joined = xstrdup("");
        else joined[len] = '\0';
        /* strip */
        str_strip_inplace(joined);
        result = joined;
    } else {
        /* scalar: stringify */
        char buf[64];
        if (node->type == JSON_NUMBER) snprintf(buf, sizeof buf, "%g", node->num_val);
        else if (node->type == JSON_BOOL) snprintf(buf, sizeof buf, "%s", node->bool_val ? "true" : "false");
        else buf[0] = '\0';
        result = xstrdup(buf);
    }
    json_free(node);
    if (result) str_strip_inplace(result);
    return result;
}

/* PoP: mark_recommended @ tools/clarify_tool.py:mark_recommended */
/* Label the first choice as the agent's recommendation (idempotent).
 * Returns a newly allocated array of `count` heap strings (caller frees). */
char **clarify_mark_recommended(char **choices, size_t count) {
    char **out = calloc(count ? count : 1, sizeof(char *));
    if (!out) return NULL;
    if (count < 2) {
        for (size_t i = 0; i < count; i++) out[i] = xstrdup(choices[i] ? choices[i] : "");
        return out;
    }
    char *first = xstrdup(choices[0] ? choices[0] : "");
    str_strip_inplace(first);
    /* already recommended? leave untouched */
    size_t labellen = strlen(CLARIFY_RECOMMENDED_LABEL);
    size_t flen = strlen(first);
    bool already = flen >= labellen &&
        strcasecmp(first + flen - labellen, CLARIFY_RECOMMENDED_LABEL) == 0;
    if (already) {
        free(first);
        for (size_t i = 0; i < count; i++) out[i] = xstrdup(choices[i] ? choices[i] : "");
        return out;
    }
    size_t total = flen + 1 + labellen + 1;
    char *decorated = malloc(total);
    snprintf(decorated, total, "%s %s", first, CLARIFY_RECOMMENDED_LABEL);
    free(first);
    out[0] = decorated;
    for (size_t i = 1; i < count; i++) out[i] = xstrdup(choices[i] ? choices[i] : "");
    return out;
}

/* PoP: strip_recommended @ tools/clarify_tool.py:strip_recommended */
char *clarify_strip_recommended(const char *text) {
    if (!text) return xstrdup("");
    char *s = xstrdup(text);
    str_strip_inplace(s);
    size_t labellen = strlen(CLARIFY_RECOMMENDED_LABEL);
    size_t flen = strlen(s);
    if (flen >= labellen) {
        char *tail = s + flen - labellen;
        if (strcasecmp(tail, CLARIFY_RECOMMENDED_LABEL) == 0) {
            tail[0] = '\0';
            str_strip_inplace(s);
        }
    }
    return s;
}

/* PoP: _parse_multi_select_response @ tools/clarify_tool.py:_parse_multi_select_response */
/* Parse a multi-select response into a NULL-terminated array of heap strings. */
char **clarify_parse_multi_select_response(const char *raw_response) {
    /* fixed cap; grows if needed */
    size_t cap = 8, n = 0;
    char **out = calloc(cap, sizeof(char *));
    if (!out) return NULL;

    if (raw_response && (raw_response[0] == '[')) {
        char *err = NULL;
        json_t *node = json_parse(raw_response, &err);
        if (err) free(err);
        if (node && node->type == JSON_ARRAY) {
            size_t len = json_len(node);
            for (size_t i = 0; i < len; i++) {
                json_t *e = json_get(node, i);
                const char *es = (e && e->type == JSON_STRING) ? e->str_val : NULL;
                if (!es) continue;
                char *s = xstrdup(es);
                str_strip_inplace(s);
                if (!s[0]) { free(s); continue; }
                if (n + 1 >= cap) { cap *= 2; out = realloc(out, cap * sizeof(char *)); }
                out[n++] = s;
            }
            out[n] = NULL;
            json_free(node);
            return out;
        }
        if (node) json_free(node);
    }

    /* comma-separated fallback */
    if (!raw_response) { out[0] = NULL; return out; }
    char *dup = xstrdup(raw_response);
    char *save = NULL;
    for (char *tok = strtok_r(dup, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        char *s = xstrdup(tok);
        str_strip_inplace(s);
        if (!s[0]) { free(s); continue; }
        if (n + 1 >= cap) { cap *= 2; out = realloc(out, cap * sizeof(char *)); }
        out[n++] = s;
    }
    out[n] = NULL;
    free(dup);
    return out;
}

/* ------------------------------------------------------------------ */
/* batch normalization                                                 */
/* ------------------------------------------------------------------ */

/* PoP: _normalize_questions @ tools/clarify_tool.py:_normalize_questions */
/* Returns a JSON array string (the normalized list) on success, an error JSON
 * string on failure, or the literal "null" for the empty-list case. */
char *clarify_normalize_questions(const char *questions_json) {
    if (!questions_json) return xstrdup("null");
    char *err = NULL;
    json_t *node = json_parse(questions_json, &err);
    if (err) free(err);
    if (!node) return xstrdup("{\"error\": \"questions must be an array of question objects.\"}");
    if (node->type != JSON_ARRAY) {
        json_free(node);
        return xstrdup("{\"error\": \"questions must be an array of question objects.\"}");
    }
    size_t n = json_len(node);
    if (n == 0) {
        json_free(node);
        return xstrdup("null");
    }
    if (n > CLARIFY_MAX_QUESTIONS) {
        json_free(node);
        char buf[128];
        snprintf(buf, sizeof buf, "{\"error\": \"questions supports at most %d items.\"}", CLARIFY_MAX_QUESTIONS);
        return xstrdup(buf);
    }

    json_t *norm = json_array();
    for (size_t index = 0; index < n; index++) {
        json_t *item = json_get(node, index);
        if (item && item->type == JSON_STRING) {
            json_t *o = json_object();
            json_set(o, "question", json_string(item->str_val));
            json_set(o, "multi_select", json_bool(false));
            json_set(o, "choices", json_null());
            json_set(o, "choices_offered", json_null());
            json_set(o, "id", json_null());
            char qid[8];
            snprintf(qid, sizeof qid, "q%zu", index);
            json_set(o, "qid", json_string(qid));
            json_append(norm, o);
            continue;
        }
        if (!item || item->type != JSON_OBJECT) {
            char buf[128];
            snprintf(buf, sizeof buf, "{\"error\": \"questions[%zu] must be an object with a 'question'.\"}", index);
            char *r = xstrdup(buf);
            json_free(norm); json_free(node);
            return r;
        }
        json_t *qnode = json_obj_get(item, "question");
        const char *text = (qnode && qnode->type == JSON_STRING) ? qnode->str_val : "";
        char *text_d = xstrdup(text ? text : "");
        str_strip_inplace(text_d);
        if (!text_d[0]) {
            char buf[128];
            snprintf(buf, sizeof buf, "{\"error\": \"questions[%zu].question must be non-empty text.\"}", index);
            char *r = xstrdup(buf);
            free(text_d);
            json_free(norm); json_free(node);
            return r;
        }

        char **choices = NULL;
        size_t ccount = 0;
        json_t *choices_node = json_obj_get(item, "choices");
        bool has_choices = (choices_node != NULL);
        if (choices_node) {
            if (choices_node->type != JSON_ARRAY) {
                char buf[128];
                snprintf(buf, sizeof buf, "{\"error\": \"questions[%zu].choices must be a list.\"}", index);
                char *r = xstrdup(buf);
                free(text_d);
                json_free(norm); json_free(node);
                return r;
            }
            size_t cn = json_len(choices_node);
            choices = calloc(cn ? cn : 1, sizeof(char *));
            for (size_t i = 0; i < cn; i++) {
                json_t *c = json_get(choices_node, i);
                /* serialize element back to a JSON value string and flatten */
                char *cj = (c) ? json_serialize(c) : xstrdup("null");
                char *flat = clarify_flatten_choice(cj);
                free(cj);
                if (flat && flat[0]) choices[ccount++] = flat;
            }
            if (ccount > CLARIFY_MAX_CHOICES) ccount = CLARIFY_MAX_CHOICES;
            if (ccount == 0) { for (size_t i = 0; i < cn; i++) free(choices[i]); free(choices); choices = NULL; has_choices = false; }
        }

        json_t *id_node = json_obj_get(item, "id");
        const char *model_id = (id_node && id_node->type == JSON_STRING) ? id_node->str_val : "";
        char *model_id_d = xstrdup(model_id ? model_id : "");
        str_strip_inplace(model_id_d);
        if (!model_id_d[0]) { free(model_id_d); model_id_d = NULL; }

        json_t *o = json_object();
        char qid[8];
        snprintf(qid, sizeof qid, "q%zu", index);
        json_set(o, "qid", json_string(qid));
        json_set(o, "id", model_id_d ? json_string(model_id_d) : json_null());
        json_set(o, "question", json_string(text_d));

        json_t *ms_node = json_obj_get(item, "multi_select");
        bool ms = (ms_node && ms_node->type == JSON_BOOL) ? ms_node->bool_val : false;

        if (has_choices) {
            /* build recommended-decorated + bare offered arrays */
            char **decorated = clarify_mark_recommended(choices, ccount);
            json_t *offered = json_array();
            json_t *decor = json_array();
            for (size_t i = 0; i < ccount; i++) {
                if (choices[i]) json_append(offered, json_string(choices[i]));
                if (decorated[i]) json_append(decor, json_string(decorated[i]));
                free(decorated[i]);
            }
            free(decorated);
            json_set(o, "choices", decor);
            json_set(o, "choices_offered", offered);
            json_set(o, "multi_select", json_bool(ms));
        } else {
            json_set(o, "choices", json_null());
            json_set(o, "choices_offered", json_null());
            json_set(o, "multi_select", json_bool(false));
        }
        json_append(norm, o);

        if (choices) { for (size_t i = 0; i < ccount; i++) free(choices[i]); free(choices); }
        free(text_d);
        free(model_id_d);
    }

    char *result = json_serialize(norm);
    json_free(norm);
    json_free(node);
    return result;
}
/* entry_json: one normalized question object. raw: the raw answer (or NULL). */
/* PoP: clarify_clean_batch_answer @ tools/clarify_tool.py:_clean_batch_answer */
char *clarify_clean_batch_answer(const char *entry_json, const char *raw) {
    char *err = NULL;
    json_t *entry = json_parse(entry_json, &err);
    if (err) free(err);
    if (!entry) return xstrdup("");
    json_t *ms = json_obj_get(entry, "multi_select");
    bool multi = (ms && ms->type == JSON_BOOL) ? ms->bool_val : false;
    char *result;
    if (multi) {
        char **parts = clarify_parse_multi_select_response(raw ? raw : "");
        json_t *arr = json_array();
        for (size_t i = 0; parts[i]; i++) {
            char *clean = clarify_strip_recommended(parts[i]);
            json_append(arr, json_string(clean));
            free(clean);
            free(parts[i]);
        }
        free(parts);
        result = json_serialize(arr);
        json_free(arr);
    } else {
        char *clean = clarify_strip_recommended(raw ? raw : "");
        json_t *node = json_string(clean);
        result = json_serialize(node);
        json_free(node);
        free(clean);
    }
    json_free(entry);
    return result;
}
/* answers_json: a JSON object {qid: raw}. */
/* PoP: clarify_batch_result @ tools/clarify_tool.py:_batch_result */
char *clarify_batch_result(const char *normalized_json, const char *answers_json, bool timed_out) {
    char *nerr = NULL;
    json_t *norm = json_parse(normalized_json, &nerr);
    if (nerr) free(nerr);
    char *aerr = NULL;
    json_t *answers = answers_json ? json_parse(answers_json, &aerr) : NULL;
    if (aerr) free(aerr);

    json_t *responses = json_array();
    if (norm && norm->type == JSON_ARRAY) {
        size_t n = json_len(norm);
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_get(norm, i);
            if (!e || e->type != JSON_OBJECT) continue;
            json_t *row = json_object();
            json_t *idn = json_obj_get(e, "id");
            if (idn && idn->type == JSON_STRING && idn->str_val && idn->str_val[0])
                json_set(row, "id", json_string(idn->str_val));
            json_t *qn = json_obj_get(e, "question");
            json_set(row, "question", json_string((qn && qn->type == JSON_STRING && qn->str_val) ? qn->str_val : ""));
            json_t *co = json_obj_get(e, "choices_offered");
            if (co) json_set(row, "choices_offered", json_copy(co));
            else json_set(row, "choices_offered", json_null());
            json_t *qidn = json_obj_get(e, "qid");
            const char *qid = (qidn && qidn->type == JSON_STRING) ? qidn->str_val : "";
            const char *raw = NULL;
            if (answers && answers->type == JSON_OBJECT) {
                json_t *an = json_obj_get(answers, qid);
                if (an && an->type == JSON_STRING) raw = an->str_val;
            }
            char *clean;
            if (raw) {
                char *entry_json = json_serialize(e);
                clean = clarify_clean_batch_answer(entry_json, raw);
                free(entry_json);
            } else clean = xstrdup("\"\"");
            /* clean is a JSON value string; embed directly. json_set adopts
             * clean_node, so do NOT free it (freeing the serialized string is
             * still required). */
            json_t *clean_node = json_parse(clean, NULL);
            json_set(row, "user_response", clean_node ? clean_node : json_string(""));
            free(clean);
            json_append(responses, row);
        }
    }
    json_t *result = json_object();
    json_set(result, "responses", responses);
    if (timed_out) json_set(result, "timed_out", json_bool(true));
    char *out = json_serialize(result);
    json_free(result);
    if (norm) json_free(norm);
    if (answers) json_free(answers);
    return out;
}
/* PoP: clarify_run_batch @ tools/clarify_tool.py:_run_batch */
char *clarify_run_batch(const char *normalized_json, clarify_callback cb,
                        const char *title, bool cb_is_batch, void *user_data) {
    if (clarify_callback_accepts_questions(cb_is_batch)) {
        /* build a JSON array of the normalized entries for the callback */
        json_t *norm = json_parse(normalized_json, NULL);
        char *norm_dup = json_serialize(norm);
        char *raw = cb(title ? title : "", norm_dup, false, user_data);
        free(norm_dup);
        if (norm) json_free(norm);

        json_t *answers = NULL;
        bool timed_out = false;
        if (!raw || (raw[0] && strcmp(raw, CLARIFY_TIMEOUT_RESPONSE) == 0)) {
            timed_out = true;
        } else {
            char *e = NULL;
            json_t *r = json_parse(raw, &e);
            if (e) free(e);
            if (r && r->type == JSON_OBJECT) {
                json_t *an = json_obj_get(r, "answers");
                if (an && an->type == JSON_OBJECT) answers = json_copy(an);
                json_t *to = json_obj_get(r, "timed_out");
                if (to && to->type == JSON_BOOL) timed_out = to->bool_val;
            } else if (r && r->type == JSON_STRING && strcmp(r->str_val, CLARIFY_TIMEOUT_RESPONSE) == 0) {
                timed_out = true;
            }
            if (r) json_free(r);
        }
        char *answers_str = answers ? json_serialize(answers) : xstrdup("{}");
        if (answers) json_free(answers);
        char *out = clarify_batch_result(normalized_json, answers_str, timed_out);
        free(answers_str);
        free(raw);
        return out;
    }

    /* legacy: loop one question at a time */
    json_t *norm = json_parse(normalized_json, NULL);
    json_t *answers = json_object();
    bool timed_out = false;
    if (norm && norm->type == JSON_ARRAY) {
        size_t n = json_len(norm);
        for (size_t i = 0; i < n && !timed_out; i++) {
            json_t *e = json_get(norm, i);
            if (!e || e->type != JSON_OBJECT) continue;
            json_t *qn = json_obj_get(e, "question");
            const char *q = (qn && qn->type == JSON_STRING) ? qn->str_val : "";
            json_t *choices = json_obj_get(e, "choices");
            char *choices_str = (choices && choices->type == JSON_ARRAY) ? json_serialize(choices) : xstrdup("null");
            json_t *ms = json_obj_get(e, "multi_select");
            bool multi = (ms && ms->type == JSON_BOOL) ? ms->bool_val : false;
            char *raw = cb(q ? q : "", choices_str, multi, user_data);
            free(choices_str);
            if (!raw || (raw[0] && strcmp(raw, CLARIFY_TIMEOUT_RESPONSE) == 0)) {
                timed_out = true;
                free(raw);
                break;
            }
            json_t *qidn = json_obj_get(e, "qid");
            const char *qid = (qidn && qidn->type == JSON_STRING) ? qidn->str_val : "";
            json_set(answers, qid, json_string(raw));
            free(raw);
        }
    }
    char *answers_str = json_serialize(answers);
    char *out = clarify_batch_result(normalized_json, answers_str, timed_out);
    free(answers_str);
    json_free(answers);
    if (norm) json_free(norm);
    return out;
}

/* ------------------------------------------------------------------ */
/* top-level clarify_tool                                              */
/* ------------------------------------------------------------------ */

/* PoP: clarify_tool @ tools/clarify_tool.py:clarify_tool */
char *clarify_tool(const char *question, const char *choices_json, bool multi_select,
                   const char *questions_json, clarify_callback cb, bool cb_is_batch,
                   void *user_data) {
    /* batch path */
    if (questions_json) {
        char *normalized = clarify_normalize_questions(questions_json);
        if (normalized && normalized[0] == '{' && normalized[1] == '\"' && normalized[2] == 'e') {
            /* error JSON */
            return normalized;
        }
        if (normalized && strcmp(normalized, "null") == 0) {
            free(normalized);
            /* fall through to single-question path */
        } else {
            if (!cb) {
                char *r = xstrdup("{\"error\": \"Clarify tool is not available in this execution context.\"}");
                free(normalized);
                return r;
            }
            char *title = question ? xstrdup(question) : xstrdup("");
            str_strip_inplace(title);
            char *out;
            out = clarify_run_batch(normalized, cb, title, cb_is_batch, user_data);
            free(title);
            free(normalized);
            return out;
        }
    }

    if (!question || !question[0]) {
        return xstrdup("{\"error\": \"Question text is required.\"}");
    }
    char *q = xstrdup(question);
    str_strip_inplace(q);
    if (!q[0]) {
        free(q);
        return xstrdup("{\"error\": \"Question text is required.\"}");
    }

    /* validate + flatten choices */
    char **choices = NULL;
    size_t ccount = 0;
    bool has_choices = false;
    if (choices_json) {
        char *err = NULL;
        json_t *cn = json_parse(choices_json, &err);
        if (err) free(err);
        if (!cn || cn->type != JSON_ARRAY) {
            free(q);
            if (cn) json_free(cn);
            return xstrdup("{\"error\": \"choices must be a list of strings.\"}");
        }
        size_t len = json_len(cn);
        choices = calloc(len ? len : 1, sizeof(char *));
        for (size_t i = 0; i < len; i++) {
            json_t *c = json_get(cn, i);
            char *cj = (c) ? json_serialize(c) : xstrdup("null");
            char *flat = clarify_flatten_choice(cj);
            free(cj);
            if (flat && flat[0]) choices[ccount++] = flat;
        }
        if (cn) json_free(cn);
        if (ccount > CLARIFY_MAX_CHOICES) ccount = CLARIFY_MAX_CHOICES;
        has_choices = (ccount > 0);
        if (!has_choices) { for (size_t i = 0; i < ccount; i++) free(choices[i]); free(choices); choices = NULL; }
    }

    if (!cb) {
        free(q);
        if (choices) { for (size_t i = 0; i < ccount; i++) free(choices[i]); free(choices); }
        return xstrdup("{\"error\": \"Clarify tool is not available in this execution context.\"}");
    }

    char **offered = choices;
    char **decorated = NULL;
    if (has_choices) {
        decorated = clarify_mark_recommended(choices, ccount);
        offered = decorated;
    }

    char *choices_for_cb = NULL;
    if (has_choices) {
        json_t *arr = json_array();
        for (size_t i = 0; i < ccount; i++) if (offered[i]) json_append(arr, json_string(offered[i]));
        choices_for_cb = json_serialize(arr);
        json_free(arr);
    }

    char *raw_response = cb(q, choices_for_cb ? choices_for_cb : "null", multi_select, user_data);
    free(choices_for_cb);

    json_t *result = json_object();
    json_set(result, "question", json_string(q));
    if (has_choices) {
        json_t *off = json_array();
        for (size_t i = 0; i < ccount; i++) if (choices[i]) json_append(off, json_string(choices[i]));
        json_set(result, "choices_offered", off);
    } else {
        json_set(result, "choices_offered", json_null());
    }

    if (multi_select && has_choices) {
        char **parts = clarify_parse_multi_select_response(raw_response ? raw_response : "");
        json_t *arr = json_array();
        for (size_t i = 0; parts[i]; i++) {
            char *clean = clarify_strip_recommended(parts[i]);
            json_append(arr, json_string(clean));
            free(clean);
            free(parts[i]);
        }
        free(parts);
        json_set(result, "user_response", arr);
    } else {
        char *clean = clarify_strip_recommended(raw_response ? raw_response : "");
        json_set(result, "user_response", json_string(clean));
        free(clean);
    }

    char *out = json_serialize(result);
    json_free(result);
    free(q);
    if (choices) { for (size_t i = 0; i < ccount; i++) free(choices[i]); free(choices); }
    if (decorated) { for (size_t i = 0; i < ccount; i++) free(decorated[i]); free(decorated); }
    free(raw_response);
    return out;
}

/* PoP: check_clarify_requirements @ tools/clarify_tool.py:check_clarify_requirements */
bool clarify_check_requirements(void) {
    return true;
}

/* PoP: _callback_accepts_questions @ tools/clarify_tool.py:_callback_accepts_questions */
/* Python decides this via inspect.signature; in C the caller declares its
 * callback's capability explicitly (the cb_is_batch flag), which is the exact
 * piece of information Python's signature probe extracts. */
bool clarify_callback_accepts_questions(bool cb_is_batch) {
    return cb_is_batch;
}

/* PoP: _invoke_callback @ tools/clarify_tool.py:_invoke_callback */
/* Dispatch to the platform callback. Mirrors the Python signature-probe
 * behavior: batch-capable callbacks (cb_is_batch) receive the whole
 * normalized questions list; legacy callbacks are looped one question at a
 * time (see clarify_run_batch, which owns the looping). */
char *clarify_invoke_callback(const char *question, const char *choices_json,
                              bool multi_select, clarify_callback cb,
                              bool cb_is_batch, const char *normalized_json,
                              void *user_data) {
    if (cb_is_batch) {
        return cb(question, normalized_json ? normalized_json : (choices_json ? choices_json : ""),
                  multi_select, user_data);
    }
    return cb(question, choices_json ? choices_json : "null", multi_select, user_data);
}
