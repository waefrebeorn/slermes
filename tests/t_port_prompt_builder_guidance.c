/*
 * t_port_prompt_builder_guidance.c — Oracle harness for
 * agent/prompt_builder.py:computer_use_guidance
 * (ported to src/agent/prompt_builder_guidance.c).
 *
 * Calls the port for each platform_name case and emits a single-line JSON
 * record {case, len, digest} where digest is an FNV-1a over the returned
 * string. The Python oracle replays LIVE computer_use_guidance, computes the
 * same digest, and compares.
 *
 * The real module is compiled in directly (#include of the .c) so the harness
 * uses genuine sources regardless of how the oracle runner sets up -I paths;
 * the runner's --allow-multiple-definition tolerates the duplicate symbol
 * from the linked prompt_builder_guidance.o.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/agent/prompt_builder_guidance.c"

extern prompt_builder_guidance_t *prompt_builder_guidance_init(void);
extern void prompt_builder_guidance_free(prompt_builder_guidance_t *ctx);
extern char *prompt_builder_computer_use_guidance(const prompt_builder_guidance_t *ctx,
                                                  const char *platform_name);

/* FNV-1a 64-bit digest to keep the emitted line single-line & newline-free */
static unsigned long long digest(const char *s) {
    unsigned long long h = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= *p;
        h *= 1099511628211ULL;
    }
    return h;
}

static prompt_builder_guidance_t *G_CTX;

static void emit(const char *name /* JSON token: "darwin" or null */,
                 const char *platform) {
    char *out = prompt_builder_computer_use_guidance(G_CTX, platform);
    unsigned long long d = out ? digest(out) : 0;
    size_t len = out ? strlen(out) : 0;
    printf("{\"case\":%s,\"len\":%zu,\"digest\":%llu}\n", name, len, d);
    free(out);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    G_CTX = prompt_builder_guidance_init();
    if (!G_CTX) return 2;
    emit("\"darwin\"", "darwin");
    emit("\"win32\"", "win32");
    emit("\"linux\"", "linux");
    emit("\"cygwin\"", "cygwin");   /* unknown -> treated as non-mac/non-win */
    emit("null", NULL);             /* host default (Linux here) */
    prompt_builder_guidance_free(G_CTX);
    return 0;
}
