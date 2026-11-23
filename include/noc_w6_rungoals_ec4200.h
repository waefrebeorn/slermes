#ifndef NOC_W6_RUNGOALS_EC4200_H
#define NOC_W6_RUNGOALS_EC4200_H
#include <stddef.h>
/* Pre-resolved post-turn final text for /goal and /loop after a gateway turn.
 * Returns the original (untrimmed) text when non-blank, else a valid streamed
 * final response, else the original blank text. */
/* PoP: w6_rungoals_ec4200_final_text @ gateway/run_goals.py:_final_text_for_post_turn_hooks */
const char *w6_rungoals_ec4200_final_text(const char *text, int has_event, const char *streamed);
/* Configured /goal turn budget from the resolved "goals" block.
 * kind: 0=falsy/absent ->20, 1=number, 2=string, 3=bool. */
/* PoP: w6_rungoals_ec4200_goal_max_turns @ gateway/run_goals.py:_goal_max_turns_from_config */
long w6_rungoals_ec4200_goal_max_turns(int kind, const char *val);
#endif
