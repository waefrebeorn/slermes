#ifndef NOC_CFGTURN_H
#define NOC_CFGTURN_H
#include <stdint.h>
/* Python sys.maxsize on a 64-bit build; the "unlimited" sentinel. */
#define CFGTURN_UNLIMITED INT64_MAX
const char *cfgturn_limit_label(int64_t limit);
#endif
