/*
 * port_gateway_rich_sent_store.c — Port of Python gateway/rich_sent_store.py
 */
#include <stdio.h>
#include "hermes_gateway_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"


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

/* PoP: rich_sent_store_path @ gateway/rich_sent_store.py:_store_path */
char *rich_sent_store_path(void)
{
    /* Resolve the rich sent index store path.
     * Port of Python gateway/rich_sent_store.py:_store_path(). */
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    static char path[1024];
    snprintf(path, sizeof(path), "%s/state/rich_sent_index.json", home);

    /* Ensure directory exists */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/state", home);
    struct stat st = {0};
    if (stat(dir, &st) != 0) mkdir(dir, 0755);

    return path;
}

/* PoP: rich_sent_record @ gateway/rich_sent_store.py:record */
bool rich_sent_record(const char *chat_id, const char *message_id, const char *text)
{
    /* Persist text for (chat_id, message_id). No-op on any failure.
     * Port of Python gateway/rich_sent_store.py:record(). */
    if (!text || !*text || !chat_id || !message_id) return false;

    const char *path = rich_sent_store_path();
    json_t *data = NULL;

    /* Read existing store */
    FILE *f = fopen(path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        rewind(f);
        if (sz > 0 && sz < 1048576) {
            char *buf = malloc((size_t)sz + 1);
            if (buf) {
                size_t n = fread(buf, 1, (size_t)sz, f);
                buf[n] = '\0';
                data = json_parse(buf, NULL);
                free(buf);
            }
        }
        fclose(f);
    }

    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        data = json_object();
    }

    /* Build key: chat_id:message_id */
    char key[512];
    snprintf(key, sizeof(key), "%s:%s", chat_id, message_id);

    /* Store truncated text with timestamp */
    json_t *entry = json_object();
    size_t text_len = strlen(text);
    size_t max_text = text_len < 2048 ? text_len : 2048;
    char *truncated = strndup(text, max_text);
    if (truncated) {
        json_set(entry, "t", json_string(truncated));
        free(truncated);
    }
    json_set(entry, "ts", json_number((double)time(NULL)));
    json_set(data, key, entry);

    /* Trim when over 10000 entries — skip for now */

    /* Write back */
    char *json_str = json_serialize(data);
    if (json_str) {
        FILE *out = fopen(path, "w");
        if (out) {
            fputs(json_str, out);
            fclose(out);
        }
        free(json_str);
    }

    json_free(data);
    return true;
}
