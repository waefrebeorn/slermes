/*
 * environment_gaps.c — Consolidated N/A annotations for environment backends.
 *
 * Port of Python tools/environments/ modules (SDK wrappers).
 *
 * Documents the status of all Python tools/environments/ modules.
 * Almost all are third-party SDK wrappers or ABCs — inherently Python-only.
 *
 * See THIRD_PARTY.md §2i for full N/A module catalog with install guide.
 *
 * Already ported to C:
 *   local.py → src/tools/terminal.c (terminal_backend_local_*)
 *   file_sync.py → lib/libfile_sync/file_sync.c (FileSyncManager)
 *   daytona.py → src/tools/daytona.c (stub — SDK wrapper)
 *
 * Python-only (SDK wrapper / ABC — NOT PORTABLE):
 *   base.py → N/A, Python ABC for environment backends (C has libtoolbackend)
 *   docker.py → N/A, Python docker SDK wrapper (1296 lines)
 *   ssh.py → N/A, Python paramiko SDK wrapper (375 lines)
 *   modal.py → N/A, Python modal SDK wrapper (478 lines)
 *   managed_modal.py → N/A, Python modal managed session wrapper (282 lines)
 *   modal_utils.py → N/A, Python modal utility helpers (204 lines)
 *   singularity.py → N/A, Python singularity SDK wrapper (262 lines)
 */

#include "hermes.h"
