/*
 * noc_w25_darwinpci_ff026b.h - C surface for upstream
 * hermes_platform/host/darwin_pci.py (gate w25_darwinpci_ff026b).
 *
 * The value half of the IOKit display-controller walk.  The registry, the
 * CFData handles and the ctypes bindings are the seam: a property is handed in
 * as the bytes it carried (hex), and what is ported is what those bytes decide
 * -- the little-endian integer, which devices are display controllers, and the
 * vendor id spelling.
 */
#ifndef NOC_W25_DARWINPCI_FF026B_H
#define NOC_W25_DARWINPCI_FF026B_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* hex bytes of a CFData -> the little-endian integer, or null (missing/empty). */
json_t *w25_darwinpci_ff026b_data_int(const json_t *hex);

/* a vendor-id integer -> "%04x" of its low 16 bits, "" when null. */
json_t *w25_darwinpci_ff026b_format_vendor(const json_t *value);

/* devices: [{"class_code": hex|null, "vendor_id": hex|null}, ...] and the seam
 * flag `readable` (false: IOKit could not be read) -> the vendor id list, or
 * null.  At most _MAX_DEVICES entries are inspected. */
json_t *w25_darwinpci_ff026b_display_vendor_ids(const json_t *devices, const json_t *readable);

/* The registry constants as an object. */
json_t *w25_darwinpci_ff026b_constants(void);

#ifdef __cplusplus
}
#endif
#endif
