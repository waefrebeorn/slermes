/* t_port_left_core_migration.c -- oracle harness for the pure slice of
 * hermes_cli/left_core_migration.py. Emits one JSON line per case. */
#include "port_left_core_migration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void emit_int(const char *fn, const char *in, long v)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%ld}\n", ++cs, fn, in, v);
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

static void listed_case(const char *cfg, const char *names_json, const char *own)
{
    json_t *c = json_parse(cfg, NULL);
    json_t *ns = json_parse(names_json, NULL);
    const char *arr[16];
    char bufs[16][32];
    size_t n = 0;
    if (ns && ns->type == JSON_ARRAY) {
        for (size_t q = 0; q < ns->c.count && q < 16; q++) {
            json_t *v = ns->c.items[q];
            if (v && v->type == JSON_STRING && v->str_val) {
                arr[n++] = v->str_val;
            } else if (v && v->type == JSON_NUMBER) {
                size_t idx = n++;
                snprintf(bufs[idx], sizeof(bufs[idx]), "%.17g", v->num_val);
                arr[idx] = bufs[idx];
            }
        }
    }
    int r = lcm_toolset_listed(c, arr, n, own);
    printf("{\"case\":%d,\"fn\":\"toolset_listed\",\"in\":{\"config\":%s,\"names\":%s,"
           "\"own_platform\":", ++cs, cfg, names_json);
    jstr(own);
    printf("},\"out\":%d}\n", r);
    fflush(stdout);
    json_free(c);
    json_free(ns);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* _toolset_listed */
    listed_case("{}", "[]", "");
    listed_case("{\"toolsets\":[\"spotify\"]}", "[\"spotify\"]", "");
    listed_case("{\"toolsets\":[\"spotify\"]}", "[\"spotify\"]", "spotify");
    listed_case("{\"toolsets\":[\"a\",\"b\"]}", "[\"b\"]", "");
    listed_case("{\"toolsets\":[]}", "[\"a\"]", "");
    listed_case("{\"toolsets\":null}", "[\"a\"]", "");
    listed_case("{\"toolsets\":[1,2]}", "[\"1\"]", "");
    listed_case("{\"toolsets\":[1,2]}", "[\"3\"]", "");
    listed_case("{\"platform_toolsets\":{\"gui\":[\"spotify\"]}}", "[\"spotify\"]", "");
    listed_case("{\"platform_toolsets\":{\"gui\":[\"spotify\"]}}", "[\"spotify\"]", "gui");
    listed_case("{\"platform_toolsets\":{\"gui\":[\"spotify\"],\"cli\":[\"spotify\"]}}",
                "[\"spotify\"]", "gui");
    listed_case("{\"platform_toolsets\":{\"gui\":[\"a\"]}}", "[\"spotify\"]", "");
    listed_case("{\"platform_toolsets\":[]}", "[\"spotify\"]", "");
    listed_case("{\"platform_toolsets\":{\"gui\":\"spotify\"}}", "[\"spotify\"]", "");
    listed_case("{\"platform_toolsets\":{\"gui\":[null,1]}}", "[\"1\"]", "");
    listed_case("{\"toolsets\":[[\"a\"]]}", "[\"a\"]", "");
    listed_case("{\"toolsets\":[{\"x\":1}]}", "[\"x\"]", "");
    listed_case("{\"platform_toolsets\":{\"\":\"spotify\"}}", "[\"spotify\"]", "");

    /* _delegation_backend_routes */
    {
        const char *cases[] = {
            "{}",
            "{\"backends\":{}}",
            "{\"backends\":[]}",
            "{\"backends\":{\"a\":{\"options\":[{\"p\":1},{\"m\":2}]}}}",
            "{\"backends\":{\"a\":{\"options\":[{\"p\":1}]},\"b\":{\"options\":[{\"q\":2}]}}}",
            "{\"backends\":{\"a\":{\"options\":[1,\"x\",null,{\"ok\":1}]}}}",
            "{\"backends\":{\"a\":{\"options\":[]},\"b\":{}}}",
            "{\"backends\":{\"a\":{\"options\":[{\"p\":1}]},\"b\":null,\"c\":\"x\"}}",
            "{\"backends\":{\"a\":[1,2]}}",
            NULL,
        };
        for (int k = 0; cases[k]; k++) {
            char in[512];
            snprintf(in, sizeof(in), "{\"delegation\":%s}", cases[k]);
            json_t *d = json_parse(cases[k], NULL);
            emit_json("delegation_backend_routes", in,
                      lcm_delegation_backend_routes(d));
            json_free(d);
        }
    }

    /* _at_host */
    {
        const char *urls[] = {
            "https://api.example.com/v1", "api.example.com", "example.com",
            "https://sub.example.com", "https://notexample.com",
            "https://example.com.evil.com", "http://user:pw@example.com:8080/x",
            "ftp://example.com", "", "https://[::1", "https://[::1]/v1",
            "https://EXAMPLE.com", "https://example.com.", "https://a.b.example.com",
            NULL,
        };
        const char *hosts[] = {
            "example.com", "EXAMPLE.COM", "example.com.", "com", "example",
            "sub.example.com", "::1", "", "other.com", NULL,
        };
        for (int u = 0; urls[u]; u++) {
            for (int h = 0; hosts[h]; h++) {
                int r = lcm_at_host(urls[u], hosts[h]);
                printf("{\"case\":%d,\"fn\":\"at_host\",\"in\":{\"url\":", ++cs);
                jstr(urls[u]);
                printf(",\"host\":");
                jstr(hosts[h]);
                printf("},\"out\":%d}\n", r);
            }
        }
        fflush(stdout);
    }

    return 0;
}
