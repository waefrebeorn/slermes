/*
 * test_registry_enum.c — Faithful port of tools/registry.py ToolRegistry
 * enumeration + toolset-alias API: get_registered_toolset_names,
 * get_tool_names_for_toolset, get_all_tool_names, get_tool_to_toolset_map,
 * get_toolset_for_tool, register_toolset_alias, get_toolset_alias_target,
 * get_registered_toolset_aliases.
 *
 * Verifies sorted-unique semantics, alias overwrite, and JSON shape.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "registry.h"

static int g_fail = 0;
#define TEST(c,m) do { if(!(c)){ fprintf(stderr,"FAIL: %s\n",m); g_fail++; } } while(0)

/* stub for tool_error (not exercised on these paths). */
char *tool_error(const char *m, ...) { (void)m; return strdup("{\"success\":false}"); }

/* dummy no-op handler so registry_register_ex accepts the registration. */
static char *dummy_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id; return strdup("{\"ok\":true}");
}

static void free_names(char **names) {
    if (!names) return;
    for (size_t i = 0; names[i]; i++) free(names[i]);
    free(names);
}

int main(void) {
    /* Register a small controlled set directly. */
    registry_register_ex("enum_a1", "a1", "{}", "alpha", dummy_handler);
    registry_register_ex("enum_a2", "a2", "{}", "alpha", dummy_handler);
    registry_register_ex("enum_b1", "b1", "{}", "beta",  dummy_handler);
    registry_register_ex("enum_b2", "b2", "{}", "beta",  dummy_handler);
    registry_register_ex("enum_g1", "g1", "{}", "gamma", dummy_handler);

    /* get_all_tool_names sorted */
    {
        size_t n = 0; char **names = registry_get_all_tool_names(&n);
        TEST(n == 5, "all tool names count");
        /* sorted: enum_a1, enum_a2, enum_b1, enum_b2, enum_g1, ... plus any
         * pre-registered builtins; just assert our 5 are present + sorted. */
        int present = 0;
        for (size_t i = 0; i < n; i++)
            if (!strcmp(names[i], "enum_a1") || !strcmp(names[i], "enum_a2") ||
                !strcmp(names[i], "enum_b1") || !strcmp(names[i], "enum_b2") ||
                !strcmp(names[i], "enum_g1")) present++;
        TEST(present == 5, "all 5 controlled tools present");
        if (n > 1) TEST(strcmp(names[0], names[n-1]) <= 0, "names sorted ascending");
        free_names(names);
    }

    /* get_registered_toolset_names sorted-unique */
    {
        size_t n = 0; char **ts = registry_get_registered_toolset_names(&n);
        int a=0,b=0,g=0;
        for (size_t i = 0; i < n; i++) {
            if (!strcmp(ts[i],"alpha")) a++;
            else if (!strcmp(ts[i],"beta")) b++;
            else if (!strcmp(ts[i],"gamma")) g++;
        }
        TEST(a==1 && b==1 && g==1, "toolsets alpha/beta/gamma each unique");
        free_names(ts);
    }

    /* get_tool_names_for_toolset */
    {
        size_t n = 0; char **names = registry_get_tool_names_for_toolset("alpha", &n);
        TEST(n == 2, "alpha has 2 tools");
        int a1=0,a2=0;
        for (size_t i=0;i<n;i++){ if(!strcmp(names[i],"enum_a1"))a1++; if(!strcmp(names[i],"enum_a2"))a2++; }
        TEST(a1==1 && a2==1, "alpha = {enum_a1, enum_a2}");
        free_names(names);

        size_t nz=0; char **none = registry_get_tool_names_for_toolset("nonexistent", &nz);
        TEST(nz==0, "unknown toolset -> 0 tools");
        free_names(none);
    }

    /* get_toolset_for_tool */
    TEST(!strcmp(registry_get_toolset_for_tool("enum_b1"), "beta"), "toolset_for_tool enum_b1=beta");
    TEST(registry_get_toolset_for_tool("nope") == NULL, "unknown tool -> NULL toolset");

    /* get_tool_to_toolset_map JSON */
    {
        char *map = registry_get_tool_to_toolset_map();
        json_node_t *j = json_parse(map, NULL);
        TEST(j != NULL, "map parses as JSON");
        if (j) {
            TEST(!strcmp(json_object_get_string(j, "enum_a1", ""), "alpha"), "map enum_a1=alpha");
            TEST(!strcmp(json_object_get_string(j, "enum_g1", ""), "gamma"), "map enum_g1=gamma");
            json_free(j);
        }
        free(map);
    }

    /* toolset alias subsystem */
    registry_register_toolset_alias("web", "browser");
    registry_register_toolset_alias("ui", "browser");
    TEST(!strcmp(registry_get_toolset_alias_target("web"), "browser"), "alias web->browser");
    TEST(!strcmp(registry_get_toolset_alias_target("ui"),  "browser"), "alias ui->browser");
    TEST(registry_get_toolset_alias_target("zzz") == NULL, "unknown alias -> NULL");
    /* overwrite */
    registry_register_toolset_alias("web", "net");
    TEST(!strcmp(registry_get_toolset_alias_target("web"), "net"), "alias web overwritten -> net");

    {
        char *aliases = registry_get_registered_toolset_aliases();
        json_node_t *j = json_parse(aliases, NULL);
        TEST(j != NULL, "aliases parse as JSON");
        if (j) {
            TEST(!strcmp(json_object_get_string(j, "web", ""), "net"),  "aliases web=net");
            TEST(!strcmp(json_object_get_string(j, "ui",  ""), "browser"), "aliases ui=browser");
            json_free(j);
        }
        free(aliases);
    }

    /* per-tool max_result_size + requires_env via registry_register_ex_full */
    {
        const char *envs[2] = { "HERMES_DESKTOP", "SOME_TOKEN" };
        registry_register_ex_full("enum_req", "needs env", "{}", "alpha", dummy_handler,
                                  envs, 2, 12345);
        TEST(registry_get_max_result_size("enum_req", 0) == 12345, "per-tool max result size honored");
        TEST(registry_get_max_result_size("enum_a1", 0) == REGISTRY_DEFAULT_RESULT_SIZE_CHARS,
             "no per-tool size -> global default");
        TEST(registry_get_max_result_size("enum_a1", 555) == 555, "no per-tool size -> default arg");
        TEST(registry_get_max_result_size("nope", 0) == REGISTRY_DEFAULT_RESULT_SIZE_CHARS,
             "unknown tool -> global default");

        /* toolset requirements JSON */
        char *req = registry_check_toolset_requirements();
        json_node_t *j = json_parse(req, NULL);
        TEST(j != NULL, "check_toolset_requirements parses");
        if (j) {
            /* alpha has enum_a1/a2/enum_req all available -> true */
            TEST(json_object_get_bool(j, "alpha", 0) == 1, "alpha available");
            json_free(j);
        }
        free(req);

        /* available toolsets: alpha lists our tools */
        char *avail = registry_get_available_toolsets();
        json_node_t *ja = json_parse(avail, NULL);
        TEST(ja != NULL, "get_available_toolsets parses");
        if (ja) {
            json_node_t *alpha = json_object_get(ja, "alpha");
            TEST(alpha != NULL, "alpha present in available toolsets");
            if (alpha) {
                json_node_t *tools = json_object_get(alpha, "tools");
                TEST(tools != NULL && json_len(tools) >= 3, "alpha tools >= 3");
            }
            json_free(ja);
        }
        free(avail);

        /* toolset requirements: enum_req's env vars surface under alpha */
        char *tr = registry_get_toolset_requirements();
        json_node_t *jt = json_parse(tr, NULL);
        TEST(jt != NULL, "get_toolset_requirements parses");
        if (jt) {
            json_node_t *alpha = json_object_get(jt, "alpha");
            TEST(alpha != NULL, "alpha present");
            if (alpha) {
                json_node_t *ev = json_object_get(alpha, "env_vars");
                int seen_desktop = 0;
                for (size_t m = 0; m < json_len(ev); m++)
                    if (!strcmp(json_node_get_string(json_get(ev, m)), "HERMES_DESKTOP")) seen_desktop = 1;
                TEST(seen_desktop == 1, "requires_env HERMES_DESKTOP surfaced");
            }
            json_free(jt);
        }
        free(tr);
    }

    if (g_fail==0) printf("ALL PASSED\n"); else printf("%d FAIL\n", g_fail);
    return g_fail?1:0;
}
