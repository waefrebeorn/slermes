/* port_tools_fuzzy_match.h */
#ifndef PORT_TOOLS_FUZZY_MATCH_H
#define PORT_TOOLS_FUZZY_MATCH_H
typedef struct { int start; int end; } span_t;
span_t *fuzzy_map_normalized_positions(const char *original,
                                       const char *normalized,
                                       const span_t *matches,
                                       int n_matches,
                                       int *out_n);
#endif
