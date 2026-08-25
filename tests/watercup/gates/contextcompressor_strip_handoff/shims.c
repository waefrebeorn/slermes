/* Link shims: off-path token estimator. */
#include <stddef.h>
size_t estimate_messages_tokens_rough(const void *msgs) { (void)msgs; return 0; }
