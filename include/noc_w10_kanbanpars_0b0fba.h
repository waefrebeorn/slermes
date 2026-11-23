#ifndef NOC_W10_KANBANPARS_0B0FBA_H
#define NOC_W10_KANBANPARS_0B0FBA_H
#include "hermes_json.h"

/* PoP port of hermes_cli/kanban_parser.py — argparse tree data constructors. */
/* Returns {"ok":"<canonical decimal>"} or {"err":"<ArgumentTypeError text>"}. */
json_t *w10_kanbanpars_0b0fba_nonnegative_int(const char *value);
/* _bulk_ids(verb) -> [["--ids"], {"nargs":"+","help":...}] */
json_t *w10_kanbanpars_0b0fba_bulk_ids(const char *verb);
/* _reason(help) -> [["--reason"], {"help":...}] */
json_t *w10_kanbanpars_0b0fba_reason(const char *help);
/* _json_flag(**kw) -> [["--json"], {"action":"store_true", ...kw}] */
json_t *w10_kanbanpars_0b0fba_json_flag(json_t *kw);
#endif /* NOC_W10_KANBANPARS_0B0FBA_H */
