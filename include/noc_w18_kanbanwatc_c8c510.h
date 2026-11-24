/* noc_w18_kanbanwatc_c8c510.h — public C API for gateway/kanban_watchers_common.py port. */
#ifndef NOC_W18_KANBANWATC_C8C510_H
#define NOC_W18_KANBANWATC_C8C510_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_kanbanwatc_c8c510_list_boards @ gateway/kanban_watchers_common.py:_list_boards */
json_t *w18_kanbanwatc_c8c510_list_boards(int list_raises, json_t *boards, json_t *metadata);
/* PoP: w18_kanbanwatc_c8c510_board_slugs @ gateway/kanban_watchers_common.py:_board_slugs */
json_t *w18_kanbanwatc_c8c510_board_slugs(int list_raises, json_t *boards,
                                          json_t *metadata, const char *default_board);
/* PoP: w18_kanbanwatc_c8c510_positive_int @ gateway/kanban_watchers_common.py:_positive_int_setting */
json_t *w18_kanbanwatc_c8c510_positive_int(json_t *cfg, const char *key);
/* PoP: w18_kanbanwatc_c8c510_auto_decompose @ gateway/kanban_watchers_common.py:_resolve_auto_decompose_settings */
json_t *w18_kanbanwatc_c8c510_auto_decompose(int config_error, json_t *cfg);
/* PoP: w18_kanbanwatc_c8c510_gc_retention @ gateway/kanban_watchers_common.py:_gc_retention_days */
json_t *w18_kanbanwatc_c8c510_gc_retention(int config_error, json_t *cfg);
/* PoP: w18_kanbanwatc_c8c510_dispatch_allowed @ gateway/kanban_watchers_common.py:_kanban_dispatch_allowed */
json_t *w18_kanbanwatc_c8c510_dispatch_allowed(int import_error, int paused);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_KANBANWATC_C8C510_H */
