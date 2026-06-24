# Checkpoint 82 — Battleship v109

## Enhancement: display_tool_preview() — full build_tool_preview parity

### What Changed
Rewrote `display_tool_preview()` at `src/cli/display_core.c:538-770` to match Python
`agent/display.py:build_tool_preview()`. Added all special-case tool preview formats:

1. **process**: `action session_id "data" 30s` format (combined from multiple args)
2. **todo**: `reading task list` / `updating N task(s)` / `planning N task(s)`
3. **session_search**: `recall: "query"` format
4. **memory**: `+target: "content"` / `~target: "old"` / `-target: "old"` format
5. **send_message**: `to target: "msg..."` format
6. **skills_list**: `list <category>` format
7. **cronjob**: `create <name|skill>` / `listing` / `action job_id` format
8. **execute_code**: first line only (not raw full code)
9. **browser_snapshot**: `full page` / `compact`
10. **browser_back / browser_get_images / browser_vision**: tool name as preview
11. **web_extract**: first URL from urls array
12. **mixture_of_agents**: `user_prompt` key support
13. Uses `display_oneline()` helper for whitespace collapse

Now uses table-driven primary key loop instead of if-else chain.
All 27 tools in Python's primary_args dict are covered.

### Build: clean, 0 errors. Tests: 4/4 pass.
