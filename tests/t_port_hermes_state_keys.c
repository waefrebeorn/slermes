/* t_port_hermes_state_keys.c — oracle harness for hermes_state_keys.py and the
 * two hermes_state_runtime.py key builders. Emits one JSON line per case. */
#include "port_hermes_state_keys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *P(const char *s)
{
    return json_parse(s, NULL);
}

static void jstr_escape(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar(ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

static void emit_str(int cs, const char *fn, const char *in, const char *out)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":{\"str\":", cs, fn, in);
    jstr_escape(out ? out : "");
    printf("}}\n");
    fflush(stdout);
}

static void emit_null(int cs, const char *fn, const char *in)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":null}\n", cs, fn, in);
    fflush(stdout);
}

static void emit_str_or_null(int cs, const char *fn, const char *in,
                             const char *out)
{
    if (!out)
        emit_null(cs, fn, in);
    else
        emit_str(cs, fn, in, out);
}

static void emit_json(int cs, const char *fn, const char *in, json_t *out)
{
    char *s = out ? json_serialize(out) : NULL;
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%s}\n", cs, fn, in,
           s ? s : "null");
    fflush(stdout);
    free(s);
    json_free(out);
}

static void emit_obj(int cs, const char *fn, const char *in, const char *a,
                     const char *b)
{
    (void)cs;
    (void)fn;
    (void)in;
    (void)a;
    (void)b;
}

static void emit_run(int cs, const char *req, const char *job)
{
    char *j = NULL, *e = NULL;
    int ok = hsk_cron_run_from_request(req, job, &j, &e);
    printf("{\"case\":%d,\"fn\":\"cron_run_from_request\",\"in\":{\"request_id\":", cs);
    jstr_escape(req);
    printf(",\"job_id\":");
    if (job)
        jstr_escape(job);
    else
        printf("null");
    printf("},\"out\":");
    if (ok) {
        printf("{\"job\":");
        jstr_escape(j);
        printf(",\"exec\":");
        jstr_escape(e);
        printf("}\n");
    } else {
        printf("null}\n");
    }
    fflush(stdout);
    free(j);
    free(e);
}

