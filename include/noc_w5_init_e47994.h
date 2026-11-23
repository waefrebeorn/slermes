#ifndef NOC_W5_INIT_E47994_H
#define NOC_W5_INIT_E47994_H

#include <stddef.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_init_e47994_check_file_requirements @ tools/__init__.py:check_file_requirements
 * The namespace module only delegates: the terminal backend's requirement check
 * result is injected by the fixture (no tool-stack import at namespace load). */
int w5_init_e47994_check_file_requirements(int delegated);

/* PoP: w5_init_e47994_all @ tools/__init__.py:__all__ */
struct json_t *w5_init_e47994_all(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_INIT_E47994_H */
