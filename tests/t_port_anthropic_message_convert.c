/*
 * t_port_anthropic_message_convert.c — Oracle harness for the block-shaping
 * helpers of agent/anthropic_message_convert.py.
 */
#include "port_anthropic_message_convert.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit(const char *family, const char *case_name, json_t *node)
{
    printf("{\"family\":\"%s\",\"case\":\"%s\",\"out\":", family,
           case_name ? case_name : "");
    if (!node) {
        fputs("null", stdout);
    } else {
        char *s = json_serialize(node);
        fputs(s ? s : "null", stdout);
        free(s);
        json_free(node);
    }
    printf("}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* carry_cache_control */
    emit("carry", "with_marker",
         amc_carry_cache_control("{\"type\":\"text\",\"text\":\"x\"}",
                                 "{\"type\":\"text\",\"cache_control\":{\"type\":\"ephemeral\"}}", 1));
    emit("carry", "no_marker",
         amc_carry_cache_control("{\"type\":\"text\"}", "{\"type\":\"text\"}", 0));
    emit("carry", "non_dict_marker",
         amc_carry_cache_control("{\"type\":\"text\"}",
                                 "{\"type\":\"text\",\"cache_control\":\"nope\"}", 0));
    emit("carry", "no_block", amc_carry_cache_control("{\"a\":1}", NULL, 0));

    /* split_blank_text_blocks */
    static const char *const block_sets[] = {
        "[]",
        "[{\"type\":\"text\",\"text\":\"hi\"},{\"type\":\"text\",\"text\":\"  \"},{\"type\":\"text\",\"text\":\"there\"}]",
        "[{\"type\":\"text\",\"text\":\"\"}]",
        "[{\"type\":\"text\",\"text\":null,\"cache_control\":{\"type\":\"ephemeral\"}}]",
        "[{\"type\":\"text\",\"text\":\"a\",\"cache_control\":{\"k\":1}},{\"type\":\"text\",\"text\":\" \"},{\"type\":\"text\",\"text\":\"\",\"cache_control\":{\"k\":2}}]",
        "[{\"type\":\"thinking\",\"thinking\":\"\"}]",
    };
    for (size_t i = 0; i < sizeof(block_sets) / sizeof(block_sets[0]); i++) {
        char name[32];
        snprintf(name, sizeof(name), "split_%zu", i);
        emit("split", name, amc_split_blank_text_blocks(block_sets[i]));
    }

    /* safe_text */
    {
        static const char *const texts[] = { NULL, "", "   ", "\n\t", "hello",
                                             " padded ", "(empty)" };
        for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
            char *out = amc_safe_text(texts[i]);
            printf("{\"family\":\"safe_text\",\"case\":\"st_%zu\",\"out\":", i);
            if (out) {
                fputc('"', stdout);
                fputs(out, stdout);
                fputc('"', stdout);
            } else {
                fputs("null", stdout);
            }
            printf("}\n");
            free(out);
        }
    }

    /* image blocks */
    static const char *const urls[] = {
        "https://example.test/a.png",
        "",
        "data:image/png;base64,AAAA",
        "data:image/jpg;base64,BBBB",
        "data:image/jpeg;base64,CCCC",
        "data:image/gif;base64,DDDD",
        "data:image/webp;base64,EEEE",
        "data:image/bmp;base64,FFFF",
        "data:image/tiff;base64,GGGG",
        "data:application/octet-stream;base64,HHHH",
        "  data:image/png;base64,IIII  ",
        "data:IMAGE/PNG;base64,JJJJ",
        "data:image/PNG;base64,KKKK",
    };
    for (size_t i = 0; i < sizeof(urls) / sizeof(urls[0]); i++) {
        char name[32];
        snprintf(name, sizeof(name), "img_%zu", i);
        emit("image", name, amc_image_block_from_openai_url(urls[i]));
    }

    /* replay whitelists */
    static const char *const replay_blocks[] = {
        "{\"type\":\"text\",\"text\":\"hi\",\"parsed_output\":{\"x\":1},\"citations\":null}",
        "{\"type\":\"text\",\"text\":\"  \"}",
        "{\"type\":\"text\",\"text\":\"hi\",\"citations\":[{\"a\":1}],\"cache_control\":{\"type\":\"ephemeral\"}}",
        "{\"type\":\"thinking\",\"thinking\":\"hmm\",\"signature\":\"sig\",\"caller\":\"x\"}",
        "{\"type\":\"thinking\",\"thinking\":\"hmm\",\"signature\":\"\"}",
        "{\"type\":\"redacted_thinking\",\"data\":\"abc\"}",
        "{\"type\":\"redacted_thinking\",\"data\":\"\"}",
        "{\"type\":\"redacted_thinking\"}",
        "{\"type\":\"tool_use\",\"id\":\"t1\",\"name\":\"read\",\"input\":{\"p\":1},\"caller\":{\"y\":2}}",
        "{\"type\":\"tool_use\"}",
        "{\"type\":\"image\",\"source\":{\"type\":\"base64\",\"media_type\":\"image/png\",\"data\":\"AA\"}}",
        "{\"type\":\"image\"}",
        "{\"type\":\"unknown_block\",\"x\":1}",
        "\"just a string\"",
    };
    for (size_t i = 0; i < sizeof(replay_blocks) / sizeof(replay_blocks[0]); i++) {
        char name[32];
        snprintf(name, sizeof(name), "rp_%zu", i);
        emit("replay", name, amc_sanitize_replay_block(replay_blocks[i]));
    }
    return 0;
}
