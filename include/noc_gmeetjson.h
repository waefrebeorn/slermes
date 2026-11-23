#ifndef NOC_GMEETJSON_H
#define NOC_GMEETJSON_H
/* Opaque forward declaration (matches libjson's `typedef struct json_t json_t`). */
typedef struct json_t json_t;

/* Parsed JSON from path, or NULL when the file is missing/unreadable/empty/
 * malformed. Mirrors read_json's use of encoding="utf-8-sig" (BOM stripped). */
json_t *gmeetjson_read_json(const char *path);
#endif
