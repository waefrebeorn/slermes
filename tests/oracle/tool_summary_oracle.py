#!/usr/bin/env python3
"""Differential oracle: C compress_summarize_tool vs Python _summarize_tool_result.

The C summary line is what compaction shows instead of a tool result, so it has to match the Python
format character for character. This drives the real C function (tests/unit/tool_summary_diff.c, linked
against libjson only) and the real Python function over the same table and reports disagreements.

    python tests/oracle/tool_summary_oracle.py [-v]

Exit 0 = agree, 1 = disagreement, 2 = could not run (missing binary / import).
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
EXE = REPO / ".testbin" / "tool_summary_diff.exe"
QUARRY = Path("C:/Users/eman5/hermes-agent")

# Every tool the C summarizer knows about, plus the fallback paths.
CASES = [
    ("terminal", '{"command": "echo hi"}', '{"exit_code": 0, "output": "hi\\n"}'),
    ("terminal", '{"command": "false"}', '{"exit_code": 1, "output": ""}'),
    ("terminal", '{"command": "%s"}' % ("x" * 120), "line1\nline2\nline3\n"),
    ("read_file", '{"path": "src/tools/file.c", "offset": 40}', "a\nb\nc\n"),
    ("read_file", '{"path": "README.md"}', "single line"),
    ("read_file", "{}", ""),
    ("write_file", '{"path": "out.txt", "content": "one\\ntwo"}', "wrote 2 lines"),
    ("write_file", '{"path": "out.txt"}', "stale write refused"),
    ("write_file", '{"path": "x"}', '{"error": "permission denied"}'),
    ("search_files", '{"pattern": "TODO", "path": "src", "target": "content"}', '{"total_count": 7}'),
    ("search_files", '{"pattern": "zzz"}', ""),
    ("patch", '{"path": "src/a.c", "mode": "replace"}', "patched"),
    ("web_search", '{"query": "wubu os"}', "results"),
    ("web_extract", '{"url": "https://example.com/"}', "text"),
    ("web_extract", '{"urls": ["https://a/", "https://b/", "https://c/"]}', "text"),
    ("browser_navigate", '{"url": "https://example.com/"}', '{"status": "loaded"}'),
    ("browser_click", '{"ref": "e3"}', "clicked"),
    ("delegate_task", '{"goal": "port the vault module and prove it live against a page"}', "done"),
    ("execute_code", '{"code": "print(1)\\nprint(2)"}', "1\n2\n"),
    ("skill_view", '{"name": "wubu-vault"}', "# Vault\nbody"),
    ("skills_list", '{"category": "devops"}', '{"count": 12}'),
    ("skills_list", "{}", "no json here"),
    ("skill_manage", '{"action": "create", "name": "new-skill"}', "created"),
    ("vision_analyze", '{"question": "what is this"}', "an image"),
    ("memory", '{"action": "add", "target": "user"}', "saved"),
    ("todo_list", "{}", "3 tasks"),
    ("text_to_speech", '{"text": "hi"}', "audio"),
    ("some_unknown_tool", '{"alpha": 1, "beta": 2}', "hello"),
    ("some_unknown_tool", "{}", ""),
    ("some_unknown_tool", '{"alpha": 1}', '{"error": "boom"}'),
]


def build() -> Path:
    if EXE.exists():
        EXE.unlink()
    cc = "C:/winlibs/mingw64/bin/gcc.exe"
    if not Path(cc).exists():
        cc = "gcc"
    cmd = [cc, "-O1", "-w", "-include", "hermes_win32.h", "-D_GNU_SOURCE",
           "-I", "include/compat", "-I", "include", "-I", "src", "-I", "lib", "-I", "lib/libjson",
           "src/agent/tool_result_summary.c", "tests/unit/tool_summary_diff.c",
           "lib/libjson/json.o", "-L", "C:/winlibs/extra-libs", "-ltre", "-o", str(EXE)]
    r = subprocess.run(cmd, cwd=str(REPO), capture_output=True, text=True)
    if r.returncode != 0:
        print("cannot build the harness:\n" + (r.stderr or "")[:900])
        sys.exit(2)
    return EXE


def main() -> int:
    verbose = "-v" in sys.argv
    exe = build()

    sys.path.insert(0, str(QUARRY))
    try:
        from agent.context_compressor import _summarize_tool_result as py_summarize
    except Exception as e:                                    # noqa: BLE001
        print(f"cannot import the Python summarizer: {type(e).__name__}: {e}")
        return 2

    def esc(field: str) -> str:
        return field.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t")

    payload = "".join(f"{n}\t{a}\t{esc(c)}\n" for n, a, c in CASES)
    proc = subprocess.run([str(exe)], input=payload, capture_output=True, text=True)
    if proc.returncode != 0:
        print("harness failed:\n" + (proc.stderr or "")[:600])
        return 2
    got = [l[4:] for l in proc.stdout.splitlines() if l.startswith("OUT ")]
    if len(got) != len(CASES):
        print(f"harness returned {len(got)} lines for {len(CASES)} cases")
        return 2

    problems = 0
    for (name, args, content), c_out in zip(CASES, got):
        p_out = py_summarize(name, args, content)
        if c_out != p_out:
            problems += 1
            print(f"DISAGREE [{name}]")
            print(f"    C : {c_out!r}")
            print(f"    py: {p_out!r}")
        elif verbose:
            print(f"agree [{name}]: {c_out!r}")
    print(f"tool-summary oracle: {len(CASES) - problems} agree, {problems} problem(s)")
    return 0 if problems == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
