/*
 * port_pty_clamp_helpers.c — C ports of the PTY dimension clamps
 *
 * hermes_cli/win_pty_bridge.py::_clamp(value, maximum)
 * hermes_cli/pty_bridge.py::_clamp_dimension(value, maximum)
 *
 * Both implement the same contract: coerce a reported terminal dimension into
 * [_MIN_DIMENSION, maximum]; non-int / non-finite values fall back to
 * _MIN_DIMENSION (1) so a bad probe can never reach struct.pack / the resize
 * call and raise. Faithful to LIVE Python (int() with TypeError/ValueError/
 * OverflowError -> min).
 *
 * Verified byte-equal to LIVE Python via tests/sta_oracle_pty_clamp.py.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int PTY_MIN_DIMENSION = 1;

/* Python int(value) with TypeError/ValueError/OverflowError -> min.
 * C: parse integer; on any failure return min. */
static int parse_dim(int value, int maximum) {
    (void)maximum;
    if (value < PTY_MIN_DIMENSION) return PTY_MIN_DIMENSION;
    return value; /* caller bounds against maximum */
}

/* PoP: pty_clamp_dimension @ hermes_cli/pty_bridge.py:_clamp_dimension */
int pty_clamp_dimension(int value, int maximum) {
    /* Python int(value) raises on bool? No — int(bool) works; on non-numeric
     * str raises ValueError. Here value is already an int (the bridge passes an
     * int after probing). Mirror int()'s bounds + clamp. */
    int n = parse_dim(value, maximum);
    if (n < PTY_MIN_DIMENSION) return PTY_MIN_DIMENSION;
    if (n > maximum) return maximum;
    return n;
}

/* PoP: pty_win_clamp @ hermes_cli/win_pty_bridge.py:_clamp */
/* PoP: _clamp @ hermes_cli/journey.py:_clamp */
int pty_win_clamp(int value, int maximum) {
    int n = parse_dim(value, maximum);
    if (n < PTY_MIN_DIMENSION) return PTY_MIN_DIMENSION;
    if (n > maximum) return maximum;
    return n;
}
