#ifndef NOC_W11_INIT_B19B81_H
#define NOC_W11_INIT_B19B81_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_init_b19b81_normalize_arch @ hermes_platform/host/facts.py:normalize_arch */
struct json_t *w11_init_b19b81_normalize_arch(const char *raw);

/* PoP: w11_init_b19b81_windows_native_arch @ hermes_platform/host/facts.py:windows_native_arch
 * wow64_native is passed as a decimal string (empty/"none" -> None). */
struct json_t *w11_init_b19b81_windows_native_arch(const char *wow64_native, const char *machine);

/* PoP: w11_init_b19b81_classify_gpu_vendors @ hermes_platform/host/facts.py:classify_gpu_vendors */
struct json_t *w11_init_b19b81_classify_gpu_vendors(const struct json_t *vendor_ids);

/* PoP: w11_init_b19b81_parse_cpuinfo @ hermes_platform/host/facts.py:parse_cpuinfo */
struct json_t *w11_init_b19b81_parse_cpuinfo(const char *text, const char *device_tree_model);

/* PoP: w11_init_b19b81_parse_cpuinfo_vendor @ hermes_platform/host/facts.py:parse_cpuinfo_vendor */
struct json_t *w11_init_b19b81_parse_cpuinfo_vendor(const char *text);

/* PoP: w11_init_b19b81_parse_meminfo_total @ hermes_platform/host/facts.py:parse_meminfo_total */
struct json_t *w11_init_b19b81_parse_meminfo_total(const char *text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_INIT_B19B81_H */
