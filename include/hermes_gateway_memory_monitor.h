/**
 * @file hermes_gateway_memory_monitor.h
 * @brief Memory monitor API (port of Python gateway/memory_monitor.py).
 */
#ifndef HERMES_GATEWAY_MEMORY_MONITOR_H
#define HERMES_GATEWAY_MEMORY_MONITOR_H

#include "hermes_gateway_types.h"

/* ================================================================
 *  Memory Monitor
 * ================================================================ */

/* Get current RSS in megabytes. Returns -1 on failure. */
int get_rss_mb(void);

/* Log current memory usage via stderr with optional prefix.
 * Port of Python log_memory_usage(). */
void log_memory_usage(const char *prefix);

/* Start background memory monitoring thread.
 * interval_seconds: polling interval (default 300). Returns true if started. */
bool start_memory_monitoring(double interval_seconds);

/* Stop background memory monitoring. */
void stop_memory_monitoring(void);

/* Check if memory monitoring is running. */
bool is_memory_monitoring_running(void);

#endif /* HERMES_GATEWAY_MEMORY_MONITOR_H */