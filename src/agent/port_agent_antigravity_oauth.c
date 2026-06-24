/*
 * port_agent_antigravity_oauth.c — Port of Python agent/antigravity_oauth.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _candidate_discovery_roots */
typedef struct {
    char paths[16][1024];
    int count;
} path_list_t;

path_list_t antigravity_candidate_discovery_roots(void) {
    path_list_t result = {0};
    const char *home = getenv("HOME");
    if (!home) return result;
    
    snprintf(result.paths[0], 1024, "%s/.config/antigravity", home);
    snprintf(result.paths[1], 1024, "%s/.antigravity", home);
    snprintf(result.paths[2], 1024, "/etc/antigravity");
    result.count = 3;
    return result;
}


/* Port of Python: _iter_discovery_files */
typedef struct {
    char files[64][1024];
    int count;
} file_list_t;

file_list_t antigravity_iter_discovery_files(void) {
    file_list_t result = {0};
    path_list_t roots = antigravity_candidate_discovery_roots();
    
    for (int i = 0; i < roots.count && result.count < 64; i++) {
        /* Check for common token file names */
        const char *token_files[] = {"token.json", "credentials.json", "oauth.json", NULL};
        for (int j = 0; token_files[j]; j++) {
            char path[2048];
            snprintf(path, sizeof(path), "%s/%s", roots.paths[i], token_files[j]);
            FILE *f = fopen(path, "r");
            if (f) {
                fclose(f);
                strncpy(result.files[result.count], path, 1023);
                result.count++;
            }
        }
    }
    return result;
}


/* Port of Python: _secret_candidates */
typedef struct {
    char secrets[64][2048];
    int count;
} secret_list_t;

secret_list_t antigravity_secret_candidates(void) {
    secret_list_t result = {0};
    file_list_t files = antigravity_iter_discovery_files();
    
    for (int i = 0; i < files.count && result.count < 64; i++) {
        FILE *f = fopen(files.files[i], "r");
        if (f) {
            /* Read file content as potential secret */
            size_t n = fread(result.secrets[result.count], 1, 2047, f);
            result.secrets[result.count][n] = '\0';
            result.count++;
            fclose(f);
        }
    }
    return result;
}


/* Port of Python: run_antigravity_oauth_login_pure */
bool antigravity_run_oauth_login_pure(char *token_out, size_t out_sz) {
    if (!token_out || out_sz == 0) return false;
    
    /* Run the Antigravity OAuth PKCE flow */
    /* In C, this would start a local server and open a browser */
    /* Simplified: check for existing token first */
    secret_list_t candidates = antigravity_secret_candidates();
    if (candidates.count > 0) {
        strncpy(token_out, candidates.secrets[0], out_sz - 1);
        token_out[out_sz - 1] = '\0';
        return true;
    }
    
    return false; /* No token found, would need browser flow */
}

