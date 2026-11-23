#ifndef NOC_W15_KANBANDDBB_576F01_H
#define NOC_W15_KANBANDDBB_576F01_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_cli/kanban_db_boards.py (gate w15_kanbandbb_576f01).
 * All results are freshly allocated json_t values freed by the caller. */

/* PoP: w15_kanbandbb_576f01_default_board_display_name @ hermes_cli/kanban_db_boards.py:_default_board_display_name */
struct json_t *w15_kanbandbb_576f01_default_board_display_name(const char *slug);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_KANBANDDBB_576F01_H */
