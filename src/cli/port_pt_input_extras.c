/*
 * port_pt_input_extras.c
 *
 * Pure helpers ported from hermes_cli/pt_input_extras.py - prompt_toolkit
 * keypress normalization for extended-key aliases.
 *
 * The module's real work is mutating prompt_toolkit's ANSI_SEQUENCES table.
 * What is portable is the modifier arithmetic:
 *
 *   pti_lock_variants <- pt_input_extras.py:_lock_variants
 *   pti_lock_twins    <- pt_input_extras.py:_lock_twins
 */

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

/* pt_input_extras.py:_LOCK_BIT_OFFSETS
 * base, +CapsLock, +NumLock, +both (xterm modifier encoding). */
static const int PTI_LOCK_BIT_OFFSETS[4] = { 0, 64, 128, 192 };

/* ===========================================================================
 *  _lock_variants  (pt_input_extras.py:_lock_variants)
 *
 *      tuple(modifier + off for off in _LOCK_BIT_OFFSETS)
 *
 *  Returns a newly allocated array of 4 ints; caller frees.
 * =========================================================================== */

/* PoP: _lock_variants @ hermes_cli/pt_input_extras.py:_lock_variants */
int *pti_lock_variants(int modifier)
{
    int *out = (int *)malloc(sizeof(PTI_LOCK_BIT_OFFSETS));
    if (!out) return NULL;
    for (size_t i = 0; i < sizeof(PTI_LOCK_BIT_OFFSETS) / sizeof(PTI_LOCK_BIT_OFFSETS[0]); i++)
        out[i] = modifier + PTI_LOCK_BIT_OFFSETS[i];
    return out;
}

/* ===========================================================================
 *  _lock_twins  (pt_input_extras.py:_lock_twins)
 *
 *      _lock_variants(modifier)[1:]
 *
 *  Only the lock twins, never the base value - a caller asking for the twins
 *  wants the three modified forms and not the unmodified modifier itself.
 *  Returns a newly allocated array of 3 ints; caller frees.
 * =========================================================================== */

/* PoP: _lock_twins @ hermes_cli/pt_input_extras.py:_lock_twins */
int *pti_lock_twins(int modifier)
{
    int *all = pti_lock_variants(modifier);
    if (!all) return NULL;
    int *out = (int *)malloc(3 * sizeof(int));
    if (!out) { free(all); return NULL; }
    memcpy(out, all + 1, 3 * sizeof(int));
    free(all);
    return out;
}
