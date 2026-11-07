/*
 * t_port_anthropic_message_convert.c — Oracle harness for agent/anthropic_message_convert.py
 * block-shaping helpers (src/cli/port_anthropic_message_convert.c). Emits inputs + C output
 * as JSONL so an oracle replays the LIVE Python on identical values.
 * Families:
 *   carry:      {family, case, out_json, block_json, copy, out}  _carry_cache_control
 *   split:      {family, case, blocks, out}                      _split_blank_text_blocks
 *               out = [kept, relocated|null, dropped_indexes]
 *   safe_text:  {family, case, text|null, out}                   _safe_text
 *   image:      {family, case, url, out}                         _image_block_from_openai_url
 *   replay:     {family, case, block, out}                       _sanitize_replay_block
 */
#include "port_anthropic_message_convert.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

/* emit a node result: out:null or out:<json> */
static void emit_node(const char *family, const char *case_name,
                      const char *inputs, json_t *node)
{
    printf("{\"family\":\"%s\",\"case\":\"%s\",%s\"out\":", family, case_name, inputs);
    if (node) { char *s = json_serialize(node); fputs(s ? s : "null", stdout); free(s); }
    else fputs("null", stdout);
    printf("}\n");
    if (node) json_free(node);
}

/* emit a plain-string result: out:null or out:"..." */
static void emit_str(const char *family, const char *case_name,
                     const char *inputs, const char *v)
{
    printf("{\"family\":\"%s\",\"case\":\"%s\",%s\"out\":", family, case_name, inputs);
    jstr(v);
    printf("}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* carry */
    {
        struct { const char *outj, *blk; int copy; const char *n; } cs[] = {
            { "{\"type\":\"text\",\"text\":\"x\"}", "{\"type\":\"text\",\"cache_control\":{\"type\":\"ephemeral\"}}", 1, "with_marker" },
            { "{\"type\":\"text\"}", "{\"type\":\"text\"}", 0, "no_marker" },
            { "{\"type\":\"text\"}", "{\"type\":\"text\",\"cache_control\":\"nope\"}", 0, "non_dict_marker" },
            { "{\"a\":1}", NULL, 0, "no_block" },
        };
        for (int i = 0; i < 4; i++) {
            printf("{\"family\":\"carry\",\"case\":\"%s\",\"out_json\":%s,\"block_json\":%s,\"copy\":%d,\"out\":",
                   cs[i].n, cs[i].outj, cs[i].blk ? cs[i].blk : "null", cs[i].copy);
            json_t *r = amc_carry_cache_control(cs[i].outj, cs[i].blk, cs[i].copy);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); json_free(r); }
            else fputs("null", stdout);
            printf("}\n");
        }
    }

    /* split */
    {
        static const char *const sets[] = {
            "[]",
            "[{\"type\":\"text\",\"text\":\"hi\"},{\"type\":\"text\",\"text\":\"  \"},{\"type\":\"text\",\"text\":\"there\"}]",
            "[{\"type\":\"text\",\"text\":\"\"}]",
            "[{\"type\":\"text\",\"text\":null,\"cache_control\":{\"type\":\"ephemeral\"}}]",
            "[{\"type\":\"text\",\"text\":\"a\",\"cache_control\":{\"k\":1}},{\"type\":\"text\",\"text\":\" \"},{\"type\":\"text\",\"text\":\"\",\"cache_control\":{\"k\":2}}]",
            "[{\"type\":\"thinking\",\"thinking\":\"\"}]",
        };
        for (int i = 0; i < 6; i++) {
            char n[32];
            snprintf(n, sizeof(n), "split_%d", i);
            printf("{\"family\":\"split\",\"case\":\"%s\",\"blocks\":%s,\"out\":", n, sets[i]);
            json_t *r = amc_split_blank_text_blocks(sets[i]);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); json_free(r); }
            else fputs("null", stdout);
            printf("}\n");
        }
    }

    /* safe_text */
    {
        const char *texts[] = { NULL, "", "   ", "\n\t", "hello", " padded ", "(empty)" };
        for (int i = 0; i < 7; i++) {
            char n[32];
            snprintf(n, sizeof(n), "st_%d", i);
            char *out = amc_safe_text(texts[i]);
            printf("{\"family\":\"safe_text\",\"case\":\"%s\",\"text\":", n);
            jstr(texts[i]);
            printf(",\"out\":");
            jstr(out);
            printf("}\n");
            free(out);
        }
    }

    /* image */
    {
        static const char *const urls[] = {
            "https://example.test/a.png", "", "data:image/png;base64,AAAA",
            "data:image/jpg;base64,BBBB", "data:image/jpeg;base64,CCCC",
            "data:image/gif;base64,DDDD", "data:image/webp;base64,EEEE",
            "data:image/bmp;base64,FFFF", "data:image/tiff;base64,GGGG",
            "data:application/octet-stream;base64,HHHH", "  data:image/png;base64,IIII  ",
            "data:IMAGE/PNG;base64,JJJJ", "data:image/PNG;base64,KKKK",
        };
        for (int i = 0; i < 13; i++) {
            char n[32];
            snprintf(n, sizeof(n), "img_%d", i);
            printf("{\"family\":\"image\",\"case\":\"%s\",\"url\":", n);
            jstr(urls[i]);
            printf(",\"out\":");
            json_t *r = amc_image_block_from_openai_url(urls[i]);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); json_free(r); }
            else fputs("null", stdout);
            printf("}\n");
        }
    }

    /* replay */
    {
        static const char *const blocks[] = {
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
        for (int i = 0; i < 14; i++) {
            char n[32];
            snprintf(n, sizeof(n), "rp_%d", i);
            printf("{\"family\":\"replay\",\"case\":\"%s\",\"block\":", n);
            printf("%s", blocks[i]);
            printf(",\"out\":");
            json_t *r = amc_sanitize_replay_block(blocks[i]);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); json_free(r); }
            else fputs("null", stdout);
            printf("}\n");
        }
    }
    return 0;
}
