/*
 * plugin_manifest_test.c — behavioral test for port_plugin_manifest.c
 *
 * Mirrors the documented semantics of plugins.py:_parse_manifest /
 * PluginManifest. I/O (YAML load, __init__.py scan) is injected by a fake
 * detector callback.
 */

#include "plugin_manifest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int checks = 0, failures = 0;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } } while (0)
#define EQ_STR(a, b, msg) do { checks++; if (!(a) || !(b) || strcmp((a),(b))!=0) { failures++; printf("FAIL: %s (\"%s\" != \"%s\")\n", msg, (a)?(a):"(null)", (b)?(b):"(null)"); } } while (0)
#define HAS(h, n, msg) do { checks++; if (!strstr((h),(n))) { failures++; printf("FAIL: %s (missing %s)\n", msg, (n)); } } while (0)

static void test_basic_parse(void) {
    const char *json = "{\"name\":\"disk-cleanup\",\"version\":\"1.2.3\","
                        "\"description\":\"Cleans disk\",\"author\":\"bob\","
                        "\"kind\":\"standalone\","
                        "\"requires_env\":[\"OPENAI_API_KEY\",{\"name\":\"FOO\",\"scope\":\"read\"}],"
                        "\"provides_tools\":[\"cleanup\",\"wipe\"],"
                        "\"provides_hooks\":[\"on_start\"]}";
    plugin_manifest_t *m = plugin_manifest_parse(json, "disk-cleanup", "", "user", NULL);
    CHECK(m != NULL, "parse ok");
    EQ_STR(plugin_manifest_name(m), "disk-cleanup", "name");
    EQ_STR(plugin_manifest_version(m), "1.2.3", "version");
    EQ_STR(plugin_manifest_author(m), "bob", "author");
    EQ_STR(plugin_manifest_source(m), "user", "source");
    EQ_STR(plugin_manifest_key(m), "disk-cleanup", "flat key = name");
    EQ_STR(plugin_manifest_kind(m), "standalone", "kind standalone");

    /* requires_env: 2 entries; str + dict */
    CHECK(plugin_manifest_req_env_count(m) == 2, "req_env count");
    const plugin_req_env_t *e0 = plugin_manifest_req_env(m, 0);
    EQ_STR(e0->name, "OPENAI_API_KEY", "req_env[0] name (str)");
    CHECK(e0->extra_json == NULL, "req_env[0] no extra");
    const plugin_req_env_t *e1 = plugin_manifest_req_env(m, 1);
    EQ_STR(e1->name, "FOO", "req_env[1] name (dict)");
    HAS(e1->extra_json, "\"scope\":\"read\"", "req_env[1] extra json preserved");

    /* provides_tools / hooks */
    CHECK(plugin_manifest_provides_tools_count(m) == 2, "tools count");
    EQ_STR(plugin_manifest_provides_tool(m, 0), "cleanup", "tool[0]");
    CHECK(plugin_manifest_provides_hooks_count(m) == 1, "hooks count");
    EQ_STR(plugin_manifest_provides_hook(m, 0), "on_start", "hook[0]");

    /* round-trip */
    char *out = plugin_manifest_to_json(m);
    HAS(out, "\"name\":\"disk-cleanup\"", "to_json name");
    HAS(out, "\"kind\":\"standalone\"", "to_json kind");
    HAS(out, "\"key\":\"disk-cleanup\"", "to_json key");
    HAS(out, "\"requires_env\"", "to_json requires_env");
    free(out);
    plugin_manifest_free(m);
}

static void test_key_derivation(void) {
    /* nested plugin: prefix/image_gen + dir openai -> key "image_gen/openai" */
    plugin_manifest_t *m = plugin_manifest_parse("{}", "openai", "image_gen", "user", NULL);
    EQ_STR(plugin_manifest_name(m), "openai", "name fallback to dir");
    EQ_STR(plugin_manifest_key(m), "image_gen/openai", "nested key = prefix/dir");
    plugin_manifest_free(m);

    /* flat plugin with explicit name */
    m = plugin_manifest_parse("{\"name\":\"myplug\"}", "myplug", "", "project", NULL);
    EQ_STR(plugin_manifest_key(m), "myplug", "flat key = name");
    plugin_manifest_free(m);
}

static void test_kind_validation(void) {
    /* invalid kind -> standalone */
    plugin_manifest_t *m = plugin_manifest_parse("{\"kind\":\"FLUX_CAPACITOR\"}", "x", "", "user", NULL);
    EQ_STR(plugin_manifest_kind(m), "standalone", "invalid kind -> standalone");
    plugin_manifest_free(m);

    /* valid kind, mixed case -> lowercased */
    m = plugin_manifest_parse("{\"kind\":\"Backend\"}", "x", "", "user", NULL);
    EQ_STR(plugin_manifest_kind(m), "backend", "kind lowercased");
    plugin_manifest_free(m);

    /* all valid kinds accepted */
    const char *kinds[] = {"standalone","backend","exclusive","platform","model-provider"};
    for (int i = 0; i < 5; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"kind\":\"%s\"}", kinds[i]);
        plugin_manifest_t *mm = plugin_manifest_parse(buf, "x", "", "user", NULL);
        EQ_STR(plugin_manifest_kind(mm), kinds[i], "valid kind preserved");
        plugin_manifest_free(mm);
    }
    CHECK(plugin_manifest_kind_is_valid("standalone"), "kind_is_valid standalone");
    CHECK(!plugin_manifest_kind_is_valid("bogus"), "kind_is_valid bogus false");
}

/* detector: mimics __init__.py scan heuristic (memory provider / model provider) */
static const char *fake_detector(const char *dir_name) {
    if (strcmp(dir_name, "memprov") == 0) return "exclusive";
    if (strcmp(dir_name, "modelprov") == 0) return "model-provider";
    return NULL;
}

static void test_kind_auto_coercion(void) {
    /* no "kind" in manifest and detector returns a kind -> coerced */
    plugin_manifest_t *m = plugin_manifest_parse("{}", "memprov", "", "user", fake_detector);
    EQ_STR(plugin_manifest_kind(m), "exclusive", "auto-coerce memory provider -> exclusive");
    plugin_manifest_free(m);

    m = plugin_manifest_parse("{}", "modelprov", "", "user", fake_detector);
    EQ_STR(plugin_manifest_kind(m), "model-provider", "auto-coerce model provider -> model-provider");
    plugin_manifest_free(m);

    /* dir unknown -> stays standalone */
    m = plugin_manifest_parse("{}", "plain", "", "user", fake_detector);
    EQ_STR(plugin_manifest_kind(m), "standalone", "no coercion when detector returns NULL");
    plugin_manifest_free(m);

    /* explicit kind present -> detector NOT applied */
    m = plugin_manifest_parse("{\"kind\":\"backend\"}", "memprov", "", "user", fake_detector);
    EQ_STR(plugin_manifest_kind(m), "backend", "explicit kind wins over detector");
    plugin_manifest_free(m);
}

int main(void) {
    test_basic_parse();
    test_key_derivation();
    test_kind_validation();
    test_kind_auto_coercion();
    printf("plugin_manifest_test: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
