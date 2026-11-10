/* t_port_kanban_schemas.c — oracle harness for _prop.
 * Emits {"tag":"prop","case":N,"type":<json>,"desc":<json>,"out":<json string>}. */
#include "port_kanban_schemas.h"

#include <stdio.h>
#include <stdlib.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    static const char *const types[] = {
        "string", "integer", "boolean", "array", "object", "string", ""
    };
    static const char *const descs[] = {
        "The column title", "The order position", "Whether archived",
        "Task ids", "The card fields", "with \"quotes\" and \\ backslash", ""
    };
    size_t n = sizeof(types) / sizeof(types[0]);
    for (size_t i = 0; i < n; i++) {
        json_t *b = kts_prop(types[i], descs[i]);
        char *s = json_serialize(b);
        printf("{\"tag\":\"prop\",\"case\":%zu,\"type\":", i);
        jstr(types[i]);
        printf(",\"desc\":");
        jstr(descs[i]);
        printf(",\"out\":");
        jstr(s);
        printf("}\n");
        free(s); json_free(b);
    }
    /* _schema: name, description, properties (JSON), required (JSON) */
    {
        const char *name = "kanban_show";
        const char *desc = "Show a kanban task.";
        const char *props = "{\"task_id\":{\"type\":\"string\",\"description\":\"Task id.\"},\"detail\":{\"type\":\"boolean\",\"description\":\"Full detail.\"}}";
        const char *req = "[\"task_id\"]";
        json_t *s = kts_schema(name, desc, props, req);
        char *s2 = json_serialize(s);
        printf("{\"tag\":\"schema\",\"case\":0,\"name\":");
        jstr(name);
        printf(",\"desc\":");
        jstr(desc);
        printf(",\"props\":%s,\"required\":%s,\"out\":", props, req);
        jstr(s2);
        printf("}\n");
        free(s2); json_free(s);
    }
    return 0;
}
