/*
 * pet_state.c — Map agent activity to a PetState
 *
 * Port of Python: agent/pet/state.py
 * Single source of truth for "what is the agent doing?" → "which animation row?"
 */
#include <string.h>
#include <stdbool.h>
#include "pet.h"

/* PoP: pet_todos_all_done @ agent/pet/state.py:todos_all_done */
bool pet_todos_all_done(const char **statuses, int count) {
    if (!statuses || count <= 0)
        return false;
    for (int i = 0; i < count; i++) {
        if (!statuses[i]) return false;
        if (strcmp(statuses[i], "completed") != 0 &&
            strcmp(statuses[i], "cancelled") != 0)
            return false;
    }
    return true;
}

/* PoP: pet_state_derive @ agent/pet/state.py:derive_pet_state */
pet_state_t pet_state_derive(
    bool busy,
    bool awaiting_input,
    bool error,
    bool celebrate,
    bool just_completed,
    bool tool_running,
    bool reasoning)
{
    /* Priority order (highest first) — only one row shows at a time */
    if (error)              return PET_STATE_FAILED;
    if (celebrate)          return PET_STATE_JUMP;
    if (just_completed)     return PET_STATE_WAVE;
    if (awaiting_input)     return PET_STATE_WAITING;
    if (tool_running)       return PET_STATE_RUN;
    if (reasoning)          return PET_STATE_REVIEW;
    if (busy)               return PET_STATE_RUN;
    return PET_STATE_IDLE;
}
