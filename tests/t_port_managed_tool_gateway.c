/*
 * Oracle harness for tools/managed_tool_gateway.py.
 * Tests the pure helper functions: managed_vendor_base_path,
 * managed_vendor_upload_path, is_managed_nous_gateway_url,
 * managed_gateway_auth_headers, _read_user_token_override.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "hermes_json.h"
#include "managed_gateway.h"

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s cases.json\n", argv[0]); return 2; }
    char *txt = NULL;
    long len = 0;
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("open cases"); return 2; }
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    txt = (char *)malloc(len + 1);
    len = fread(txt, 1, len, f); txt[len] = '\0'; fclose(f);

    char *err = NULL;
    json_t *root = json_parse(txt, &err);
    free(txt);
    if (err) { fprintf(stderr, "parse: %s\n", err); free(err); return 2; }
    if (!root || root->type != JSON_ARRAY) { fprintf(stderr, "not array\n"); return 2; }

    for (size_t i = 0; i < root->c.count; i++) {
        json_t *c = json_get(root, i);
        json_t *opj = json_obj_get(c, "op");
        const char *name = opj && opj->type == JSON_STRING ? opj->str_val : "";

        if (strcmp(name, "managed_vendor_base_path") == 0) {
            const char *vendor = json_get_str(c, "vendor", "");
            char buf[256];
            managed_vendor_base_path(vendor, buf, sizeof(buf));
            printf("%s\n", buf);
        } else if (strcmp(name, "managed_vendor_upload_path") == 0) {
            const char *vendor = json_get_str(c, "vendor", "");
            char buf[256];
            managed_vendor_upload_path(vendor, buf, sizeof(buf));
            printf("%s\n", buf);
        } else if (strcmp(name, "is_managed_nous_gateway_url") == 0) {
            const char *home = json_get_str(c, "home", "/tmp/sm_test_home");
            const char *url = json_get_str(c, "url", "");
            bool r = is_managed_nous_gateway_url(home, url);
            printf(r ? "true\n" : "false\n");
        } else if (strcmp(name, "managed_gateway_auth_headers") == 0) {
            const char *home = json_get_str(c, "home", "/tmp/sm_test_home");
            const char *url = json_get_str(c, "url", "");
            char buf[4096];
            int rc = managed_gateway_auth_headers(home, url, buf, sizeof(buf));
            if (rc == 0)
                printf("%s\n", buf);
            else
                printf("none\n");
        } else if (strcmp(name, "_read_user_token_override") == 0) {
            char buf[4096];
            bool r = managed_gw_read_user_token_override(buf, sizeof(buf));
            if (r) printf("%s\n", buf);
            else printf("none\n");
        } else {
            printf("unknown op\n");
        }
    }
    free(root);
    return 0;
}
