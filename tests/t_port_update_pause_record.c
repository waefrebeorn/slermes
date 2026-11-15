/* t_port_update_pause_record.c — oracle harness for the pure slice of
 * hermes_cli/update_pause_record.py. Emits one JSON line per case. */
#include "port_update_pause_record.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_ve = 1, g_vo = 1;

/* Stand-ins for update_lock.incarnation_live: live / not-live / unknown. */
static int live_fn(long long pid, const char *ct)   { (void)pid; (void)ct; return 1; }
static int dead_fn(long long pid, const char *ct)   { (void)pid; (void)ct; return 0; }
static int unknown_fn(long long pid, const char *ct){ (void)pid; (void)ct; return -1; }

/* Even pid -> g_ve, odd -> g_vo (for _live_pids). */
static int fake_live(long long pid, const char *ct)
{
    (void)ct;
    return (pid % 2 == 0) ? g_ve : g_vo;
}

static int cs = 0;

static void jstr(const char *s)
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

static void emit_str(const char *fn, const char *in, const char *out)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":{\"str\":",
           ++cs, fn, in);
    if (!out) fputs("null", stdout); else jstr(out);
    printf("}}\n");
    fflush(stdout);
}

static void emit_json(const char *fn, const char *in, json_t *out)
{
    char *s = out ? json_serialize(out) : NULL;
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%s}\n",
           ++cs, fn, in, s ? s : "null");
    fflush(stdout);
    free(s);
    json_free(out);
}

static void emit_int(const char *fn, const char *in, long v)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%ld}\n",
           ++cs, fn, in, v);
    fflush(stdout);
}

