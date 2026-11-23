#ifndef NOC_W10_KANBANOUTP_46C6B6_H
#define NOC_W10_KANBANOUTP_46C6B6_H
#include "hermes_json.h"

/* PoP port of hermes_cli/kanban_output.py - text / --json output helpers. */
/* ", ".join(f"{k}={v}" for k, v in sorted(counts.items())) or empty */
json_t *w10_kanbanoutp_46c6b6_fmt_counts(json_t *counts, json_t *empty);
/* _STATUS_ICONS.get(status, "?") */
const char *w10_kanbanoutp_46c6b6_status_icon(const char *status);
#endif /* NOC_W10_KANBANOUTP_46C6B6_H */
