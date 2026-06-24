/*
 * port_gateway_rich_sent_store.c — Port of Python gateway/rich_sent_store.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: lookup */
typedef struct {
    char text[4096];
    bool found;
} rich_sent_result_t;

rich_sent_result_t rich_sent_store_lookup(const char *chat_id, const char *message_id) {
    rich_sent_result_t result = {0};
    if (!chat_id || !message_id) return result;
    
    /* Look up stored text for (chat_id, message_id) pair */
    /* In C, this would read from a JSON file or database */
    /* Simplified implementation */
    
    const char *store_dir = getenv("HERMES_HOME");
    if (!store_dir) store_dir = ".";
    
    char path[1024];
    snprintf(path, sizeof(path), "%s/.rich_sent_store.json", store_dir);
    
    FILE *f = fopen(path, "r");
    if (!f) return result;
    
    /* Read file and search for key */
    char *content = NULL;
    size_t sz = 0;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    content = malloc(sz + 1);
    if (content) {
        fread(content, 1, sz, f);
        content[sz] = '\0';
        
        /* Build key */
        char key[512];
        snprintf(key, sizeof(key), "%s:%s", chat_id, message_id);
        
        /* Search for key in JSON */
        const char *found = strstr(content, key);
        if (found) {
            const char *val = strchr(found, ':');
            if (val) {
                val = strchr(val + 1, '"');
                if (val) {
                    val++;
                    size_t i = 0;
                    while (*val && *val != '"' && i < sizeof(result.text) - 1) {
                        result.text[i++] = *val++;
                    }
                    result.text[i] = '\0';
                    result.found = (i > 0);
                }
            }
        }
        free(content);
    }
    fclose(f);
    return result;
}

