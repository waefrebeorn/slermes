#ifndef NOC_W6_TURNAUTHOR_9C0BAB_H
#define NOC_W6_TURNAUTHOR_9C0BAB_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_turnauthor_9c0bab_env_key @ agent/turn_author.py:TURN_AUTHOR_ENV */
const char *w6_turnauthor_9c0bab_env_key(void);

/* PoP: w6_turnauthor_9c0bab_clean_text @ agent/turn_author.py:_clean_text
 * Drops Unicode Other categories (Cc/Cs/Co; unassigned Cn is not tracked),
 * strips whitespace, caps at 200 code points.  NULL == Python None. Caller frees. */
char *w6_turnauthor_9c0bab_clean_text(const char *value);

/* PoP: w6_turnauthor_9c0bab_bot_flag @ agent/turn_author.py:_bot_flag
 * String inputs are stripped + ASCII-lowercased before the truthy-set check;
 * JSON bool/number inputs follow Python's bool() coercion. */
int w6_turnauthor_9c0bab_bot_flag(const struct json_t *value);

/* PoP: w6_turnauthor_9c0bab_bot_author_id @ agent/turn_author.py:bot_author_id
 * NULL origin is treated as "".  Caller frees. */
char *w6_turnauthor_9c0bab_bot_author_id(const char *profile, const char *origin);

/* PoP: w6_turnauthor_9c0bab_parse_turn_author @ agent/turn_author.py:parse_turn_author
 * `raw` is a JSON object, or a JSON string holding a JSON object (parsed as
 * json.loads would be).  Returns a NEW JSON object {"id","name","is_bot"}
 * plus optional "hops", or JSON null for anything Python would reject. Caller frees. */
struct json_t *w6_turnauthor_9c0bab_parse_turn_author(const struct json_t *raw);

/* PoP: w6_turnauthor_9c0bab_turn_author_env @ agent/turn_author.py:turn_author_env
 * Returns {"HERMES_TURN_AUTHOR": json.dumps(author, separators=(",", ":"))}
 * as a NEW JSON object. Caller frees. */
struct json_t *w6_turnauthor_9c0bab_turn_author_env(const struct json_t *author);

/* PoP: w6_turnauthor_9c0bab_a2a_key @ agent/turn_author.py:a2a_key
 * "a2a:<id>" for a bot author with a truthy id, else NULL. Caller frees. */
char *w6_turnauthor_9c0bab_a2a_key(const struct json_t *author);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_TURNAUTHOR_9C0BAB_H */
