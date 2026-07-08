/*
 * t_port_fuzzy_match.c — faithful verification for port_fuzzy_match.c.
 * Computes fz_map_normalized_positions for several fixed cases and prints
 * just the resulting (start,end) pairs per case. The oracle
 * (tests/fz_oracle.py) recomputes the LIVE Python source for the same
 * hardcoded cases and compares exactly.
 */

#include "port_fuzzy_match.c"
#include <stdio.h>

static void emit(int case_no, const char *orig, const char *norm,
               const int nm[][2], int n)
{
    int local[64][2];
    int got = fz_map_normalized_positions(orig, norm, nm, n, local);
    printf("CASE %d OUT", case_no);
    for (int i = 0; i < got; i++)
        printf(" %d,%d", local[i][0], local[i][1]);
    printf("\n");
}

int main(void)
{
    /* Case 1: simple whitespace collapse "a  b" -> "a b", match [0,3] */
    { const int nm[1][2] = {{0,3}}; emit(1, "a  b", "a b", nm, 1); }
    /* Case 2: tab->space + extra spaces collapsed */
    { const int nm[1][2] = {{0,5}}; emit(2, "a\t  b  c", "a b c", nm, 1); }
    /* Case 3: match ends with space, trailing original whitespace consumed */
    { const int nm[1][2] = {{0,2}}; emit(3, "a   b", "a b", nm, 1); }
    /* Case 4: no matches -> empty out */
    { const int nm[1][2] = {{0,0}}; emit(4, "hello world", "hello world", nm, 0); }
    /* Case 5: multiple matches (note: the 2nd normalized match index
     * (5) lies past the collapsed normalized string for this input, which
     * trips a latent min()-on-empty crash in the Python source itself —
     * omitted from the oracle compare; C handles it gracefully). */
    { const int nm[2][2] = {{0,1},{5,6}}; emit(5, "x  y  z", "x y z", nm, 2); }
    /* Case 6: issue #52491 — match ends with non-space, trailing ws NOT consumed */
    { const int nm[1][2] = {{0,1}}; emit(6, "a   b", "a b", nm, 1); }
    return 0;
}
