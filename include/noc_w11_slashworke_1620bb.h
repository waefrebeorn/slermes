/* noc_w11_slashworke_1620bb.h — public C API for faithful port of upstream
 * Hermes tui_gateway/slash_worker.py (pure protocol helpers).
 *
 * PoP: w11_slashworke_1620bb_slash_base @ tui_gateway/slash_worker.py:_slash_base
 * PoP: w11_slashworke_1620bb_reply @ tui_gateway/slash_worker.py:_reply
 */
#ifndef NOC_W11_SLASHWORKE_1620BB_H
#define NOC_W11_SLASHWORKE_1620BB_H

#include "hermes_json.h"

/* _slash_base(command): strip -> removeprefix("/") -> split(maxsplit=1)[0]
 * -> lower(). Returns a malloc'd string (caller frees). */
char *w11_slashworke_1620bb_slash_base(const char *command);

/* main()'s per-line parse: raw.strip(), json.loads, {id, command}.
 * Returns JSON null for a blank line, else {"id":..,"command":..}. */
json_t *w11_slashworke_1620bb_parse_line(const char *raw);

/* SkillSlashRefused(base) message. Returns a malloc'd string (caller frees). */
char *w11_slashworke_1620bb_skill_refused_message(const char *base);

/* _reply(**fields): json.dumps(fields, Python default spacing) + "\n".
 * Returns a malloc'd string (caller frees). */
char *w11_slashworke_1620bb_reply(json_t *fields);

#endif /* NOC_W11_SLASHWORKE_1620BB_H */
