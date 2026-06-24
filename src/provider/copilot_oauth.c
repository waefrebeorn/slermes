/*
 * copilot_oauth.c — GitHub Copilot OAuth device code flow.
 * Port of Python hermes_cli/copilot_auth.py.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define COPILOT_CLIENT_ID       "Ov23li8tweQw6odWQebz"
#define DEVICE_CODE_URL         "https://github.com/login/device/code"
#define OAUTH_TOKEN_URL         "https://github.com/login/oauth/access_token"
#define POLL_INTERVAL_SEC       5

int copilot_validate_token(const char *token, char *err_msg, size_t err_sz) {
    if (!token || !*token) {
        if (err_msg) snprintf(err_msg, err_sz, "Empty token");
        return 1;
    }
    if (strncmp(token, "ghp_", 4) == 0) {
        if (err_msg) snprintf(err_msg, err_sz,
            "Classic PATs (ghp_*) not supported.");
        return 1;
    }
    if (strncmp(token, "gho_", 4) == 0 ||
        strncmp(token, "github_pat_", 11) == 0 ||
        strncmp(token, "ghu_", 4) == 0) return 0;
    return 0;
}

/* Port of Python tools/skills_hub.py:_resolve_token(). */
int copilot_resolve_token(char *out, size_t out_sz, char *source, size_t src_sz) {
    static const char *env_vars[] = {
        "COPILOT_GITHUB_TOKEN", "GH_TOKEN", "GITHUB_TOKEN", NULL
    };
    char err_buf[256];

    for (int i = 0; env_vars[i]; i++) {
        const char *val = getenv(env_vars[i]);
        if (val && val[0]) {
            int rc = copilot_validate_token(val, err_buf, sizeof(err_buf));
            if (rc == 0) {
                snprintf(out, out_sz, "%s", val);
                if (source) snprintf(source, src_sz, "%s", env_vars[i]);
                return 0;
            }
        }
    }

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "gh auth token 2>/dev/null");
    FILE *fp = popen(cmd, "r");
    if (fp) {
        if (fgets(out, (int)out_sz, fp) && out[0]) {
            size_t len = strlen(out);
            while (len > 0 && (out[len-1] == '\n' || out[len-1] == '\r'))
                out[--len] = '\0';
            int rc = copilot_validate_token(out, err_buf, sizeof(err_buf));
            if (rc == 0) {
                if (source) snprintf(source, src_sz, "gh auth token");
                pclose(fp);
                return 0;
            }
        }
        pclose(fp);
    }
    out[0] = '\0';
    return 1;
}

int copilot_device_code_flow(char *out_token, size_t token_sz) {
    char post_data[256];
    snprintf(post_data, sizeof(post_data),
             "client_id=%s&scope=read:user%%20copilot", COPILOT_CLIENT_ID);

    char resp_file[64];
    snprintf(resp_file, sizeof(resp_file), "/tmp/copilot_dev_XXXXXX");
    int fd = mkstemp(resp_file);
    if (fd < 0) return 1;
    close(fd);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "curl -s -X POST '%s' -d '%s' -H 'Accept: application/json' -o '%s'",
        DEVICE_CODE_URL, post_data, resp_file);

    if (system(cmd) != 0) { unlink(resp_file); return 1; }

    char *json_str = NULL;
    {
        FILE *fp = fopen(resp_file, "r");
        if (!fp) { unlink(resp_file); return 1; }
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        rewind(fp);
        json_str = (char *)malloc((size_t)fsize + 1);
        if (!json_str) { fclose(fp); unlink(resp_file); return 1; }
        size_t n = fread(json_str, 1, (size_t)fsize, fp);
        json_str[n] = '\0';
        fclose(fp);
    }
    unlink(resp_file);

    json_t *root = json_parse(json_str, NULL);
    free(json_str);
    if (!root) return 1;

    const char *device_code = json_get_str(root, "device_code", NULL);
    const char *user_code = json_get_str(root, "user_code", NULL);
    const char *verif_uri = json_get_str(root, "verification_uri", NULL);
    double expires_in = json_get_num(root, "expires_in", 900);

    if (!device_code || !user_code || !verif_uri) {
        json_free(root);
        return 1;
    }

    printf("\n=== GitHub Copilot OAuth ===\n");
    printf("1. Open: %s\n", verif_uri);
    printf("2. Enter code: %s\n\n", user_code);
    json_free(root);

    int max_polls = (int)(expires_in / POLL_INTERVAL_SEC);
    char token_post[512];
    snprintf(token_post, sizeof(token_post),
             "client_id=%s&device_code=%s&grant_type=urn:ietf:params:oauth:grant-type:device_code",
             COPILOT_CLIENT_ID, device_code);

    for (int i = 0; i < max_polls; i++) {
        sleep(POLL_INTERVAL_SEC + 3);

        char token_file[64];
        snprintf(token_file, sizeof(token_file), "/tmp/copilot_tok_XXXXXX");
        fd = mkstemp(token_file);
        if (fd < 0) continue;
        close(fd);

        snprintf(cmd, sizeof(cmd),
            "curl -s -X POST '%s' -d '%s' -H 'Accept: application/json' -o '%s'",
            OAUTH_TOKEN_URL, token_post, token_file);

        if (system(cmd) != 0) { unlink(token_file); continue; }

        FILE *fp = fopen(token_file, "r");
        if (!fp) { unlink(token_file); continue; }
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        rewind(fp);
        char *tok_json = (char *)malloc((size_t)fsize + 1);
        if (!tok_json) { fclose(fp); unlink(token_file); continue; }
        size_t n = fread(tok_json, 1, (size_t)fsize, fp);
        tok_json[n] = '\0';
        fclose(fp);
        unlink(token_file);

        json_t *tok_root = json_parse(tok_json, NULL);
        free(tok_json);
        if (!tok_root) continue;

        const char *access_token = json_get_str(tok_root, "access_token", NULL);
        const char *error = json_get_str(tok_root, "error", NULL);

        if (access_token) {
            snprintf(out_token, token_sz, "%s", access_token);
            json_free(tok_root);
            printf("[copilot-oauth] Authentication successful!\n");
            return 0;
        }

        if (error) {
            if (strcmp(error, "authorization_pending") == 0 ||
                strcmp(error, "slow_down") == 0) {
                json_free(tok_root);
                continue;
            }
            fprintf(stderr, "[copilot-oauth] Error: %s\n", error);
            json_free(tok_root);
            return 1;
        }
        json_free(tok_root);
    }

    fprintf(stderr, "[copilot-oauth] Authentication timed out\n");
    return 1;
}
