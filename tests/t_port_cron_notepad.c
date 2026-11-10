/*
 * t_port_cron_notepad.c — Oracle harness for cron/notepad.py
 * (ported to src/cli/port_cron_notepad.c).
 */
#include "port_cron_notepad.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_str(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

static const char *DB;
static int failures;

static void set_case(const char *name, const char *job, const char *key,
                     const char *value)
{
    char *err = NULL;
    json_t *rec = cn_set_note(DB, job, key, value, &err);
    printf("{\"family\":\"set\",\"case\":\"%s\",\"ok\":%d,\"err\":", name, rec != NULL);
    emit_json_str(rec ? NULL : err);
    printf(",\"key\":");
    emit_json_str(rec ? json_get_str(rec, "key", "") : NULL);
    printf(",\"value\":");
    emit_json_str(rec ? json_get_str(rec, "value", "") : NULL);
    printf("}\n");
    if (rec) json_free(rec);
    free(err);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    char root[] = ".testbin/cn_XXXXXX";
    if (!mkdtemp(root)) return 1;
    char db[512];
    snprintf(db, sizeof(db), "%s/notepad.db", root);
    DB = db;

    set_case("first", "job1", "cursor", "5");
    {
        char *v = cn_get_note(DB, "job1", "cursor");
        printf("{\"family\":\"get\",\"case\":\"present\",\"value\":");
        emit_json_str(v);
        printf("}\n");
        free(v);
        v = cn_get_note(DB, "job1", "nope");
        printf("{\"family\":\"get\",\"case\":\"absent\",\"value\":");
        emit_json_str(v ? v : NULL);
        printf("}\n");
        free(v);
    }
    set_case("update", "job1", "cursor", "9");
    set_case("second_key", "job1", "seen", "alpha");
    set_case("other_job", "job2", "cursor", "1");

    {
        json_t *notes = cn_list_notes(DB, "job1");
        char *s = json_serialize(notes);
        printf("{\"family\":\"list\",\"case\":\"job1\",\"notes\":%s}\n", s ? s : "null");
        free(s);
        if (notes) json_free(notes);
    }

    printf("{\"family\":\"delete\",\"case\":\"present\",\"ret\":%d}\n",
           cn_delete_note(DB, "job1", "seen"));
    printf("{\"family\":\"delete\",\"case\":\"absent\",\"ret\":%d}\n",
           cn_delete_note(DB, "job1", "seen"));

    {
        char *text = cn_render_notepad_section(DB, "job1");
        printf("{\"family\":\"render\",\"case\":\"job1\",\"text\":");
        emit_json_str(text);
        printf("}\n");
        free(text);
        text = cn_render_notepad_section(DB, "empty-job");
        printf("{\"family\":\"render\",\"case\":\"empty\",\"text\":");
        emit_json_str(text);
        printf("}\n");
        free(text);
    }

    printf("{\"family\":\"clear\",\"case\":\"job1\",\"ret\":%d}\n",
           cn_clear_notepad(DB, "job1"));
    printf("{\"family\":\"clear\",\"case\":\"job1_again\",\"ret\":%d}\n",
           cn_clear_notepad(DB, "job1"));

    /* caps */
    {
        char bigkey[200];
        memset(bigkey, 'k', sizeof(bigkey) - 1);
        bigkey[sizeof(bigkey) - 1] = '\0';
        set_case("key_too_long", "job1", bigkey, "v");
    }
    {
        char *big = (char *)malloc(CN_MAX_VALUE_BYTES + 100);
        memset(big, 'v', CN_MAX_VALUE_BYTES + 99);
        big[CN_MAX_VALUE_BYTES + 99] = '\0';
        set_case("value_too_large", "job1", "k", big);
        free(big);
    }
    {
        /* five 13 KB values exceed the 64 KB job budget on the sixth write */
        char *big = (char *)malloc(13 * 1024);
        memset(big, 'x', 13 * 1024 - 1);
        big[13 * 1024 - 1] = '\0';
        char name[32];
        for (int i = 0; i < 5; i++) {
            snprintf(name, sizeof(name), "fill_%d", i);
            cn_set_note(DB, "capjob", name, big, NULL);
        }
        set_case("job_total_overflow", "capjob", "sixth", big);
        free(big);
        cn_clear_notepad(DB, "capjob");
    }
    set_case("empty_job_id", "", "k", "v");
    set_case("empty_key", "job1", "", "v");
    return 0;
}
