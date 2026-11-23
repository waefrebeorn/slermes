/*
 * image_eviction_policy.c — C home for upstream Hermes agent/image_eviction_policy.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: agent_image_eviction_policy_pending @ agent/image_eviction_policy.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static long long agent_image_eviction_policy_CONST_OUTBOUND_IMAGE_LIMIT = 20;
static long long agent_image_eviction_policy_CONST_OUTBOUND_IMAGE_HARD_LIMIT = 100;
static long long agent_image_eviction_policy_CONST_IMAGE_EVICTION_BATCH = 8;
static long long agent_image_eviction_policy_CONST_OUTBOUND_IMAGE_FLOOR = 3;
static long long agent_image_eviction_policy_CONST_MANY_IMAGE_DIMENSION_LIMIT = 2000;

void agent_image_eviction_policy_port_pending(void) { (void)0; }
