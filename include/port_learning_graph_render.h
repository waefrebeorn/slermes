/*
 * port_learning_graph_render.h — public API for agent/learning_graph_render.py.
 *
 * Two layers:
 *   - Pure color/date/score helpers (declared here, defined in
 *     port_learning_graph_render_helpers.c).
 *   - The rendering + orchestration layer (18 functions) defined in
 *     port_learning_graph_render.c: compute_recency, _date_at, the _ChartBucket
 *     total, _build_chart_buckets, _bucket_label_node, _bucket_nodes,
 *     _bucket_rows, _category_counts, category_color_map, category_legend,
 *     _bucket_category, _trajectory_row, render_graph, build_legend,
 *     axis_labels, _peak_day, _merge_runs, render_frames.
 *
 * All functions are JSON-in / JSON-out; returned strings are malloc'd
 * (caller frees). Pure, stdlib-only on the C side (no I/O), mirroring the
 * Python module.
 */

#ifndef PORT_LEARNING_GRAPH_RENDER_H
#define PORT_LEARNING_GRAPH_RENDER_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- foundational helpers (port_learning_graph_render_helpers.c) ---- */
double learning_graph_render_clamp(double v, double lo, double hi);
double learning_graph_render_smoothstep(double p);
double learning_graph_render_recency_ink(double rec);
char  *learning_graph_render_rgb_to_hex(int r, int g, int b);
char  *learning_graph_render_hsl_to_rgb(double h, double s, double l);
char  *learning_graph_render_hex_to_rgb(const char *s);
char  *learning_graph_render_mix_rgb(int ar, int ag, int ab, int br, int bg, int bb, double t);
char  *learning_graph_render_complementary_ink(int r, int g, int b);
char  *learning_graph_render_derive_palette(const char *primary_hex, int dark);
char  *learning_graph_render_node_label(const char *label, const char *id);
char  *learning_graph_render_format_date(double ts);
int    learning_graph_render_to_ts(const void *value, double *out_ts);
char  *learning_graph_render_period_key(double ts, const char *granularity);
char  *learning_graph_render_period_label(double ts, const char *granularity);
double learning_graph_render_node_score(const json_t *node, double rec);
char  *learning_graph_render_node_meta(const json_t *node);

/* ---- rendering / orchestration layer (port_learning_graph_render.c) ---- */

char *learning_graph_render_compute_recency(const char *nodes_json);
char *learning_graph_render_date_at(const char *rec_json, double reveal);
int   learning_graph_render_bucket_total(const char *bucket_json);
char *learning_graph_render_build_chart_buckets(const char *nodes_json,
                                                 const char *rec_json,
                                                 int max_rows);
char *learning_graph_render_bucket_label_node(const char *bucket_json);
char *learning_graph_render_bucket_nodes(const char *bucket_json,
                                         const char *memory_lookup_json);
char *learning_graph_render_bucket_rows(const char *buckets_json,
                                        const char *payload_json);
char *learning_graph_render_category_counts(const char *payload_json);
char *learning_graph_render_category_color_map(const char *payload_json);
char *learning_graph_render_category_legend(const char *payload_json, int limit);
char *learning_graph_render_bucket_category(const char *bucket_json);
char *learning_graph_render_trajectory_row(const char *buckets_json,
                                           int width, double reveal);
char *learning_graph_render_graph(const char *payload_json, int cols, int rows,
                                  double reveal);
char *learning_graph_render_build_legend(const char *payload_json);
char *learning_graph_render_axis_labels(const char *payload_json);
char *learning_graph_render_peak_day(const char *payload_json);
char *learning_graph_render_merge_runs(const char *runs_json);
char *learning_graph_render_frames(const char *payload_json, int cols, int rows,
                                   int frames);

#ifdef __cplusplus
}
#endif

#endif /* PORT_LEARNING_GRAPH_RENDER_H */
