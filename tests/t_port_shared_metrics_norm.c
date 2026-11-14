/* t_port_shared_metrics_norm.c — oracle harness for the normalisation half of
 * hermes_cli/observability/shared_metrics_contract.py.
 * Emits one JSON line per case with a "fn" discriminator. */
#include "port_shared_metrics_contract.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const install_methods[] = {
    "apt", "docker", "git", "home-manager", "homebrew", "microsoft-store", "nixos", "pip",
    "unknown",
};
static const char *const skill_provenances[] = {
    "agent_created", "external", "installed", "local", "unknown",
};
static const char *const tool_categories[] = {
    "browser", "code_execution", "communication", "computer_use", "connector", "delegation",
    "file", "home_automation", "mcp", "media", "memory", "other", "planning", "project",
    "scheduler", "skill", "terminal", "unknown", "web",
};
static const char *const model_error_classes[] = {
    "auth", "auth_permanent", "billing", "content_policy_blocked", "context_overflow",
    "data_policy_denied", "format_error", "image_corrupt", "image_too_large",
    "incomplete_response", "invalid_encrypted_content", "llama_cpp_grammar_pattern",
    "long_context_tier", "model_entitlement", "model_not_found",
    "multimodal_tool_content_unsupported", "oauth_long_context_beta_forbidden", "overloaded",
    "payload_too_large", "provider_policy_blocked", "rate_limit", "reasoning_mandatory",
    "role_alternation", "server_error", "ssl_cert_verification", "thinking_signature",
    "timeout", "unknown", "upstream_blocked", "upstream_rate_limit", "none",
};

static int cs = 0;

static void jstr_escape(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar(ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

static void jstr(const char *s)
{
    if (!s) fputs("null", stdout);
    else jstr_escape(s);
}

static void emit_str(const char *fn, const char *in, const char *out)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":{\"str\":",
           ++cs, fn, in);
    jstr_escape(out ? out : "");
    printf("}}\n");
    fflush(stdout);
}

static void emit_json(const char *fn, const char *in, json_t *out)
{
    char *s = out ? json_serialize(out) : NULL;
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%s}\n",
           ++cs, fn, in, s ? s : "null");
    fflush(stdout);
    free(s);
    json_free(out);
}

static void emit_int(const char *fn, const char *in, long v)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%ld}\n", ++cs, fn, in, v);
    fflush(stdout);
}

/* Run one _allowlisted case. `set` selects the static table. */
static void allowlisted_case(const char *normalized, const char *set)
{
    const char *const *table;
    size_t n;
    if (strcmp(set, "install_methods") == 0) {
        table = install_methods; n = sizeof(install_methods) / sizeof(install_methods[0]);
    } else if (strcmp(set, "skill_provenances") == 0) {
        table = skill_provenances; n = sizeof(skill_provenances) / sizeof(skill_provenances[0]);
    } else if (strcmp(set, "tool_categories") == 0) {
        table = tool_categories; n = sizeof(tool_categories) / sizeof(tool_categories[0]);
    } else {
        table = model_error_classes;
        n = sizeof(model_error_classes) / sizeof(model_error_classes[0]);
    }
    printf("{\"case\":%d,\"fn\":\"allowlisted\",\"in\":{\"normalized\":",
           ++cs);
    jstr(normalized);
    printf(",\"set\":");
    jstr(set);
    printf("},\"out\":{\"str\":");
    jstr_escape(smc_allowlisted(normalized, table, n));
    printf("}}\n");
    fflush(stdout);
}

