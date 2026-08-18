/* Standalone behavioral test for the image-gen requirements closure.
 * Links the REAL compiled objects (no re-implementation) and stubs only the
 * few cross-module symbols the requirements path doesn't actually need. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* Real implementations (linked from .o). */
extern bool image_gen_check_fal_api_key(void);
extern bool image_gen_check_image_generation_requirements(void);
extern char *image_gen_read_configured_image_provider(void);
extern bool igp_is_available(void);
extern char *igr_get_provider(const char *name);
extern bool plug_ensure_discovered(void);

/* --- no-op stubs for symbols outside the requirements path --- */
void hermes_log(int level, const char *ctx, const char *fmt, ...) { (void)level;(void)ctx;(void)fmt; }
bool plugin_ensure_discovered(void) { return true; }
const char *plugin_get_bundled_plugins_dir(void) { return ""; }
void plugin_install_debug_handler(void) {}
bool plugin_env_enabled(void) { return false; }
const char *plugin_get_disabled_plugins(void) { return ""; }
/* subagent_* are provided by subagent_lifecycle.o (do not stub). */
/* plug_ensure_discovered wraps plugin_ensure_discovered; provide directly. */
bool plug_ensure_discovered(void) { return plugin_ensure_discovered(); }

static int g_fail = 0;
#define CHECK(c,m) do{ if(c) printf("PASS: %s\n",m); else {printf("FAIL: %s\n",m); g_fail++;} }while(0)

int main(void) {
    unsetenv("HERMES_IMAGE_GEN_PROVIDER");
    setenv("FAL_KEY", "k", 1);
    CHECK(image_gen_check_image_generation_requirements() == true, "FAL key -> true");

    unsetenv("FAL_KEY");
    unsetenv("HERMES_IMAGE_GEN_PROVIDER");
    CHECK(image_gen_check_image_generation_requirements() == false, "no FAL, default fal -> false");

    setenv("HERMES_IMAGE_GEN_PROVIDER", "openai", 1);
    setenv("OPENAI_API_KEY", "sk", 1);
    CHECK(image_gen_check_image_generation_requirements() == true, "openai + key -> true");

    unsetenv("OPENAI_API_KEY");
    CHECK(image_gen_check_image_generation_requirements() == false, "openai no key -> false");

    setenv("HERMES_IMAGE_GEN_PROVIDER", "openai", 1);
    char *p = igr_get_provider("openai");
    CHECK(p != NULL, "igr_get_provider('openai') non-NULL");
    free(p);
    setenv("OPENAI_API_KEY", "sk", 1);
    CHECK(igp_is_available() == true, "igp openai w/ key -> true");
    unsetenv("OPENAI_API_KEY");
    CHECK(igp_is_available() == false, "igp openai w/o key -> false");

    if (g_fail) { printf("\n%d FAILURES\n", g_fail); return 1; }
    printf("\nALL PASS\n");
    return 0;
}
