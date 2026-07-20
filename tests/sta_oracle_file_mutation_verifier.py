#!/usr/bin/env python3
"""
sta_oracle_file_mutation_verifier.py — Python oracle for the file-mutation
verifier (run_agent.AIAgent._record_file_mutation_result +
_format_file_mutation_failure_footer).

Faithful re-implementation of the C port's stateful behavior (the C port
mirrors the *failure-footer* string semantics, keyed by path, showing up to 10
with "... and N more", and backtick-wrapping handled by the C tracker's
literal backtick format). Ground truth is run_agent.py's functions.
"""
import sys
import json


# Mirror of the C tracker semantics (dict keyed by path -> entry).
def run(fixture_lines):
    tracker = {}  # path -> dict(tool, error_preview, is_error)
    results = []

    MUT_TOOLS = {"write_file", "patch", "edit_file", "append_file",
                 "delete_file", "create_file"}

    def extract_paths(args_json):
        paths = []
        try:
            a = json.loads(args_json) if args_json else {}
        except Exception:
            a = {}
        if isinstance(a, dict):
            if isinstance(a.get("path"), str):
                paths.append(a["path"])
            if isinstance(a.get("file_path"), str):
                paths.append(a["file_path"])
            if isinstance(a.get("paths"), list):
                for e in a["paths"]:
                    if isinstance(e, str):
                        paths.append(e)
        return paths

    def record(tool, args_json, result, is_error):
        if tool not in MUT_TOOLS:
            return
        paths = extract_paths(args_json)
        if not paths:
            return
        preview = ""
        if is_error:
            try:
                r = json.loads(result) if result else {}
                if isinstance(r, dict) and isinstance(r.get("error"), str) and r["error"]:
                    preview = r["error"][:255]
            except Exception:
                pass
        for path in paths:
            if path in tracker:
                if not is_error:
                    del tracker[path]
            elif is_error:
                tracker[path] = {"tool": tool, "error_preview": preview, "is_error": True}

    def format_footer():
        failed = {k: v for k, v in tracker.items() if v.get("is_error")}
        if not failed:
            return ""
        lines = [
            "File-mutation verifier: %d file(s) were not modified this turn. "
            "Run `git status` or `read_file` to confirm." % len(failed)
        ]
        shown = 0
        for path, info in failed.items():
            if shown >= 10:
                break
            preview = (info.get("error_preview") or "").strip()
            tool = info.get("tool") or "patch"
            if preview:
                lines.append("  - `%s`  [%s] %s" % (path, tool, preview))
            else:
                lines.append("  - `%s`  [%s] failed" % (path, tool))
            shown += 1
        remaining = len(failed) - shown
        if remaining > 0:
            lines.append("  - ... and %d more" % remaining)
        return "\n".join(lines)

    for line in fixture_lines:
        line = line.rstrip("\n")
        if not line:
            continue
        if line == "init":
            tracker = {}
            continue
        if line == "format":
            results.append({"op": "format", "footer": format_footer()})
            continue
        if line.startswith("record "):
            parts = line[7:].split(" ", 3)
            if len(parts) < 3:
                continue
            tool, path, iserr_s = parts[0], parts[1], parts[2]
            preview = parts[3] if len(parts) > 3 else ""
            record(tool, path, preview, iserr_s == "1")
    return results


def main():
    path = sys.argv[1]
    with open(path, "r", encoding="utf-8") as f:
        lines = f.readlines()
    results = run(lines)
    sys.stdout.write(json.dumps({"results": results}, ensure_ascii=False, separators=(",", ":")))


if __name__ == "__main__":
    main()
