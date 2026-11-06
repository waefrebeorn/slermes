#ifndef SLERMES_PORT_KANBAN_SCHEMAS_H
#define SLERMES_PORT_KANBAN_SCHEMAS_H

#include "json.h"

/* _prop(type_, description): {"type": type_, "description": description} */
json_t *kts_prop(const char *type_, const char *description);

#endif /* SLERMES_PORT_KANBAN_SCHEMAS_H */
