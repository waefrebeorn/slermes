#ifndef SLERMES_PORT_KANBAN_SCHEMAS_H
#define SLERMES_PORT_KANBAN_SCHEMAS_H

#include "json.h"

/* _prop(type_, description): {"type": type_, "description": description} */
json_t *kts_prop(const char *type_, const char *description);

/* _schema(name, description, properties, required): the model-facing tool
 * schema with name/description/parameters{type,properties,required}, appending
 * a trailing "board" property. properties_json/required_json are parsed JSON.
 * The board description mirrors the live _DESC_BOARD constant. */
json_t *kts_schema(const char *name, const char *description,
                   const char *properties_json, const char *required_json);

#endif /* SLERMES_PORT_KANBAN_SCHEMAS_H */
