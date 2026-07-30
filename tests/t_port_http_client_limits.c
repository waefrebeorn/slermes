/* Oracle harness for gateway/platforms/_http_client_limits.py port.
 * Reads fixture (argv[1]): {"env": {"HERMES_GATEWAY_HTTPX_KEEPALIVE_EXPIRY": "...",
 *                                    "HERMES_GATEWAY_HTTPX_MAX_KEEPALIVE": "..."}}
 * Applies env, calls platform_httpx_limits(), prints the resolved config as a
 * compact JSON object: {"available":bool,"keepalive_expiry":float,"max_keepalive":int}.
 * Byte-diffed against the Python oracle.
 */
#include "gateway/platforms/http_client_limits.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)n, f); buf[n] = '\0'; fclose(f);
    return buf;
}

/* Print a double the way Python's json.dumps does: integral values keep a
 * trailing ".0" (e.g. 2.0), non-integral use shortest round-trip. */
static void print_json_float(double v) {
    if (v == (double)(long)v && v >= -1e15 && v <= 1e15) {
        printf("%ld.0", (long)v);
    } else {
        printf("%.17g", v);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *src = read_file(argv[1]);
    if (!src) return 1;
    json_t *root = json_parse(src, NULL);

    /* Apply env overrides from the fixture. */
    if (root && root->type == JSON_OBJECT) {
        json_t *env = json_object_get(root, "env");
        if (env && env->type == JSON_OBJECT) {
            for (size_t i = 0; i < env->c.count; i++) {
                const char *k = env->c.keys[i];
                json_t *v = env->c.items[i];
                if (k && v && v->type == JSON_STRING)
                    setenv(k, v->str_val, 1);
            }
        }
    }

    http_client_limits_t lim = platform_httpx_limits();
    printf("{\"available\":%s,\"keepalive_expiry\":",
           lim.httpx_available ? "true" : "false");
    print_json_float(lim.keepalive_expiry);
    printf(",\"max_keepalive\":%d}", lim.max_keepalive);

    if (root) json_free(root);
    free(src);
    return 0;
}
