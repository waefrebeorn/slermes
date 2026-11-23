/*
 * __init__.c — C port of upstream Hermes pm/__init__.py (lazy facade).
 *
 * PoP: w5init_all  @ pm/__init__.py:__all__
 * PoP: w5init_home @ pm/__init__.py:_HOME.get
 * PoP: w5init_has  @ pm/__init__.py:name in _HOME
 *
 * _HOME = {name: module for module, names in _EXPORTS.items() for name in
 * names}. Python dict insertion order is preserved, so __all__ is the
 * first-occurrence order of the names below.
 */
#include "noc_w5_init_703dd3.h"
#include <string.h>

#define E(module, ...)                                                       \
    { module, (const char *const[]){ __VA_ARGS__ },                          \
      sizeof((const char *const[]){ __VA_ARGS__ }) / sizeof(const char *) }

typedef struct {
    const char *module;
    const char *const *names;
    size_t n;
} w5init_export_t;

static const w5init_export_t EXPORTS[] = {
    E("pm.install", "activate", "check", "drift", "enabled_extras", "env_for",
      "is_installed", "installed_package", "lazy_installs_allowed",
      "store_first_path", "uv_launcher"),
    E("pm.client", "ensure", "sync_venv", "build_environment", "lock_project",
      "stage_manager_runtime", "ensure_environment", "ensure_project_environment",
      "ensure_python_tool", "venv_is_current", "check_project_lock",
      "export_requirements", "build_requirements_environment", "prune_cache",
      "stage_tools", "prepare_tools"),
    E("pm.operations", "environment_python", "python_tool"),
    E("pm.extras", "available", "ensure_import", "install_hint"),
    E("pm.lock", "Facts", "Lockfile"),
    E("pm.package", "InstallError", "Package", "Runner", "compose_env"),
    E("pm.registry", "all_packages", "get_package", "register", "walk"),
    E("pm.store", "Store", "current_target"),
};
#define NEXPORTS (sizeof(EXPORTS) / sizeof(EXPORTS[0]))

static const char *ALL[128];
static size_t ALLN = 0;

const char *const *w5init_all(size_t *outn) {
    if (!ALLN) {
        for (size_t m = 0; m < NEXPORTS; m++) {
            for (size_t i = 0; i < EXPORTS[m].n; i++) {
                const char *name = EXPORTS[m].names[i];
                int seen = 0;
                for (size_t k = 0; k < ALLN; k++)
                    if (!strcmp(ALL[k], name)) { seen = 1; break; }
                if (!seen && ALLN < sizeof(ALL) / sizeof(ALL[0]))
                    ALL[ALLN++] = name;
            }
        }
    }
    if (outn) *outn = ALLN;
    return ALL;
}

const char *w5init_home(const char *name) {
    if (!name) return NULL;
    for (size_t m = 0; m < NEXPORTS; m++)
        for (size_t i = 0; i < EXPORTS[m].n; i++)
            if (!strcmp(EXPORTS[m].names[i], name)) return EXPORTS[m].module;
    return NULL;
}

int w5init_has(const char *name) { return w5init_home(name) != NULL; }