int main(void)
{
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);

    /* cron_request_id */
    emit_str(1, "cron_request_id",
             "{\"job_id\":\"job1\",\"execution_id\":\"42\"}",
             hsk_cron_request_id("job1", "42"));
    emit_str(2, "cron_request_id",
             "{\"job_id\":\"j\",\"execution_id\":\"\"}",
             hsk_cron_request_id("j", ""));

    /* prefix_range / cron_request_range */
    {
        char *hi = NULL;
        char *lo = hsk_prefix_range("abc", &hi);
        emit_obj(3, "prefix_range", "{\"prefix\":\"abc\"}", "lo", lo);
        printf("{\"case\":3,\"fn\":\"prefix_range\",\"in\":{\"prefix\":\"abc\"},"
               "\"out\":{\"lo\":");
        jstr_escape(lo ? lo : "");
        printf(",\"hi\":");
        jstr_escape(hi ? hi : "");
        printf("}}\n");
        free(lo);
        free(hi);

        lo = hsk_cron_request_range("job1", &hi);
        printf("{\"case\":4,\"fn\":\"cron_request_range\",\"in\":{\"job_id\":\"job1\"},"
               "\"out\":{\"lo\":");
        jstr_escape(lo ? lo : "");
        printf(",\"hi\":");
        jstr_escape(hi ? hi : "");
        printf("}}\n");
        free(lo);
        free(hi);
    }

    /* cron_run_from_request */
    {
        char *job = NULL, *exec = NULL;
        int ok = hsk_cron_run_from_request("cron:job1:42", NULL, &job, &exec);
        printf("{\"case\":5,\"fn\":\"cron_run_from_request\","
               "\"in\":{\"request_id\":\"cron:job1:42\",\"job_id\":null},\"out\":");
        if (ok) {
            printf("{\"job\":");
            jstr_escape(job);
            printf(",\"exec\":");
            jstr_escape(exec);
            printf("}}\n");
        } else {
            printf("null}\n");
        }
        free(job);
        free(exec);

        job = exec = NULL;
        ok = hsk_cron_run_from_request("cron:job1:42", "job1", &job, &exec);
        printf("{\"case\":6,\"fn\":\"cron_run_from_request\","
               "\"in\":{\"request_id\":\"cron:job1:42\",\"job_id\":\"job1\"},\"out\":");
        if (ok) {
            printf("{\"job\":");
            jstr_escape(job);
            printf(",\"exec\":");
            jstr_escape(exec);
            printf("}}\n");
        } else {
            printf("null}\n");
        }
        free(job);
        free(exec);

        ok = hsk_cron_run_from_request("cron:a:b:c", NULL, &job, &exec);
        printf("{\"case\":7,\"fn\":\"cron_run_from_request\","
               "\"in\":{\"request_id\":\"cron:a:b:c\",\"job_id\":null},\"out\":");
        printf(ok ? "{" : "null}\n");
        if (ok) {
            printf("{\"job\":");
            jstr_escape(job);
            printf(",\"exec\":");
            jstr_escape(exec);
            printf("}}\n");
            free(job);
            free(exec);
        }

        ok = hsk_cron_run_from_request("nope", NULL, &job, &exec);
        printf("{\"case\":8,\"fn\":\"cron_run_from_request\","
               "\"in\":{\"request_id\":\"nope\",\"job_id\":null},\"out\":");
        printf(ok ? "{" : "null}\n");
        if (ok) {
            printf("{\"job\":");
            jstr_escape(job);
            printf(",\"exec\":");
            jstr_escape(exec);
            printf("}}\n");
            free(job);
            free(exec);
        }
    }

    /* legacy_cron_run_job */
    emit_str_or_null(9, "legacy_cron_run_job",
                     "{\"session_id\":\"cron_job1_20260101_120000\"}",
                     hsk_legacy_cron_run_job("cron_job1_20260101_120000"));
    emit_str_or_null(10, "legacy_cron_run_job",
                     "{\"session_id\":\"cron_a:b_20260101_120000\"}",
                     hsk_legacy_cron_run_job("cron_a:b_20260101_120000"));
    emit_str_or_null(11, "legacy_cron_run_job",
                     "{\"session_id\":\"other_20260101_120000\"}",
                     hsk_legacy_cron_run_job("other_20260101_120000"));
    emit_str_or_null(12, "legacy_cron_run_job",
                     "{\"session_id\":\"cron_job1_20260101_12000\"}",
                     hsk_legacy_cron_run_job("cron_job1_20260101_12000"));

    /* admission_fingerprint */
    emit_str(13, "admission_fingerprint",
             "{\"target\":\"s1\",\"payload\":{\"principal\":\"p1\",\"request\":\"r1\"}}",
             hsk_admission_fingerprint("s1", P("{\"principal\":\"p1\",\"request\":\"r1\"}")));
    emit_str(14, "admission_fingerprint",
             "{\"target\":\"s1\",\"payload\":{\"b\":1,\"a\":[2,3]}}",
             hsk_admission_fingerprint("s1", P("{\"b\":1,\"a\":[2,3]}")));

    /* local_identity */
    emit_str(15, "local_identity",
             "{\"profile_id\":\"prof\",\"principal_id\":\"prin\",\"request_id\":\"req\"}",
             hsk_local_identity("prof", "prin", "req"));

    /* profile_from_session_key_namespace */
    emit_str(16, "profile_from_session_key_namespace",
             "{\"namespace\":\"main\"}",
             hsk_profile_from_session_key_namespace("main"));
    emit_str(17, "profile_from_session_key_namespace",
             "{\"namespace\":\"main~\"}",
             hsk_profile_from_session_key_namespace("main~"));
    emit_str(18, "profile_from_session_key_namespace",
             "{\"namespace\":\"work\"}",
             hsk_profile_from_session_key_namespace("work"));

    /* profile_from_session_key */
    emit_str_or_null(19, "profile_from_session_key",
                     "{\"session_key\":\"agent:main:abc\"}",
                     hsk_profile_from_session_key("agent:main:abc"));
    emit_str_or_null(20, "profile_from_session_key",
                     "{\"session_key\":\"agent:work:abc\"}",
                     hsk_profile_from_session_key("agent:work:abc"));
    emit_str_or_null(21, "profile_from_session_key",
                     "{\"session_key\":\"agent::abc\"}",
                     hsk_profile_from_session_key("agent::abc"));
    emit_str_or_null(22, "profile_from_session_key",
                     "{\"session_key\":\"other:main:abc\"}",
                     hsk_profile_from_session_key("other:main:abc"));
    emit_str_or_null(23, "profile_from_session_key",
                     "{\"session_key\":null}",
                     hsk_profile_from_session_key(NULL));

    /* replacement_binding */
    emit_str(24, "replacement_binding",
             "{\"principal_id\":\"p1\",\"session_id\":\"s1\",\"request_id\":\"r1\"}",
             hsr_replacement_binding("p1", "s1", "r1"));

    /* mutation_receipt_key */
    {
        char *key = NULL, *digest = NULL;
        int ok = hsr_mutation_receipt_key(
            "p1", "s1", "r1", "set_model", P("{\"a\":1}"), P("7"), NULL,
            &key, &digest);
        printf("{\"case\":25,\"fn\":\"mutation_receipt_key\",\"in\":{"
               "\"principal_id\":\"p1\",\"session_id\":\"s1\",\"request_id\":\"r1\","
               "\"operation\":\"set_model\",\"payload\":{\"a\":1},"
               "\"expected_revision\":7,\"expected_generation\":null},\"out\":");
        if (ok) {
            printf("{\"key\":");
            jstr_escape(key);
            printf(",\"digest\":");
            jstr_escape(digest);
            printf("}}\n");
        } else {
            printf("null}\n");
        }
        free(key);
        free(digest);

        key = digest = NULL;
        ok = hsr_mutation_receipt_key(
            "p1", "s1", "r1", "set_model", P("{\"a\":1}"), P("7"), P("3"),
            &key, &digest);
        printf("{\"case\":26,\"fn\":\"mutation_receipt_key\",\"in\":{"
               "\"principal_id\":\"p1\",\"session_id\":\"s1\",\"request_id\":\"r1\","
               "\"operation\":\"set_model\",\"payload\":{\"a\":1},"
               "\"expected_revision\":7,\"expected_generation\":3},\"out\":");
        if (ok) {
            printf("{\"key\":");
            jstr_escape(key);
            printf(",\"digest\":");
            jstr_escape(digest);
            printf("}}\n");
        } else {
            printf("null}\n");
        }
        free(key);
        free(digest);
    }
    /* --- extended edge cases (None/empty, re newline semantics, the two
     * json.dumps ensure_ascii modes) --- */

    /* legacy_cron_run_job: '$' tolerates ONE trailing newline, '.' never
     * crosses an internal one. */
    emit_str_or_null(27, "legacy_cron_run_job",
                     "{\"session_id\":\"cron_j_20260101_120000\\n\"}",
                     hsk_legacy_cron_run_job("cron_j_20260101_120000\n"));
    emit_str_or_null(28, "legacy_cron_run_job",
                     "{\"session_id\":\"cron_j\\nx_20260101_120000\"}",
                     hsk_legacy_cron_run_job("cron_j\nx_20260101_120000"));
    emit_str_or_null(29, "legacy_cron_run_job",
                     "{\"session_id\":\"cron_j_20260101_120000\\n\\n\"}",
                     hsk_legacy_cron_run_job("cron_j_20260101_120000\n\n"));
    emit_str_or_null(30, "legacy_cron_run_job",
                     "{\"session_id\":\"\"}",
                     hsk_legacy_cron_run_job(""));

    /* local_identity: ensure_ascii=True (é, DEL \x7f, astral 😀
     * all escape to \uXXXX / surrogate pairs before hashing). */
    emit_str(31, "local_identity",
             "{\"profile_id\":\"café\",\"principal_id\":\"p\",\"request_id\":\"r\"}",
             hsk_local_identity("café", "p", "r"));
    emit_str(32, "local_identity",
             "{\"profile_id\":\"p\",\"principal_id\":\"q\",\"request_id\":\"\\u007f\"}",
             hsk_local_identity("p", "q", "\x7f"));
    emit_str(33, "local_identity",
             "{\"profile_id\":\"😀\",\"principal_id\":\"p\",\"request_id\":\"r\"}",
             hsk_local_identity("😀", "p", "r"));

    /* admission_fingerprint: ensure_ascii=False (é stays raw UTF-8). */
    emit_str(34, "admission_fingerprint",
             "{\"target\":\"s\",\"payload\":{\"k\":\"é\"}}",
             hsk_admission_fingerprint("s", P("{\"k\":\"é\"}")));

    /* cron_run_from_request: empty job / empty exec / prefix mismatch. */
    emit_run(35, "cron:x:1", "");
    emit_run(36, "cron:j:", NULL);
    emit_run(37, "cron:other:1", "j");

    /* profile_from_session_key: no colon at all / empty namespace slot. */
    emit_str_or_null(38, "profile_from_session_key",
                     "{\"session_key\":\"agent\"}",
                     hsk_profile_from_session_key("agent"));
    emit_str_or_null(39, "profile_from_session_key",
                     "{\"session_key\":\"agent:\"}",
                     hsk_profile_from_session_key("agent:"));

    /* prefix_range: last-char bump at the ASCII edge ('z' -> '{'). */
    {
        char *hi = NULL;
        char *lo = hsk_prefix_range("z", &hi);
        printf("{\"case\":40,\"fn\":\"prefix_range\",\"in\":{\"prefix\":\"z\"},"
               "\"out\":{\"lo\":");
        jstr_escape(lo ? lo : "");
        printf(",\"hi\":");
        jstr_escape(hi ? hi : "");
        printf("}}\n");
        fflush(stdout);
        free(lo);
        free(hi);
    }

    (void)emit_json;
    return 0;
}
