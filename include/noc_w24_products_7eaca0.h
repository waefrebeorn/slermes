/*
 * noc_w24_products_7eaca0.h - C surface for upstream
 * hermes_platform/host/products.py (gate w24_products_7eaca0).
 *
 * NVIDIA platform recognition from host facts: the N1X PCI-id test (NVML packs
 * the device id above the 16-bit vendor id) and the rtx_spark / dgx_spark /
 * discrete ladder over OS family, native arch, CPU name, GPU class and the GPU
 * name set.  Reading those facts (registry, sysfs, procfs, sysctl) is a seam:
 * they arrive as values here, so no registry or driver library is touched.
 */
#ifndef NOC_W24_PRODUCTS_7EACA0_H
#define NOC_W24_PRODUCTS_7EACA0_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The CPU marker and the driver/CPU markers that pick dgx_spark. */
#define W24_PRODUCTS_7EACA0_DGX_MARKERS 2

/* pci_id: int -> bool.  (pci_id & 0xFFFF) == 0x10DE and
 * 0x2E00 <= pci_id >> 16 <= 0x2E3F.  A non-int is the TypeError seam -> null. */
json_t *w24_products_7eaca0_is_nvidia_n1x_pci_id(const json_t *pci_id);

/* {os_family, native_arch, cpu_model, gpu_class,
 *  nvidia_gpus: [{name, pci_id}, ...]} -> "rtx_spark" | "dgx_spark" |
 * "discrete" | null (no NVIDIA GPU).  A non-string cpu_model is the
 * ``.replace`` AttributeError seam -> null. */
json_t *w24_products_7eaca0_classify_nvidia_platform(const json_t *req);

/* _RTX_SPARK_CPU_MARKER and _DGX_SPARK_MARKERS, as an object. */
json_t *w24_products_7eaca0_constants(void);

#ifdef __cplusplus
}
#endif
#endif
