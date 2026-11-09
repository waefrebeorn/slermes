/*
 * port_pt_input_extras.h -- pure modifier arithmetic ported from
 * hermes_cli/pt_input_extras.py.
 *
 * The prompt_toolkit ANSI_SEQUENCES mutations stay in Python.
 */
#ifndef PORT_PT_INPUT_EXTRAS_H
#define PORT_PT_INPUT_EXTRAS_H

#include <stddef.h>

/* pt_input_extras.py:_lock_variants -- modifier plus its CapsLock / NumLock /
 * both twins: {m, m+64, m+128, m+192}. Caller frees. */
int *pti_lock_variants(int modifier);

/* pt_input_extras.py:_lock_twins -- the three lock twins, never the base
 * value. Caller frees. */
int *pti_lock_twins(int modifier);

#endif /* PORT_PT_INPUT_EXTRAS_H */