static void kw_case(const char *fn, const char *kwargs_json)
{
    json_t *kw = json_parse(kwargs_json, NULL);
    const char *out;
    if (strcmp(fn, "tool_category") == 0) out = smc_tool_category(kw);
    else if (strcmp(fn, "tool_outcome") == 0) out = smc_tool_outcome(kw);
    else out = smc_model_error_class(kw);
    char in[512];
    snprintf(in, sizeof(in), "{\"kwargs\":%s}", kwargs_json);
    emit_str(fn, in, out);
    json_free(kw);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* _allowlisted: hit, miss, empty, and the trailing-"unknown" member */
    allowlisted_case("pip", "install_methods");
    allowlisted_case("nixos", "install_methods");
    allowlisted_case("unknown", "install_methods");
    allowlisted_case("apt", "install_methods");
    allowlisted_case("nope", "install_methods");
    allowlisted_case("", "install_methods");
    allowlisted_case(NULL, "install_methods");
    allowlisted_case("LOCAL", "skill_provenances");
    allowlisted_case("local", "skill_provenances");
    allowlisted_case("agent_created", "skill_provenances");
    allowlisted_case("bogus", "skill_provenances");
    allowlisted_case("none", "model_error_classes");
    allowlisted_case("rate_limit", "model_error_classes");

    /* client_install_method: nix -> nixos, case/space folding, misses */
    {
        const char *cases[] = {"pip", "PIP", "  Git ", "nix", "Nix", "homebrew",
                               "microsoft-store", "snap", "", NULL};
        for (int k = 0; cases[k]; k++) {
            printf("{\"case\":%d,\"fn\":\"client_install_method\",\"in\":{\"value\":",
                   ++cs);
            jstr(cases[k]);
            printf("},\"out\":{\"str\":");
            jstr_escape(smc_client_install_method(cases[k]));
            printf("}}\n");
        }
        /* NULL reads as "" */
        printf("{\"case\":%d,\"fn\":\"client_install_method\",\"in\":{\"value\":null},"
               "\"out\":{\"str\":", ++cs);
        jstr_escape(smc_client_install_method(NULL));
        printf("}}\n");
        fflush(stdout);
    }

    /* skill_provenance */
    {
        const char *cases[] = {"local", "LOCAL", " external ", "agent_created",
                               "builtin", "", NULL};
        for (int k = 0; cases[k]; k++) {
            printf("{\"case\":%d,\"fn\":\"skill_provenance\",\"in\":{\"value\":", ++cs);
            jstr(cases[k]);
            printf("},\"out\":{\"str\":");
            jstr_escape(smc_skill_provenance(cases[k]));
            printf("}}\n");
        }
        fflush(stdout);
    }

    /* _metric_identifier */
    {
        struct { const char *v; size_t max; } cases[] = {
            {"  Hello.World-1  ", 64}, {"1abc", 64}, {"abc", 1}, {"abc", 3},
            {"a", 0}, {"", 64}, {"a_b.c:d/e@f+g-h", 64}, {"a b", 64},
            {"UPPER", 64}, {"a", 65}, {"x", 2},
        };
        size_t n = sizeof(cases) / sizeof(cases[0]);
        for (size_t k = 0; k < n; k++) {
            char *got = smc_metric_identifier(cases[k].v, cases[k].max);
            printf("{\"case\":%d,\"fn\":\"metric_identifier\",\"in\":{\"value\":", ++cs);
            jstr(cases[k].v);
            printf(",\"max_length\":%zu},\"out\":{\"str\":", cases[k].max);
            jstr_escape(got ? got : "");
            printf("}}\n");
            free(got);
        }
        printf("{\"case\":%d,\"fn\":\"metric_identifier\",\"in\":{\"value\":null,"
               "\"max_length\":64},\"out\":{\"str\":", ++cs);
        char *got = smc_metric_identifier(NULL, 64);
        jstr_escape(got ? got : "");
        printf("}}\n");
        free(got);
        fflush(stdout);
    }

    /* client_resource */
    {
        struct { const char *v, *o, *a, *m; } cases[] = {
            {" v1.2 ", "Darwin", "arm64", "nix"},
            {"v1", "linux", "x86_64", "pip"},
            {"", "Windows", "AMD64", "apt"},
            {"  ", "darwin", "aarch64", "Nix"},
            {"12345678901234567890123456789012345678901234567890123456789012345",
             "linux", "x86", "git"},
            {"V1.0-RC1", "macos", "armv8", "home-manager"},
            {NULL, NULL, NULL, NULL},
            {"v", "plan9", "sparc", "snap"},
        };
        size_t n = sizeof(cases) / sizeof(cases[0]);
        for (size_t k = 0; k < n; k++) {
            json_t *r = smc_client_resource(cases[k].v, cases[k].o,
                                            cases[k].a, cases[k].m);
            printf("{\"case\":%d,\"fn\":\"client_resource\",\"in\":{"
                   "\"hermes_version\":", ++cs);
            jstr(cases[k].v);
            printf(",\"os_name\":");
            jstr(cases[k].o);
            printf(",\"architecture\":");
            jstr(cases[k].a);
            printf(",\"install_method\":");
            jstr(cases[k].m);
            printf("},\"out\":");
            char *s = r ? json_serialize(r) : NULL;
            fputs(s ? s : "null", stdout);
            printf("}\n");
            free(s);
            json_free(r);
        }
        fflush(stdout);
    }

    /* client_resource_is_valid */
    {
        const char *cases[] = {
            "{\"architecture\":\"arm64\",\"hermes_version\":\"v1.2\","
            "\"install_method\":\"nixos\",\"os_family\":\"macos\"}",
            "{\"architecture\":\"arm64\",\"hermes_version\":\"v1.2\","
            "\"install_method\":\"nixos\",\"os_family\":\"macos\",\"extra\":\"x\"}",
            "{\"architecture\":\"arm64\",\"hermes_version\":\"v1.2\","
            "\"install_method\":\"nixos\"}",
            "{\"architecture\":\"sparc\",\"hermes_version\":\"v1.2\","
            "\"install_method\":\"nixos\",\"os_family\":\"macos\"}",
            "{\"architecture\":\"arm64\",\"hermes_version\":\"\","
            "\"install_method\":\"nixos\",\"os_family\":\"macos\"}",
            "{\"architecture\":\"arm64\",\"hermes_version\":123,"
            "\"install_method\":\"nixos\",\"os_family\":\"macos\"}",
            "{\"architecture\":\"arm64\",\"hermes_version\":\"v1.2\","
            "\"install_method\":\"snap\",\"os_family\":\"macos\"}",
            "[]",
            "null",
            NULL,
        };
        for (int k = 0; cases[k]; k++) {
            json_t *r = json_parse(cases[k], NULL);
            long v = smc_client_resource_is_valid(r);
            printf("{\"case\":%d,\"fn\":\"client_resource_is_valid\","
                   "\"in\":{\"resource\":%s},\"out\":%ld}\n",
                   ++cs, cases[k], v);
            json_free(r);
        }
        printf("{\"case\":%d,\"fn\":\"client_resource_is_valid\","
               "\"in\":{\"resource\":null},\"out\":%ld}\n", ++cs,
               (long)smc_client_resource_is_valid(NULL));
        fflush(stdout);
    }

    /* tool_category */
    kw_case("tool_category", "{}");
    kw_case("tool_category", "{\"toolset\":\"\"}");
    kw_case("tool_category", "{\"toolset\":\"mcp_foo\"}");
    kw_case("tool_category", "{\"toolset\":\"browser_use\"}");
    kw_case("tool_category", "{\"toolset\":\"image_gen\"}");
    kw_case("tool_category", "{\"toolset\":\"tts\"}");
    kw_case("tool_category", "{\"toolset\":\"homeassistant\"}");
    kw_case("tool_category", "{\"toolset\":\"discord\"}");
    kw_case("tool_category", "{\"toolset\":\"slack\"}");
    kw_case("tool_category", "{\"toolset\":\"hermes-yuanbao\"}");
    kw_case("tool_category", "{\"toolset\":\"clarify\"}");
    kw_case("tool_category", "{\"toolset\":\"kanban\"}");
    kw_case("tool_category", "{\"toolset\":\"todo\"}");
    kw_case("tool_category", "{\"toolset\":\"session_search\"}");
    kw_case("tool_category", "{\"toolset\":\"cronjob\"}");
    kw_case("tool_category", "{\"toolset\":\"skills\"}");
    kw_case("tool_category", "{\"toolset\":\"x_search\"}");
    kw_case("tool_category", "{\"toolset\":\"terminal\"}");
    kw_case("tool_category", "{\"toolset\":\"Terminal\"}");
    kw_case("tool_category", "{\"toolset\":\"  mcp_x  \"}");
    kw_case("tool_category", "{\"tool_name\":\"connectors__github\",\"toolset\":\"zzz\"}");
    kw_case("tool_category", "{\"tool_name\":\"connectors__\",\"toolset\":\"mcp_x\"}");
    kw_case("tool_category", "{\"tool_name\":\"other\",\"toolset\":\"web\"}");
    kw_case("tool_category", "{\"toolset\":123}");
    kw_case("tool_category", "{\"toolset\":true}");
    kw_case("tool_category", "{\"toolset\":null}");
    kw_case("tool_category", "{\"toolset\":[\"mcp\"]}");
    kw_case("tool_category", "{\"toolset\":[]}");
    kw_case("tool_category", "{\"toolset\":[\"mcp\",\"x\"]}");
    kw_case("tool_category", "{\"toolset\":{}}");
    kw_case("tool_category", "{\"toolset\":{\"a\":1}}");
    kw_case("tool_category", "{\"toolset\":{\"a\":1,\"b\":2}}");
    kw_case("tool_category", "{\"toolset\":0}");
    kw_case("tool_category", "{\"toolset\":1.5}");
    kw_case("tool_category", "{\"toolset\":false}");
    kw_case("tool_category", "{\"toolset\":true}");
    kw_case("tool_category", "{\"tool_name\":[\"connectors__x\"],\"toolset\":\"mcp\"}");
    kw_case("tool_category", "{\"tool_name\":true,\"toolset\":\"web\"}");
    kw_case("tool_category", "{\"tool_name\":false,\"toolset\":\"web\"}");
    kw_case("tool_category", "{\"tool_name\":{\"a\":1},\"toolset\":\"web\"}");
    kw_case("tool_category", "{\"tool_name\":\"CONNECTORS__x\",\"toolset\":\"web\"}");
    kw_case("tool_category", "{\"tool_name\":\"connectors__x\"}");
    kw_case("tool_category", "null");
    kw_case("tool_category", "[]");

    /* tool_outcome */
    kw_case("tool_outcome", "{}");
    kw_case("tool_outcome", "{\"status\":\"ok\"}");
    kw_case("tool_outcome", "{\"status\":\"OK\"}");
    kw_case("tool_outcome", "{\"status\":\" error \"}");
    kw_case("tool_outcome", "{\"status\":\"timeout\"}");
    kw_case("tool_outcome", "{\"status\":\"timed_out\"}");
    kw_case("tool_outcome", "{\"status\":\"blocked\"}");
    kw_case("tool_outcome", "{\"status\":\"cancelled\"}");
    kw_case("tool_outcome", "{\"status\":\"success\"}");
    kw_case("tool_outcome", "{\"status\":\"weird\"}");
    kw_case("tool_outcome", "{\"status\":null}");
    kw_case("tool_outcome", "{\"status\":[\"ok\"]}");
    kw_case("tool_outcome", "{\"status\":true}");
    kw_case("tool_outcome", "{\"status\":1}");

    /* model_error_class */
    kw_case("model_error_class", "{}");
    kw_case("model_error_class", "{\"reason\":\"rate_limit\"}");
    kw_case("model_error_class", "{\"reason\":\" RATE_LIMIT \"}");
    kw_case("model_error_class", "{\"reason\":\"none\"}");
    kw_case("model_error_class", "{\"reason\":\"unknown\"}");
    kw_case("model_error_class", "{\"reason\":\"auth\"}");
    kw_case("model_error_class", "{\"reason\":\"upstream_rate_limit\"}");
    kw_case("model_error_class", "{\"reason\":\"context_compression\"}");
    kw_case("model_error_class", "{\"reason\":\"bogus\"}");
    kw_case("model_error_class", "{\"reason\":\"\"}");

    return 0;
}
