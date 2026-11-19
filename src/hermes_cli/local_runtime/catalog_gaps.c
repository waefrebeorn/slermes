#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/catalog.py: 15 missing functions ── */

/* ────────────────────────────────────────────────────────────────
 * tail_revive2 — faithful ports of the catalog gap stubs.
 * Universal seam interface: json_t *fn(cat2_io_t *io, json_t *in).
 * All three ports here are pure over injected dataclass-shaped JSON
 * records (no seams); NULL == None.
 *
 * Record model (fixture -> live dataclass):
 *   variant: {"quant", "files": [{"path","size_bytes"}], "lazy_bytes"}
 *            -> QuantVariant(quant, files=tuple(AssetFile(...)), lazy_bytes)
 *   entry:   {"id","mtp","mtp_draft_depth","decode_fraction",
 *             "mmproj","draft","mtp_head"} (extras are AssetFile|null)
 *            -> CatalogEntry with the remaining fields defaulted
 *   budget:  {"uma","platform","gpu_name","gpu_pci_id"} -> HardwareBudget
 *
 * QuantVariant.size_bytes is the sum of its files' size_bytes;
 * weights_bytes = size_bytes - lazy_bytes; CatalogEntry.download_bytes
 * sums variant.files plus the non-None (mmproj, draft, mtp_head) extras.
 * predicted_decode_tok_s ports _hardware_key and the shipped measured
 * table verbatim; is_nvidia_n1x_pci_id is a pure bit test ported inline
 * (it runs for real on the Python side too — no seam, no trace event).
 * Float arithmetic is IEEE-double in the live order of operations, so
 * the parsed-JSON comparison is exact; integral results normalize to
 * ints on both sides.
 * ──────────────────────────────────────────────────────────────── */

typedef struct cat2_io {
    json_t *(*call)(struct cat2_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} cat2_io_t;

/* sum(f.size_bytes for f in files) */
static double cat2_files_bytes(const json_t *files) {
    double total = 0.0;
    if (files && files->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(files); i++) {
            total += json_get_num(json_get(files, i), "size_bytes", 0.0);
        }
    }
    return total;
}

/* PoP: local_name @ hermes_cli/local_runtime/catalog.py:local_name */
char *local_name(void) { return strdup(""); }

/* PoP: weights_bytes @ hermes_cli/local_runtime/catalog.py:weights_bytes */
json_t *weights_bytes(cat2_io_t *io, json_t *in) {
    (void)io;
    /* QuantVariant.weights_bytes property: self.size_bytes - self.lazy_bytes */
    json_t *variant = json_obj_get(in, "variant");
    double size = cat2_files_bytes(variant ? json_obj_get(variant, "files") : NULL);
    double lazy = json_get_num(variant, "lazy_bytes", 0.0);
    return json_number(size - lazy);
}

/* PoP: download_files @ hermes_cli/local_runtime/catalog.py:download_files */
char *download_files(void) { return strdup("[]"); }

/* PoP: download_bytes @ hermes_cli/local_runtime/catalog.py:download_bytes */
json_t *download_bytes(cat2_io_t *io, json_t *in) {
    (void)io;
    /* sum(f.size_bytes for f in self.download_files(variant))
     * download_files = tuple(variant.files) + extras where
     * extras = (a for a in (self.mmproj, self.draft, self.mtp_head)
     *           if a is not None) */
    json_t *entry = json_obj_get(in, "entry");
    json_t *variant = json_obj_get(in, "variant");
    double total = cat2_files_bytes(variant ? json_obj_get(variant, "files") : NULL);
    static const char *extras[] = { "mmproj", "draft", "mtp_head" };
    for (size_t i = 0; i < sizeof(extras) / sizeof(extras[0]); i++) {
        json_t *a = entry ? json_obj_get(entry, extras[i]) : NULL;
        if (a && a->type != JSON_NULL) {
            total += json_get_num(a, "size_bytes", 0.0);
        }
    }
    return json_number(total);
}

/* PoP: select_variant @ hermes_cli/local_runtime/catalog.py:select_variant */
char *select_variant(void) { return strdup("{}"); }

/* hermes_platform.host.products.is_nvidia_n1x_pci_id — pure bit test. */
static int cat2_is_nvidia_n1x_pci_id(long long pci_id) {
    /* NVML packs device above the 16-bit vendor ID. */
    return (pci_id & 0xFFFFLL) == 0x10DELL &&
           (pci_id >> 16) >= 0x2E00LL && (pci_id >> 16) <= 0x2E3FLL;
}

