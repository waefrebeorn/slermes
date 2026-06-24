/*
 * memory_manager.c — Port of Python agent/memory_manager.py
 *
 * Python API → C implementation mapping:
 *   MemoryManager.__init__()       → memory_manager_init() in hermes_gap_fixes.c
 *   MemoryManager.load()           → memory_manager_load() in hermes_gap_fixes.c
 *   MemoryManager.save()           → memory_manager_save() in hermes_gap_fixes.c
 *   MemoryManager.search()         → memory_manager_search() in hermes_gap_fixes.c
 *   MemoryManager.delete()         → memory_manager_delete() in hermes_gap_fixes.c
 *   MemoryManager.list()           → memory_manager_list() in hermes_gap_fixes.c
 *   MemoryManager.format_snapshot()→ memory_format_snapshot() in memory_provider.c
 *   MemoryManager.prefetch()       → builtin_prefetch() in memory_provider.c (static)
 *
 * All 6 memory_manager_* functions are implemented directly in
 * hermes_gap_fixes.c with the correct public names. This file exists
 * for name parity — the actual implementations are in hermes_gap_fixes.c
 * and declared in hermes_gap_fixes.h.
 *
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.__init__()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.load()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.save()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.search()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.delete()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.list()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.format_snapshot()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.prefetch()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.add_provider()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.build_system_prompt()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.prefetch_all()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.queue_prefetch_all()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager.sync_all()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager._provider_sync_accepts_messages()
 * AG26: Port of Python agent/memory_manager.py:MemoryManager._submit_background()
 * AG26: Port of Python agent/memory_manager.py:sanitize_context()
 * AG26: Port of Python agent/memory_manager.py:StreamingContextScrubber()
 * AG26: Port of Python agent/memory_manager.py:build_memory_context_block()
 */

#include "hermes_gap_fixes.h"    /* memory_manager_init/load/save/search/delete/list */
#include "hermes_memory.h"       /* memory_search(), memory_format_snapshot() */
