/* Generic multi-arg oracle harness for port_*.c functions taking 2-3
 * const char* args and returning double/int/char* (deterministic, no IO).
 * Usage: harness_multi <cfunc> <arg1> [arg2] [arg3]
 * The harness declares the target as an extern with a flexible signature set;
 * we resolve at link time. To keep it simple, each target is declared with its
 * exact signature and dispatched by name.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- declare every candidate we wire (add more as needed) --- */
extern double cli_gateway_platforms_yuanbao_sticker__multiset_char_hit_ratio(const char *needle, const char *haystack);
extern double cli_gateway_platforms_yuanbao_sticker__bigram_jaccard(const char *a, const char *b);
extern double cli_gateway_platforms_yuanbao_sticker__longest_subsequence_ratio(const char *needle, const char *haystack);
extern double cli_gateway_platforms_yuanbao_sticker__score_field(const char *haystack, const char *query);
extern int    env_line_defines_key(const char *line, const char *key);
extern char  *web_windows_build_number(const char *version, const char *platform_label);
extern int    cli_tools_url_safety__allows_private_ip_resolution(const char *hostname, const char *scheme);
extern int    cli_tools_website_policy__match_host_against_rule(const char *host, const char *pattern);
extern int    cli_gateway_platforms_yuanbao_sticker__compact_text(const char *raw, char *buf, size_t bufsz);
/* batch 2 */
extern int    aux__is_codex_gpt54_or_gpt55(const char *model, const char *provider);
extern int    aux__is_codex_spark(const char *model, const char *provider);
extern bool   agent_thinking_timeout_is_thinking_timeout(const char *reason_value, const char *model, const char *err);
extern int    toolset_allowed_for_platform(const char *ts_key, const char *platform);
extern bool   provider_supports_explicit_api_mode(const char *provider, const char *configured_provider);
extern int    curses_query_matches(const char *label, const char *query);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <func> <a1> [a2] [a3]\n", argv[0]); return 2; }
    const char *func = argv[1];
    const char *a1 = argv[2];
    const char *a2 = argc > 3 ? argv[3] : "";
    const char *a3 = argc > 4 ? argv[4] : "";
    if (strcmp(func, "multiset_char_hit_ratio") == 0)   printf("%.10g\n", cli_gateway_platforms_yuanbao_sticker__multiset_char_hit_ratio(a1,a2));
    else if (strcmp(func, "bigram_jaccard") == 0)       printf("%.10g\n", cli_gateway_platforms_yuanbao_sticker__bigram_jaccard(a1,a2));
    else if (strcmp(func, "longest_subsequence_ratio") == 0) printf("%.10g\n", cli_gateway_platforms_yuanbao_sticker__longest_subsequence_ratio(a1,a2));
    else if (strcmp(func, "score_field") == 0)          printf("%.10g\n", cli_gateway_platforms_yuanbao_sticker__score_field(a1,a2));
    else if (strcmp(func, "env_line_defines_key") == 0) printf("%d\n", env_line_defines_key(a1,a2));
    else if (strcmp(func, "web_windows_build_number") == 0) { char *r=web_windows_build_number(a1,a2); printf("%s\n", r?r:""); free(r); }
    else if (strcmp(func, "allows_private_ip_resolution") == 0) printf("%d\n", cli_tools_url_safety__allows_private_ip_resolution(a1,a2));
    else if (strcmp(func, "match_host_against_rule") == 0) printf("%d\n", cli_tools_website_policy__match_host_against_rule(a1,a2));
    else if (strcmp(func, "compact_text") == 0) { char buf[4096]; cli_gateway_platforms_yuanbao_sticker__compact_text(a1,buf,sizeof(buf)); printf("%s\n", buf); }
    else if (strcmp(func, "is_codex_gpt54_or_gpt55") == 0) printf("%d\n", aux__is_codex_gpt54_or_gpt55(a1,a2));
    else if (strcmp(func, "is_codex_spark") == 0) printf("%d\n", aux__is_codex_spark(a1,a2));
    else if (strcmp(func, "thinking_timeout_is_thinking_timeout") == 0) printf("%d\n", agent_thinking_timeout_is_thinking_timeout(a1,a2,a3));
    else if (strcmp(func, "toolset_allowed_for_platform") == 0) printf("%d\n", toolset_allowed_for_platform(a1,a2));
    else if (strcmp(func, "provider_supports_explicit_api_mode") == 0) printf("%d\n", provider_supports_explicit_api_mode(a1,a2));
    else if (strcmp(func, "query_matches") == 0) printf("%d\n", curses_query_matches(a1,a2));
    else { fprintf(stderr, "unknown func %s\n", func); return 4; }
    return 0;
}