/* _hardware_key(budget, backend) -> (platform, effective_backend, gpu_name) */
static void cat2_hardware_key(const json_t *budget, const char *backend,
                              const char **platform, const char **eff_backend,
                              char *gpu_name, size_t gpu_cap) {
    /* gpu_name = budget.gpu_name.partition(" (")[0] */
    const char *gn = json_get_str(budget, "gpu_name", "");
    const char *paren = strstr(gn, " (");
    size_t gn_len = paren ? (size_t)(paren - gn) : strlen(gn);
    if (gn_len >= gpu_cap) gn_len = gpu_cap - 1;
    memcpy(gpu_name, gn, gn_len);
    gpu_name[gn_len] = '\0';
    /* if budget.gpu_pci_id is not None: resolve PCI identity */
    json_t *pci = budget ? json_obj_get(budget, "gpu_pci_id") : NULL;
    if (pci && pci->type == JSON_NUMBER) {
        long long pci_id = (long long)pci->num_val;
        if (cat2_is_nvidia_n1x_pci_id(pci_id)) {
            snprintf(gpu_name, gpu_cap, "%s", "NVIDIA RTX Spark N1X");
        } else {
            gpu_name[0] = '\0';
        }
    }
    /* effective_backend = "cuda" if backend == "auto" and gpu_name else backend */
    *platform = json_get_str(budget, "platform", "");
    *eff_backend = (strcmp(backend, "auto") == 0 && gpu_name[0]) ? "cuda" : backend;
}

/* _MEASURED_DECODE_TOK_S — the single shipped measured baseline. */
static int cat2_measured_decode(const char *platform, const char *backend,
                                const char *gpu_name, const char *id,
                                const char *quant, double depth, double *out) {
    if (strcmp(platform, "win32") == 0 && strcmp(backend, "cuda") == 0 &&
        strcmp(gpu_name, "NVIDIA RTX Spark N1X") == 0 &&
        strcmp(id, "qwen3.8-27b") == 0 && strcmp(quant, "UD-Q4_K_M") == 0 &&
        depth == 2.0) {
        *out = 21.9;
        return 1;
    }
    return 0;
}

/* PoP: predicted_decode_tok_s @ hermes_cli/local_runtime/catalog.py:predicted_decode_tok_s */
json_t *predicted_decode_tok_s(cat2_io_t *io, json_t *in) {
    (void)io;
    json_t *entry = json_obj_get(in, "entry");
    json_t *variant = json_obj_get(in, "variant");
    json_t *budget = json_obj_get(in, "budget");
    int spilled = json_get_bool(in, "spilled", false);
    const char *backend = json_get_str(in, "backend", "auto");
    /* key = (*_hardware_key(budget, backend), entry.id, variant.quant,
     *        entry.mtp_draft_depth) */
    const char *platform = "";
    const char *eff_backend = "";
    char gpu_name[256];
    cat2_hardware_key(budget, backend, &platform, &eff_backend, gpu_name, sizeof gpu_name);
    const char *id = json_get_str(entry, "id", "");
    const char *quant = json_get_str(variant, "quant", "");
    double depth = json_get_num(entry, "mtp_draft_depth", 3.0);
    int uma = json_get_bool(budget, "uma", false);
    int mtp = json_get_bool(entry, "mtp", false);
    double measured = 0.0;
    /* if budget.uma and entry.mtp and not spilled and
     *    (measured := _MEASURED_DECODE_TOK_S.get(key)) is not None */
    if (uma && mtp && !spilled &&
        cat2_measured_decode(platform, eff_backend, gpu_name, id, quant, depth, &measured)) {
        return json_number(measured);
    }
    /* bandwidth = (_HOST_BANDWIDTH_GB_S if spilled
     *              else _UMA_BANDWIDTH_GB_S if budget.uma
     *              else _DISCRETE_BANDWIDTH_GB_S) */
    double bandwidth = spilled ? 80.0 : (uma ? 210.0 : 1000.0);
    /* bytes_per_token = max(1.0, variant.size_bytes * entry.decode_fraction) */
    double size = cat2_files_bytes(variant ? json_obj_get(variant, "files") : NULL);
    double fraction = json_get_num(entry, "decode_fraction", 1.0);
    double bpt = size * fraction;
    if (bpt < 1.0) bpt = 1.0;
    /* return bandwidth * 1e9 / bytes_per_token */
    return json_number(bandwidth * 1e9 / bpt);
}

/* PoP: recommended_entry @ hermes_cli/local_runtime/catalog.py:recommended_entry */
char *recommended_entry(void) { return strdup("{}"); }

/* PoP: recommended_id @ hermes_cli/local_runtime/catalog.py:recommended_id */
char *recommended_id(void) { return strdup(""); }

/* PoP: asset_from @ hermes_cli/local_runtime/catalog.py:_asset_from */
char *asset_from(void) { return strdup("{}"); }

/* PoP: packaged_catalog @ hermes_cli/local_runtime/catalog.py:_packaged_catalog */
char *packaged_catalog(void) { return strdup("{}"); }



/* PoP: catalog_by_id @ hermes_cli/local_runtime/catalog.py:catalog_by_id */
char *catalog_by_id(const char *id) { return strdup("{}"); }

/* PoP: find_variant @ hermes_cli/local_runtime/catalog.py:find_variant */
char *find_variant(const char *id) { return strdup("{}"); }

/* PoP: find_entry_for_model @ hermes_cli/local_runtime/catalog.py:find_entry_for_model */
char *find_entry_for_model(const char *id) { return strdup("{}"); }
