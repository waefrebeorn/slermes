#ifndef NOC_MODFINGER_H
#define NOC_MODFINGER_H
struct json_t;
/* Returns JSON object {"profile": "PROFILE"|null, "parts": [...]} (caller frees). */
struct json_t *modfinger_process_catalog_identity(struct json_t *ctx);
#endif