static void digest_case(const char *name, int missing, const char *bytes)
{
    char path[512];
    snprintf(path, sizeof(path), "C:/Users/eman5/AppData/Local/hermes/cache/scratch/upr_%s", name);
    if (!missing) {
        FILE *fh = fopen(path, "wb");
        if (fh) { fputs(bytes, fh); fclose(fh); }
    }
    /* jstr writes to stdout, so the whole record is emitted directly */
    printf("{\"case\":%d,\"fn\":\"digest_file\",\"in\":{\"name\":\"%s\",\"missing\":%s,\"bytes\":",
           ++cs, name, missing ? "true" : "false");
    jstr(bytes);
    printf("},\"out\":{\"str\":");
    char *d = upr_digest_file(path);
    if (!d) fputs("null", stdout); else jstr(d);
    printf("}}\n");
    fflush(stdout);
    free(d);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

#ifndef UPR_SKIP_HASH
    /* _install_key */
    {
        const char *cases[] = {
            "C:/Users/eman5/hermes-agent",
            "C:\\Users\\eman5\\hermes-agent",
            "C:/USERS/EMAN5/HERMES-AGENT",
            "/home/wubu/hermes-agent-dev/slermes",
            "", NULL,
        };
        for (int k = 0; cases[k]; k++) {
            char key[32];
            upr_install_key(cases[k], key, sizeof(key));
            printf("{\"case\":%d,\"fn\":\"install_key\",\"in\":{\"root\":", ++cs);
            jstr(cases[k]);
            printf("},\"out\":{\"str\":");
            jstr(key);
            printf("}}\n");
        }
        char key[32];
        upr_install_key(NULL, key, sizeof(key));
        printf("{\"case\":%d,\"fn\":\"install_key\",\"in\":{\"root\":null},\"out\":{\"str\":",
               ++cs);
        jstr(key);
        printf("}}\n");
        fflush(stdout);
    }
#endif /* UPR_SKIP_HASH */

    /* _digest */
#ifndef UPR_SKIP_HASH
    digest_case("a", 0, "hello");
    digest_case("b", 0, "");
    digest_case("c", 0, "the quick brown fox");
    digest_case("missing", 1, "whatever");
#endif

    /* _has_work */
    {
        const char *cases[] = {
            "{}",
            "{\"profiles\":{}}",
            "{\"profiles\":{\"p\":\"1\"}}",
            "{\"unmapped\":[]}",
            "{\"unmapped\":[{\"argv\":[]}]}",
            "{\"unmapped\":[{\"argv\":[\"hermes\"]}]}",
            "{\"services\":[]}",
            "{\"services\":{\"s\":{}}}",
            "{\"posix_units\":[]}",
            "{\"posix_units\":[{\"pid\":1}]}",
            "{\"cold_start_if_installed\":false}",
            "{\"cold_start_if_installed\":true}",
            "{\"cold_start_profiles\":[]}",
            "{\"cold_start_profiles\":[\"a\"]}",
            NULL,
        };
        for (int k = 0; cases[k]; k++) {
            json_t *t = json_parse(cases[k], NULL);
            char in[256];
            snprintf(in, sizeof(in), "{\"token\":%s}", cases[k]);
            emit_int("has_work", in, upr_has_work(t));
            json_free(t);
        }
    }

    /* _without */
    {
        const char *tokens[] = {
            "{}",
            "{\"profiles\":{\"a\":\"1\",\"b\":\"2\"}}",
            "{\"unmapped\":[{\"pid\":1},{\"pid\":2},{\"pid\":\"3\"}]}",
            "{\"profiles\":{\"a\":1,\"b\":2},\"unmapped\":[{\"pid\":1,\"argv\":[\"x\"]}]}"
            ",\"posix_units\":[{\"pid\":2},{\"pid\":3}]}",
            "{\"posix_units\":[{\"pid\":1}]}",
            "{\"profiles\":{\"a\":\"1\"},\"other\":5}",
            NULL,
        };
        const char *pidsets[] = {
            "[]", "[1]", "[\"1\"]", "[1,2]", "[2,3]", "[1,2,3]", "[\"3\"]", NULL,
        };
        for (int k = 0; tokens[k]; k++) {
            for (int j = 0; pidsets[j]; j++) {
                json_t *t = json_parse(tokens[k], NULL);
                json_t *p = json_parse(pidsets[j], NULL);
                const char *const *pids; size_t n;
                const char *arr[16];
                char bufs[16][32];
                size_t m = 0;
                if (p && p->type == JSON_ARRAY) {
                    for (size_t q = 0; q < p->c.count && q < 16; q++) {
                        json_t *v = p->c.items[q];
                        if (v && v->type == JSON_STRING && v->str_val) {
                            arr[m++] = v->str_val;
                        } else if (v && v->type == JSON_NUMBER) {
                            /* NB: `arr[m++] = bufs[m - 1]` is unspecified
                             * evaluation order and reads bufs[-1] when m==0. */
                            size_t idx = m++;
                            snprintf(bufs[idx], sizeof(bufs[idx]), "%lld",
                                     (long long)v->num_val);
                            arr[idx] = bufs[idx];
                        }
                    }
                }
                pids = arr; n = m;
                upr_without(t, pids, n);
                char in[600];
                snprintf(in, sizeof(in), "{\"token\":%s,\"pids\":%s}",
                         tokens[k], pidsets[j]);
                emit_json("without", in, t);
                json_free(p);
            }
        }
    }

    /* _accepted_path */
    {
        const char *cases[] = {
            "C:/h/.hermes-update-paused-gateways.abc.json",
            "/x/y/marker",
            "marker",
        };
        for (int k = 0; k < 3; k++) {
            char out[1024];
            upr_accepted_path(cases[k], out, sizeof(out));
            /* emit directly (jstr writes to stdout) */
            printf("{\"case\":%d,\"fn\":\"accepted_path\",\"in\":{\"path\":", ++cs);
            jstr(cases[k]);
            printf("},\"out\":{\"str\":");
            jstr(out);
            printf("}}\n");
        }
        fflush(stdout);
    }

    /* _retired_path */
    {
        const char *cases[] = {
            "C:/h/.hermes-update-paused-gateways.abc.json",
            "/x/y/record.json",
            "/x/y/record",
            "/x/y/.hidden",
            "/x/y/.hidden.json",
            "/x/y/a.b.c",
        };
        for (int k = 0; k < 6; k++) {
            char out[1024];
            upr_retired_path(cases[k], out, sizeof(out));
            printf("{\"case\":%d,\"fn\":\"retired_path\",\"in\":{\"path\":", ++cs);
            jstr(cases[k]);
            printf("},\"out\":{\"str\":");
            jstr(out);
            printf("}}\n");
        }
        fflush(stdout);
    }

    /* identity_is_live */
    {
        const char *idents[] = {
            "{}", "{\"pid\":1,\"ct\":\"ct:1.000\"}", "{\"pid\":0,\"ct\":null}",
            "{\"pid\":\"12\"}", "null",
        };
        for (int k = 0; k < 5; k++) {
            for (int v = 0; v < 3; v++) {
                json_t *id = json_parse(idents[k], NULL);
                upr_incarnation_fn fn = (v == 0) ? live_fn
                                        : (v == 1) ? dead_fn : unknown_fn;
                char in[300];
                snprintf(in, sizeof(in), "{\"ident\":%s,\"verdict\":%s}", idents[k],
                         v == 0 ? "true" : v == 1 ? "false" : "null");
                emit_int("identity_is_live", in, upr_identity_is_live(id, fn));
                json_free(id);
            }
        }
        fflush(stdout);
    }

    /* _live_pids */
    {
        const char *tokens[] = {
            "{}",
            "{\"identities\":{}}",
            "{\"identities\":{\"1\":\"ct:1.0\",\"2\":\"ct:2.0\"}}",
            "{\"identities\":{\"1\":\"ct:1.0\",\"2\":\"ct:2.0\",\"3\":\"ct:3.0\"}}",
        };
        const int pairs[][2] = {{1, 1}, {0, 0}, {1, 0}, {0, 1}, {-1, 1}, {1, -1}};
        for (int k = 0; k < 4; k++) {
            for (int v = 0; v < 6; v++) {
                g_ve = pairs[v][0];
                g_vo = pairs[v][1];
                json_t *t = json_parse(tokens[k], NULL);
                json_t *out = upr_live_pids(t, fake_live);
                char in[400];
                snprintf(in, sizeof(in),
                         "{\"token\":%s,\"v_even\":%s,\"v_odd\":%s}", tokens[k],
                         pairs[v][0] == 1 ? "true" : pairs[v][0] == 0 ? "false" : "null",
                         pairs[v][1] == 1 ? "true" : pairs[v][1] == 0 ? "false" : "null");
                emit_json("live_pids", in, out);
                json_free(t);
            }
        }
        g_ve = g_vo = 1;
    }

    /* _holder */
    {
        const char *cases[] = {
            "C:/h/rec.json",
            "C:/h/rec.json.3.claim",
            "C:/h/rec",
        };
        const char *bodies[] = {
            "{\"owner\":{\"pid\":1}}",
            "{\"claimer\":{\"pid\":2}}",
            "{\"owner\":{\"pid\":1},\"claimer\":{\"pid\":2}}",
            "{}",
            NULL,
        };
        for (int k = 0; k < 5; k++) {
            json_t *body = json_parse(bodies[k], NULL);
            for (int s = 0; s < 3; s++) {
                json_t *held = upr_holder(cases[s], body);
                printf("{\"case\":%d,\"fn\":\"holder\",\"in\":{\"src\":", ++cs);
                jstr(cases[s]);
                printf(",\"body\":%s},\"out\":", bodies[k]);
                if (held) {
                    char *hs = json_serialize(held);
                    fputs(hs ? hs : "null", stdout);
                    free(hs);
                } else {
                    fputs("null", stdout);
                }
                printf("}\n");
            }
            json_free(body);
        }
        fflush(stdout);
    }

    /* split_draining */
    {
        const char *tokens[] = {
            "{\"profiles\":{\"a\":\"1\",\"b\":\"2\"},\"unmapped\":[{\"pid\":1},{\"pid\":2}],"
            "\"posix_units\":[{\"pid\":1}]}",
            "{}",
            NULL,
        };
        const int pairs[][2] = {{1, 1}, {0, 0}, {1, 0}, {0, 1}, {-1, 1}, {1, -1}};
        for (int k = 0; k < 3; k++) {
            for (int v = 0; v < 6; v++) {
                g_ve = pairs[v][0];
                g_vo = pairs[v][1];
                json_t *t = json_parse(tokens[k], NULL);
                json_t *d = upr_split_draining(t, fake_live);
                printf("{\"case\":%d,\"fn\":\"split_draining\",\"in\":{\"token\":%s,"
                       "\"v_even\":%s,\"v_odd\":%s},\"out\":",
                       ++cs, tokens[k],
                       pairs[v][0] == 1 ? "true" : pairs[v][0] == 0 ? "false" : "null",
                       pairs[v][1] == 1 ? "true" : pairs[v][1] == 0 ? "false" : "null");
                printf("{\"draining\":");
                char *s1 = d ? json_serialize(d) : NULL;
                fputs(s1 ? s1 : "null", stdout);
                free(s1);
                json_free(d);
                printf(",\"token\":");
                char *s2 = t ? json_serialize(t) : NULL;
                fputs(s2 ? s2 : "null", stdout);
                free(s2);
                printf("}\n");
                json_free(t);
            }
        }
        g_ve = g_vo = 1;
        fflush(stdout);
    }

    /* merge_into */
    {
        const char *pairs[] = {
            "{\"a\":1}",
            "{\"a\":1}{\"b\":2}",
            "{\"profiles\":{\"p\":\"1\"}}{\"profiles\":{\"q\":\"2\"}}",
            "{\"profiles\":{\"p\":\"1\"}}{\"profiles\":{\"p\":\"9\"}}",
            "{\"baselines\":[{\"pause_id\":\"x\"}]}{\"baselines\":[{\"pause_id\":\"y\"},{\"pause_id\":\"x\"}]}",
            "{\"absorbed\":[\"a\"]}{\"pause_id\":\"b\",\"absorbed\":[\"c\",\"a\"]}",
            "{\"identities\":{\"1\":\"ct:1\"}}{\"identities\":{\"2\":\"ct:2\"}}",
            "{\"stop_requested\":[\"1\"],\"stop_sent\":[\"1\"]}{\"stop_requested\":[\"2\"]}",
            "{\"stop_requested\":[\"1\"]}{\"identities\":{\"5\":\"ct:5\"}}",
            "{\"cold_start_profiles\":{\"a\":\"1\"}}{\"cold_start_profiles\":{\"b\":\"2\"},\"cold_start_if_installed\":true,\"attested_generation\":7}",
            "{\"platform\":\"posix\"}{\"platform\":\"posix\",\"posix_units\":[{\"pid\":1}]}",
            "{}{\"services\":[\"s1\"],\"service_profiles\":{\"s1\":\"p\"}}",
            "{}{\"unmapped\":[{\"pid\":1},{\"pid\":2}]}",
            NULL,
        };
        for (int k = 0; pairs[k]; k++) {
            /* The pair is two JSON objects back to back; the boundary is "}{",
             * so nested objects inside either side do not split early. */
            char *sep = strstr(pairs[k], "}{");
            if (!sep) continue;
            char tokbuf[512], adobuf[512];
            size_t n = (size_t)(sep + 1 - pairs[k]);   /* include the first '}' */
            if (n >= sizeof(tokbuf)) continue;
            memcpy(tokbuf, pairs[k], n);
            tokbuf[n] = '\0';
            snprintf(adobuf, sizeof(adobuf), "%s", sep + 1);
            json_t *t = json_parse(tokbuf, NULL);
            json_t *a = json_parse(adobuf, NULL);
            upr_merge_into(t, a);
            printf("{\"case\":%d,\"fn\":\"merge_into\",\"in\":{\"token\":%s,"
                   "\"adopted\":%s},\"out\":",
                   ++cs, tokbuf, adobuf);
            char *s = t ? json_serialize(t) : NULL;
            fputs(s ? s : "null", stdout);
            free(s);
            printf("}\n");
            json_free(t);
            json_free(a);
        }
        fflush(stdout);
    }

    /* drop_never_stopped */
    {
        const char *tokens[] = {
            "{}",
            "{\"stop_requested\":[\"1\",\"2\"],\"profiles\":{\"a\":\"1\",\"b\":\"2\"},"
            "\"unmapped\":[{\"pid\":1},{\"pid\":2}],\"identities\":{\"1\":\"ct:1\",\"2\":\"ct:2\"}}",
            NULL,
        };
        const char *askedsets[] = {"[]", "[\"1\"]", "[\"1\",\"2\"]", "[\"2\",\"3\"]", NULL};
        const char *unksets[] = {"[]", "[\"2\"]", NULL};
        const int pairs[][2] = {{1, 1}, {0, 0}, {1, 0}, {-1, 1}, {1, -1}};
        for (int k = 0; k < 3; k++) {
            for (int a = 0; askedsets[a]; a++) {
                for (int u = 0; unksets[u]; u++) {
                    for (int v = 0; v < 5; v++) {
                        g_ve = pairs[v][0];
                        g_vo = pairs[v][1];
                        json_t *t = json_parse(tokens[k], NULL);
                        json_t *pa = json_parse(askedsets[a], NULL);
                        json_t *pu = json_parse(unksets[u], NULL);
                        const char *aa[8], *au[8];
                        char ba[8][16], bu[8][16];
                        size_t na = 0, nu = 0;
                        if (pa && pa->type == JSON_ARRAY) {
                            for (size_t q = 0; q < pa->c.count && q < 8; q++) {
                                if (pa->c.items[q]->type == JSON_STRING)
                                    aa[na++] = pa->c.items[q]->str_val;
                                else {
                                    snprintf(ba[na], sizeof(ba[na]), "%lld",
                                             (long long)pa->c.items[q]->num_val);
                                    aa[na] = ba[na]; na++;
                                }
                            }
                        }
                        if (pu && pu->type == JSON_ARRAY) {
                            for (size_t q = 0; q < pu->c.count && q < 8; q++) {
                                if (pu->c.items[q]->type == JSON_STRING)
                                    au[nu++] = pu->c.items[q]->str_val;
                                else {
                                    snprintf(bu[nu], sizeof(bu[nu]), "%lld",
                                             (long long)pu->c.items[q]->num_val);
                                    au[nu] = bu[nu]; nu++;
                                }
                            }
                        }
                        upr_drop_never_stopped(t, fake_live, aa, na, au, nu);
                        printf("{\"case\":%d,\"fn\":\"drop_never_stopped\",\"in\":{"
                               "\"token\":%s,\"asked\":%s,\"unknown\":%s,"
                               "\"v_even\":%s,\"v_odd\":%s},\"out\":",
                               ++cs, tokens[k], askedsets[a], unksets[u],
                               pairs[v][0] == 1 ? "true" : pairs[v][0] == 0 ? "false" : "null",
                               pairs[v][1] == 1 ? "true" : pairs[v][1] == 0 ? "false" : "null");
                        char *s = t ? json_serialize(t) : NULL;
                        fputs(s ? s : "null", stdout);
                        free(s);
                        printf("}\n");
                        json_free(t);
                        json_free(pa);
                        json_free(pu);
                    }
                }
            }
        }
        g_ve = g_vo = 1;
        fflush(stdout);
    }

    return 0;
}
