#ifndef NOC_SSURF_H
#define NOC_SSURF_H
/* NULL represents Python None (and the non-surfaces '', 'local', 'unknown');
 * otherwise returns a static string (never free). */
const char *ssurf_created_surface(const char *label);
#endif /* NOC_SSURF_H */
