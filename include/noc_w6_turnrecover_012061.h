#ifndef NOC_W6_TURNRECOVER_012061_H
#define NOC_W6_TURNRECOVER_012061_H
#include <stddef.h>

/* C home for upstream Hermes agent/turn_recovery.py (gate w6_turnrecover_012061).
 * This module is almost entirely in-place agent recovery; the genuinely pure
 * helper _image_error_max_dimension is ported. */

/* PoP: w6_turnrecover_image_error_max_dimension @ agent/turn_recovery.py:_image_error_max_dimension
 * error/message/body mirror str(exc), exc.message and exc.body (NULL/""
 * values are skipped, exactly like Python's `if value`).
 * Returns -1 for Python None, otherwise the clamped pixel ceiling. */
int w6_turnrecover_image_error_max_dimension(
    const char *error, const char *message, const char *body);

#endif /* NOC_W6_TURNRECOVER_012061_H */
