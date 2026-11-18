/* lcm_deps_stub.c — oracle-link dependency stubs for left_core_migration.
 *
 * The harness tests lcm_toolset_listed / lcm_delegation_backend_routes /
 * lcm_at_host. Linking the real provider_registry.o (which supplies
 * provider_base_url_host_matches) drags in three symbols used only by its
 * config-loading paths, which no harness case exercises. Stub those three
 * here so the harness links without the whole config/yaml closure.
 *
 * These are DEPENDENCY stubs, never stubs of the code under test. The real
 * objects remain named in the DEPS table of tests/run_all_oracles.sh.
 */
#include <stddef.h>
#include "hermes_json.h"
#include "yaml.h"

yaml_doc_t *yaml_parse_file(const char *path, char **error_msg)
{
    (void)path;
    if (error_msg) *error_msg = NULL;
    return NULL;
}

json_t *config_py_get_compatible_custom_providers(const json_t *config)
{
    (void)config;
    return NULL;
}

const char *slermes_home(void)
{
    return ".";
}
