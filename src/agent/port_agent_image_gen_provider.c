/*
 * port_agent_image_gen_provider.c — Port of Python agent/image_gen_provider.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: normalize_reference_images */
typedef struct {
    char items[16][1024];
    int count;
} ref_image_list_t;

ref_image_list_t normalize_reference_images(const char *value) {
    ref_image_list_t result = {0};
    if (!value) return result;
    
    /* Accept a single string or JSON array */
    const char *p = value;
    while (*p == ' ' || *p == '[') p++;
    
    while (*p && result.count < 16) {
        /* Skip whitespace/commas */
        while (*p == ' ' || *p == ',') p++;
        if (*p == ']' || *p == '\0') break;
        
        /* Extract string value */
        if (*p == '"') {
            p++;
            size_t i = 0;
            while (*p && *p != '"' && i < 1023) {
                result.items[result.count][i++] = *p++;
            }
            result.items[result.count][i] = '\0';
            if (i > 0) result.count++;
            if (*p == '"') p++;
        } else {
            p++;
        }
    }
    return result;
}

