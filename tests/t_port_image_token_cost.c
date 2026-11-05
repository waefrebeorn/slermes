/* t_port_image_token_cost.c — Oracle harness for the image-part counting core. */
#include "port_image_token_cost.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    static const char *const msgs[] = {
        "{}",
        "{\"content\":null}",
        "{\"content\":\"plain text\"}",
        "{\"content\":[{\"type\":\"text\",\"text\":\"hi\"}]}",
        "{\"content\":[{\"type\":\"image\"}]}",
        "{\"content\":[{\"type\":\"image_url\",\"url\":\"x\"},{\"type\":\"image\"}]}",
        "{\"content\":[{\"type\":\"input_image\",\"data\":\"a\"},{\"type\":\"tool_use\"},{\"type\":\"text\"}]}",
        "{\"content\":[{\"type\":\"image\"},{\"type\":\"image\"},{\"type\":\"image\"}]}",
        "{\"content\":[7,\"str\",null,{\"type\":\"image\"}]}",
        "{\"content\":[{\"type\":\"video\"}]}",
        "{\"_anthropic_content_blocks\":[{\"type\":\"image\"}]}",
        "{\"_anthropic_content_blocks\":[{\"type\":\"text\"}]}",
        "{\"_anthropic_content_blocks\":\"nope\"}",
        "{\"content\":{\"_multimodal\":true,\"content\":[{\"type\":\"image\"},{\"type\":\"image_url\"}]}}",
        "{\"content\":{\"_multimodal\":false,\"content\":[{\"type\":\"image\"}]}}",
        "{\"content\":{\"_multimodal\":1,\"content\":[{\"type\":\"image\"}]}}",
        "{\"output\":[{\"type\":\"image\"},{\"type\":\"function_call_output\"}]}",
        "{\"content\":[{\"type\":\"image\"}],\"output\":[{\"type\":\"image\"}],\"_anthropic_content_blocks\":[{\"type\":\"image\"}]}",
    };
    for (size_t i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++) {
        printf("{\"family\":\"msg\",\"case\":\"m_%zu\",\"count\":%d,\"cost100\":%d}\n",
               i, pic_count_image_tokens(msgs[i], 1),
               pic_count_image_tokens(msgs[i], 100));
    }

    {
        static const char *const arrays[] = {
            "[]",
            "[{\"content\":[{\"type\":\"image\"}]}]",
            "[{\"content\":[{\"type\":\"image\"}]},{\"content\":[{\"type\":\"image\"},{\"type\":\"image\"}]}]",
            "[\"not a dict\",7,{\"content\":[{\"type\":\"image_url\"}]}]",
            "null",
        };
        for (size_t i = 0; i < sizeof(arrays) / sizeof(arrays[0]); i++)
            printf("{\"family\":\"msgs\",\"case\":\"a_%zu\",\"total\":%d}\n", i,
                   pic_count_images(arrays[i]));
    }
    return 0;
}
