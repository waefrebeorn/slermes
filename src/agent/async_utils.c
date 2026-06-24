/*
 * async_utils.c — Port of Python agent/async_utils.py
 *
 * Python API → C implementation mapping:
 *   safe_schedule_threadsafe()  → N/A  (C uses synchronous patterns, not asyncio)
 *   run_with_timeout()          → run_with_timeout() in process_bootstrap.c
 *   is_subprocess_running()     → is_subprocess_running() in process_bootstrap.c
 *   terminate_subprocess()      → terminate_subprocess() in process_bootstrap.c
 *   bootstrap_process_runner()  → bootstrap_process_runner() in process_bootstrap.c
 *
 * All Python async utilities are N/A — C uses synchronous/single-threaded
 * patterns. Process management is ported in process_bootstrap.c.
 */

#include "hermes.h"   /* process_bootstrap types and helpers */
