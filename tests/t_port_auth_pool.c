/* t_port_port_auth_pool.c -- harness for the auth.py credential-pool port. */
#include "port_auth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else putchar(*p);
        }
    }
    putchar('"');
}

static void jlist(char **w, int n)
{
    putchar('[');
    for (int i = 0; i < n; i++) {
        if (i) putchar(',');
        jstr(w[i]);
    }
    putchar(']');
}

static const char *const ROWS[] = {
    "null", "\"str\"", "{}", "{\"id\":\"a\"}",
    "{\"id\":\"a\",\"access_token\":\"AT\",\"refresh_token\":\"RT\"}",
    "{\"id\":\"b\",\"access_token\":\"AT2\"}",
    "{\"id\":\"\",\"access_token\":\"X\"}",
    "{\"id\":\"c\",\"access_token\":null}",
    "{\"id\":\"d\",\"access_token\":123,\"refresh_token\":true}",
};

static const char *const POOLS[] = {
    "[]", "{}",
    "[{\"id\":\"a\",\"access_token\":\"AT\",\"refresh_token\":\"RT\"}]",
    "[{\"id\":\"a\",\"access_token\":\"AT\"},{\"id\":\"b\",\"access_token\":\"BT\"}]",
    "[{\"id\":\"a\"},{\"id\":\"\"},{\"id\":\"c\",\"access_token\":\"CT\"}]",
    "[null,\"str\",{\"id\":\"a\"}]",
};

static const char *const SUPPRESSED[] = {
    "{}", "{\"openrouter\":[\"env\",\"pool\"]}",
    "{\"openrouter\":{\"env\":1,\"pool\":1}}",
    "{\"openrouter\":\"env\"}", "{\"other\":[\"x\"]}",
    "{\"openrouter\":[]}", "{\"openrouter\":[1,null,\"s\"]}",
};

static const char *const ENTRIES[] = {
    "{}", "{\"id\":\"a\"}",
    "{\"id\":\"a\",\"access_token\":\"OLD\",\"last_status\":\"dead\"}",
    "{\"id\":\"a\",\"access_token\":\"OLD\",\"last_status\":\"ok\",\"scope\":\"s\"}",
    "{\"id\":\"a\",\"access_token\":\"OLD\",\"last_status\":\"dead\",\"failure_reason\":\"r\",\"extra\":1}",
};
static const char *const DISKS[] = {
    "null", "{}", "{\"access_token\":\"NEW\"}",
    "{\"access_token\":\"NEW\",\"refresh_token\":\"NR\",\"expires_at\":1}",
    "{\"access_token\":\"NEW\",\"last_status\":\"exhausted\",\"last_status_at\":2,\"failure_reason\":\"dr\"}",
    "{\"last_status\":\"dead\",\"last_status_at\":2}",
};

int main(void)
{
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);

    for (size_t i = 0; i < sizeof(ROWS) / sizeof(ROWS[0]); i++) {
        json_t *row = json_parse(ROWS[i], NULL);
        char at[512] = "x", rt[512] = "x";
        int isobj = st_credential_token_pair(row, at, sizeof(at), rt, sizeof(rt));
        printf("{\"fn\":\"pair\",\"arg\":");
        jstr(ROWS[i]);
        printf(",\"isobj\":%d,\"access\":", isobj);
        jstr(at);
        printf(",\"refresh\":");
        jstr(rt);
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(row);
    }

    for (size_t i = 0; i < sizeof(POOLS) / sizeof(POOLS[0]); i++) {
        json_t *pool = json_parse(POOLS[i], NULL);
        char *ids[64];
        json_t *rows[64];
        int n = st_entry_ids(pool, rows, ids, 64);
        printf("{\"fn\":\"entryids\",\"arg\":");
        jstr(POOLS[i]);
        printf(",\"out\":");
        jlist(ids, n);
        fputs("}\n", stdout);
        fflush(stdout);
        for (int k = 0; k < n; k++)
            free(ids[k]);

        char *acc[64], *ref[64];
        memset(acc, 0, sizeof(acc));
        memset(ref, 0, sizeof(ref));
        int m = st_token_pairs_by_id(pool, ids, acc, ref, 64);
        printf("{\"fn\":\"tokenpairs\",\"arg\":");
        jstr(POOLS[i]);
        printf(",\"out\":[");
        for (int k = 0; k < m; k++) {
            if (k) putchar(',');
            fputs("[", stdout);
            jstr(ids[k]);
            putchar(',');
            jstr(acc[k] ? acc[k] : "");
            putchar(',');
            jstr(ref[k] ? ref[k] : "");
            fputs("]", stdout);
        }
        fputs("]}\n", stdout);
        for (int k = 0; k < m; k++) {
            free(ids[k]);
            free(acc[k]);
            free(ref[k]);
        }
        json_free(pool);
    }

    for (size_t i = 0; i < sizeof(SUPPRESSED) / sizeof(SUPPRESSED[0]); i++) {
        const char *provs[] = {"openrouter", "other", "missing"};
        for (size_t p = 0; p < 3; p++) {
            json_t *sup = json_parse(SUPPRESSED[i], NULL);
            char *out[64];
            int n = st_suppressed_source_list(sup, provs[p], out, 64);
            printf("{\"fn\":\"suppressed\",\"arg\":");
            jstr(SUPPRESSED[i]);
            printf(",\"prov\":");
            jstr(provs[p]);
            printf(",\"out\":");
            jlist(out, n);
            printf(",\"after\":");
            char *ser = sup ? json_serialize(sup) : NULL;
            jstr(ser ? ser : "");
            free(ser);
            fputs("}\n", stdout);
        fflush(stdout);
            for (int k = 0; k < n; k++)
                free(out[k]);
            json_free(sup);
        }
    }

    for (size_t e = 0; e < sizeof(ENTRIES) / sizeof(ENTRIES[0]); e++) {
        for (size_t d = 0; d < sizeof(DISKS) / sizeof(DISKS[0]); d++) {
            const char *bas[] = {"__none__", "OLD", "NEW"};
            int cleared[] = {0, 1};
            for (size_t b = 0; b < 3; b++) {
                for (size_t c = 0; c < 2; c++) {
                    json_t *entry = json_parse(ENTRIES[e], NULL);
                    json_t *disk = json_parse(DISKS[d], NULL);
                    char out[8192];
                    st_merge_pool_row_generation(
                        entry, disk, "openrouter",
                        strcmp(bas[b], "__none__") ? bas[b] : NULL, NULL,
                        cleared[c], NULL, out, sizeof(out));
                    printf("{\"fn\":\"merge\",\"entry\":");
                    jstr(ENTRIES[e]);
                    printf(",\"disk\":");
                    jstr(DISKS[d]);
                    printf(",\"ba\":");
                    jstr(bas[b]);
                    printf(",\"cleared\":%d,\"out\":", cleared[c]);
                    jstr(out);
                    fputs("}\n", stdout);
        fflush(stdout);
                    json_free(entry);
                    json_free(disk);
                }
            }
        }
    }
    return 0;
}
