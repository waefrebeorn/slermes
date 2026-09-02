/* shim: cc_name() — mirrors Python agent/context_compressor.py:ContextCompressor.name (static property returning "compressor") */
#include <stddef.h>
#include <stdbool.h>

char *cc_name(void) {
    return "compressor";
}
