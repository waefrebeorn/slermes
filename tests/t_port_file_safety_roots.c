/*
 * t_port_file_safety_roots.c — Oracle harness for
 * agent/file_safety.py:get_safe_write_roots
 * (ported to src/agent/file_safety.c as file_safety_get_safe_write_roots).
 *
 * Reads HERMES_WRITE_SAFE_ROOT from the environment (set by the test runner),
 * calls the port (which resolves ~ expansion + realpath per entry), and emits
 * the resulting JSON array. The Python oracle runs in the SAME env and
 * compares the sorted root set.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *file_safety_get_safe_write_roots(void);

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char *roots = file_safety_get_safe_write_roots();
    if (!roots) roots = strdup("[]");
    printf("%s\n", roots);
    free(roots);
    return 0;
}
