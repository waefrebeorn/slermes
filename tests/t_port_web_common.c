/* t_port_web_common.c — oracle harness for plugins/web/_common.py.
 * Emits one JSON line per case with a "fn" discriminator. */
#include "port_web_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *P(const char *s)
{
    return json_parse(s, NULL);
}

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

/* "out" is emitted as a JSON STRING so a bare scalar survives. */
static void emit(int cs, const char *fn, const char *in, json_t *out)
{
    char *s = out ? json_serialize(out) : NULL;
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":", cs, fn, in);
    jstr_escape(s ? s : "null");
    printf("}\n");
    fflush(stdout);
    free(s);
    json_free(out);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* search_ok / search_fail */
    emit(1, "search_ok", "{\"results\":[{\"url\":\"u\",\"title\":\"t\"}]}",
         webc_search_ok(P("[{\"url\":\"u\",\"title\":\"t\"}]")));
    emit(2, "search_ok", "{\"results\":[]}", webc_search_ok(P("[]")));
    emit(3, "search_fail", "{\"error\":\"boom\"}",
         webc_search_fail("boom"));

    /* web_hit / title_hit */
    emit(4, "web_hit",
         "{\"url\":\"u\",\"title\":\"t\",\"description\":\"d\",\"position\":1}",
         webc_web_hit("u", "t", "d", 1));
    emit(5, "title_hit",
         "{\"title\":\"t\",\"url\":\"u\",\"description\":\"d\",\"position\":2}",
         webc_title_hit("t", "u", "d", 2));

    /* document */
    emit(6, "document",
         "{\"url\":\"u\",\"title\":\"t\",\"content\":\"c\",\"source_url\":null}",
         webc_document("u", "t", "c", NULL));
    emit(7, "document",
         "{\"url\":\"u\",\"title\":\"t\",\"content\":\"c\",\"source_url\":\"s\"}",
         webc_document("u", "t", "c", "s"));

    /* page_error / extract_fail / extract_interrupted */
    emit(8, "page_error", "{\"url\":\"u\",\"error\":\"e\"}",
         webc_page_error("u", "e"));
    emit(9, "extract_fail", "{\"urls\":[\"a\",\"b\"],\"error\":\"e\"}",
         webc_extract_fail(P("[\"a\",\"b\"]"), "e"));
    emit(10, "extract_fail", "{\"urls\":[],\"error\":\"e\"}",
         webc_extract_fail(P("[]"), "e"));
    emit(11, "extract_interrupted", "{\"urls\":[\"a\",\"b\"]}",
         webc_extract_interrupted(P("[\"a\",\"b\"]")));

    /* is_origin_http_error */
    emit(12, "is_origin_http_error", "{\"error\":\"HTTP 403 from origin\"}",
         json_new_bool(webc_is_origin_http_error("HTTP 403 from origin")));
    emit(13, "is_origin_http_error", "{\"error\":\"HTTP 403 from origin: blocked\"}",
         json_new_bool(webc_is_origin_http_error("HTTP 403 from origin: blocked")));
    emit(14, "is_origin_http_error", "{\"error\":\"HTTP 404 from originx\"}",
         json_new_bool(webc_is_origin_http_error("HTTP 404 from originx")));
    emit(15, "is_origin_http_error", "{\"error\":\"http 403 from origin\"}",
         json_new_bool(webc_is_origin_http_error("http 403 from origin")));
    emit(16, "is_origin_http_error", "{\"error\":null}",
         json_new_bool(webc_is_origin_http_error(NULL)));

    /* origin_http_error */
    emit(17, "origin_http_error", "{\"status\":403,\"reason\":null}",
         webc_origin_http_error(P("403"), NULL));
    emit(18, "origin_http_error", "{\"status\":403,\"reason\":\"  blocked  \"}",
         webc_origin_http_error(P("403"), "  blocked  "));
    emit(19, "origin_http_error", "{\"status\":200,\"reason\":null}",
         webc_origin_http_error(P("200"), NULL));
    emit(20, "origin_http_error", "{\"status\":\"404\",\"reason\":\"nope\"}",
         webc_origin_http_error(P("\"404\""), "nope"));
    emit(21, "origin_http_error", "{\"status\":true,\"reason\":null}",
         webc_origin_http_error(P("true"), NULL));
    emit(22, "origin_http_error", "{\"status\":\"abc\",\"reason\":null}",
         webc_origin_http_error(P("\"abc\""), NULL));
    emit(23, "origin_http_error", "{\"status\":500,\"reason\":\"\"}",
         webc_origin_http_error(P("500"), ""));
    emit(24, "origin_http_error", "{\"status\":null,\"reason\":null}",
         webc_origin_http_error(P("null"), NULL));

    /* exa_crawl_status */
    emit(25, "exa_crawl_status", "{\"text\":\"x CRAWL_HTTP_429\"}",
         webc_exa_crawl_status("x CRAWL_HTTP_429"));
    emit(26, "exa_crawl_status", "{\"text\":\"CRAWL_NOT_FOUND\"}",
         webc_exa_crawl_status("CRAWL_NOT_FOUND"));
    emit(27, "exa_crawl_status", "{\"text\":\"CRAWL_HTTP_404  \"}",
         webc_exa_crawl_status("CRAWL_HTTP_404  "));
    emit(28, "exa_crawl_status", "{\"text\":\"xCRAWL_NOT_FOUND\"}",
         webc_exa_crawl_status("xCRAWL_NOT_FOUND"));
    emit(29, "exa_crawl_status", "{\"text\":\"nothing\"}",
         webc_exa_crawl_status("nothing"));

    /* keenable_origin_error */
    emit(30, "keenable_origin_error",
         "{\"status\":403,\"body\":\"{\\\"message\\\":\\\"the target server returned 403\\\"}\"}",
         webc_keenable_origin_error(P("403"),
                                    "{\"message\":\"the target server returned 403\"}"));
    emit(31, "keenable_origin_error",
         "{\"status\":404,\"body\":\"{\\\"message\\\":\\\"The page does not exist.\\\"}\"}",
         webc_keenable_origin_error(P("404"),
                                    "{\"message\":\"The page does not exist.\"}"));
    emit(32, "keenable_origin_error",
         "{\"status\":429,\"body\":\"{\\\"message\\\":\\\"rate limited\\\"}\"}",
         webc_keenable_origin_error(P("429"),
                                    "{\"message\":\"rate limited\"}"));
    emit(33, "keenable_origin_error",
         "{\"status\":403,\"body\":\"not json\"}",
         webc_keenable_origin_error(P("403"), "not json"));

    /* with_origin_status */
    emit(34, "with_origin_status",
         "{\"entry\":{\"url\":\"u\",\"title\":\"\"},\"status\":403,\"reason\":null}",
         webc_with_origin_status(P("{\"url\":\"u\",\"title\":\"\"}"), P("403"), NULL));
    emit(35, "with_origin_status",
         "{\"entry\":{\"url\":\"u\",\"error\":\"keep\"},\"status\":200,\"reason\":null}",
         webc_with_origin_status(P("{\"url\":\"u\",\"error\":\"keep\"}"),
                                 P("200"), NULL));

    /* vendor_error_entry */
    emit(36, "vendor_error_entry",
         "{\"url\":\"u\",\"status\":403,\"reason\":null,\"body\":\"body\"}",
         webc_vendor_error_entry("u", P("403"), NULL, "body"));
    emit(37, "vendor_error_entry",
         "{\"url\":\"u\",\"status\":null,\"reason\":\"http_error\",\"body\":\"b\"}",
         webc_vendor_error_entry("u", P("null"), "http_error", "b"));
    emit(38, "vendor_error_entry",
         "{\"url\":\"u\",\"status\":null,\"reason\":null,\"body\":null}",
         webc_vendor_error_entry("u", P("null"), NULL, NULL));

    /* titled_rows */
    emit(39, "titled_rows",
         "{\"rows\":[{\"title\":\"a\",\"url\":\"u1\",\"snippet\":\"s1\"},"
         "{\"title\":\"b\",\"url\":\"u2\",\"snippet\":\"s2\"}],"
         "\"description_key\":\"snippet\"}",
         webc_titled_rows(P("[{\"title\":\"a\",\"url\":\"u1\",\"snippet\":\"s1\"},"
                            "{\"title\":\"b\",\"url\":\"u2\",\"snippet\":\"s2\"}]"),
                          "snippet"));
    emit(40, "titled_rows",
         "{\"rows\":[{\"url\":\"u1\"}],\"description_key\":\"snippet\"}",
         webc_titled_rows(P("[{\"url\":\"u1\"}]"), "snippet"));
    emit(41, "titled_rows",
         "{\"rows\":[{\"title\":null,\"url\":\"u1\",\"snippet\":5}],"
         "\"description_key\":\"snippet\"}",
         webc_titled_rows(P("[{\"title\":null,\"url\":\"u1\",\"snippet\":5}]"),
                          "snippet"));

    /* setup_schema */
    emit(42, "setup_schema",
         "{\"name\":\"n\",\"badge\":\"b\",\"tag\":\"t\",\"key_env\":\"K\","
         "\"prompt\":\"p\",\"url\":\"u\",\"extra\":{\"web_tier\":\"free\"}}",
         webc_setup_schema("n", "b", "t", "K", "p", "u",
                           P("{\"web_tier\":\"free\"}")));
    emit(43, "setup_schema",
         "{\"name\":\"n\",\"badge\":\"b\",\"tag\":\"t\",\"key_env\":\"\","
         "\"prompt\":\"\",\"url\":\"\",\"extra\":{}}",
         webc_setup_schema("n", "b", "t", "", "", "", P("{}")));

    /* keyless_variant_schema */
    emit(44, "keyless_variant_schema",
         "{\"display\":\"Exa\",\"key_env\":\"EXA_KEY\",\"key_url\":\"https://x\","
         "\"free_tag\":\"free\",\"paid_tag\":\"paid\"}",
         webc_keyless_variant_schema("Exa", "EXA_KEY", "https://x",
                                     "free", "paid"));
    return 0;
}
