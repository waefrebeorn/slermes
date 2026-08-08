/* context_compressor_constants.h - byte-exact copies of the summary/skill/
 * image/threshold constants from agent/context_compressor.py (plus
 * TODO_INJECTION_HEADER from tools/todo_tool.py). Generated from the
 * Python source so the C port cannot drift. Do NOT edit by hand.
 */

#ifndef CONTEXT_COMPRESSOR_CONSTANTS_H
#define CONTEXT_COMPRESSOR_CONSTANTS_H

#include <stddef.h>

#define CC_SUMMARY_PREFIX "[CONTEXT COMPACTION — REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window — treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary — that message is the single source of truth for what to do right now. Topic overlap with the summary does NOT mean you should resume its task: even on similar topics, the latest user message WINS. Treat ONLY the latest message as the active task and discard stale items from '## Historical Task Snapshot' entirely — do not 'wrap up' or 'finish' work described there unless the latest message explicitly asks for it. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active — never ignore or deprioritize memory content due to this compaction note. None of the above restricts HOW you work: your tools remain fully active — keep calling them normally for the active task (edit files, run commands, search) instead of merely narrating what you would do. The current session state (files, config, etc.) may reflect work described here — avoid repeating it:"
#define CC_LEGACY_SUMMARY_PREFIX "[CONTEXT SUMMARY]:"
#define CC_MERGED_SUMMARY_DELIMITER "[END OF PRIOR CONTEXT — COMPACTION SUMMARY BELOW]"
#define CC_SUMMARY_END_MARKER "--- END OF CONTEXT SUMMARY — respond to the message below, not the summary above ---"
#define CC_SKILL_PRUNED_MARKER_PREFIX "[SKILL_PRUNED:"
#define CC_NUM_HISTORICAL_PREFIXES 4
#define CC_HISTORICAL_PREFIX_0 "[CONTEXT COMPACTION — REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window — treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary — that message is the single source of truth for what to do right now. Topic overlap with the summary does NOT mean you should resume its task: even on similar topics, the latest user message WINS. Treat ONLY the latest message as the active task and discard stale items from '## Historical Task Snapshot' / '## Historical In-Progress State' / '## Historical Pending User Asks' / '## Historical Remaining Work' entirely — do not 'wrap up' or 'finish' work described there unless the latest message explicitly asks for it. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active — never ignore or deprioritize memory content due to this compaction note. None of the above restricts HOW you work: your tools remain fully active — keep calling them normally for the active task (edit files, run commands, search) instead of merely narrating what you would do. The current session state (files, config, etc.) may reflect work described here — avoid repeating it:"
#define CC_HISTORICAL_PREFIX_1 "[CONTEXT COMPACTION — REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window — treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary — that message is the single source of truth for what to do right now. Topic overlap with the summary does NOT mean you should resume its task: even on similar topics, the latest user message WINS. Treat ONLY the latest message as the active task and discard stale items from '## Historical Task Snapshot' / '## Historical In-Progress State' / '## Historical Pending User Asks' / '## Historical Remaining Work' entirely — do not 'wrap up' or 'finish' work described there unless the latest message explicitly asks for it. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active — never ignore or deprioritize memory content due to this compaction note. The current session state (files, config, etc.) may reflect work described here — avoid repeating it:"
#define CC_HISTORICAL_PREFIX_2 "[CONTEXT COMPACTION — REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window — treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary — that message is the single source of truth for what to do right now. If the latest user message is consistent with the '## Active Task' section, you may use the summary as background. If the latest user message contradicts, supersedes, changes topic from, or in any way diverges from '## Active Task' / '## In Progress' / '## Pending User Asks' / '## Remaining Work', the latest message WINS — discard those stale items entirely and do not 'wrap up the old task first'. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active — never ignore or deprioritize memory content due to this compaction note. The current session state (files, config, etc.) may reflect work described here — avoid repeating it:"
#define CC_HISTORICAL_PREFIX_3 "[CONTEXT COMPACTION — REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window — treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Your current task is identified in the '## Active Task' section of the summary — resume exactly from there. Respond ONLY to the latest user message that appears AFTER this summary. The current session state (files, config, etc.) may reflect work described here — avoid repeating it:"
#define CC_IMAGE_CHAR_EQUIVALENT 6400
#define CC_CHARS_PER_TOKEN 4
#define CC_SKILL_VIEW_PRUNE_MIN_CHARS 5000
#define CC_COMPRESSED_SUMMARY_METADATA_KEY "_compressed_summary"
#define CC_COMPRESSION_CONTINUATION_USER_CONTENT "Continue from the compressed conversation context above. This marker exists because no human user turn was available."
#define CC_LEGACY_COMPRESSION_CONTINUATION_USER_CONTENT "Continue from the compressed conversation context above. This marker exists because the compacted transcript contained no preserved user turn."
#define CC_MERGED_PRIOR_CONTEXT_HEADER "[PRIOR CONTEXT — for reference only; not a new message]"
#define CC_TODO_INJECTION_HEADER "[Your active task list was preserved across context compression]"
#define CC_HISTORICAL_TASK_HEADING "## Historical Task Snapshot"
/* threshold / probe tuning */
#define CC_MINIMUM_CONTEXT_LENGTH 64000
#define CC_SMALL_CTX_WINDOW_LIMIT 512000
#define CC_SMALL_CTX_THRESHOLD_PERCENT 0.75
#define CC_MIN_CTX_TRIGGER_RATIO 0.85
#define CC_RESTART_HANDOFF_PROBE_EXTRA_MESSAGES 4
/* ── Content truncation + tool-arg bounds (context_compressor.py _CONTENT_*) ─ */
#define CC_CONTENT_CONTENT_MAX 6000
#define CC_CONTENT_CONTENT_HEAD 4000
#define CC_CONTENT_CONTENT_TAIL 1500
#define CC_TOOL_ARGS_MAX 1500
#define CC_TOOL_ARGS_HEAD 1200
#define CC_FALLBACK_TURN_MAX_CHARS 700
#define CC_MAX_TAIL_MESSAGE_FLOOR 8

extern const char *cc_historical_summary_prefixes[];
extern const size_t cc_num_historical_prefixes;
extern const char *cc_image_part_types[];
extern const size_t cc_num_image_part_types;

#endif /* CONTEXT_COMPRESSOR_CONSTANTS_H */
