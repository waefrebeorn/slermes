/*
 * port_chat_completion_helpers.c — Port of Python agent/chat_completion_helpers.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Port of Python: rewrite_prompt_model_identity */
void rewrite_prompt_model_identity(const char *model, const char *provider,
                                    char *prompt_out, size_t out_sz) {
    if (!prompt_out || out_sz == 0) return;
    /* Find and replace Model:/Provider: lines in the prompt */
    const char *model_prefix = "Model:";
    const char *provider_prefix = "Provider:";
    char *last_model = NULL;
    char *last_provider = NULL;
    
    /* Scan for last occurrence of each line */
    const char *p = prompt_out;
    while (*p) {
        if (strncmp(p, model_prefix, strlen(model_prefix)) == 0) {
            last_model = (char *)p;
        }
        if (strncmp(p, provider_prefix, strlen(provider_prefix)) == 0) {
            last_provider = (char *)p;
        }
        /* Advance to next line */
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    
    /* Replace last Model: line */
    if (last_model && model) {
        char *colon = strchr(last_model, ':');
        if (colon) {
            /* Replace after colon */
            size_t prefix_len = colon - last_model + 1;
            size_t model_len = strlen(model);
            memmove(colon + 1 + model_len, colon + 1 + strlen(colon + 1),
                    strlen(colon + 1) + 1);
            memcpy(colon + 1, model, model_len);
        }
    }
    
    /* Replace last Provider: line */
    if (last_provider && provider) {
        char *colon = strchr(last_provider, ':');
        if (colon) {
            size_t provider_len = strlen(provider);
            memmove(colon + 1 + provider_len, colon + 1 + strlen(colon + 1),
                    strlen(colon + 1) + 1);
            memcpy(colon + 1, provider, provider_len);
        }
    }
}

