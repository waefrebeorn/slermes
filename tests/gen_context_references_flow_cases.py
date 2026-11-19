#!/usr/bin/env python3
"""Generate tests/fixtures/context_references_flow.jsonl — the scripted-seam
case corpus for the context_references_flow FLOW + leaf port (18 funcs, >=5
cases each). ASCII scope; POSIX paths; integral floats == ints. Covers normal
paths, branch edges, in-place state mutation, identity (token) vs structural
equality, real scripted exceptions (incl. the BaseException trio), the
OSError / LiveConnectionError / TimeoutExpired catch hierarchies, CM exit
raises replacing outcomes, and fail-closed missing / exhausted scripts.
Run: python tests/gen_context_references_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "context_references_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def raises(kind, message=""):
    return {"__raise__": kind, "message": message}


def attr(**kw):
    return {"__attr__": kw}


def callable_(seam):
    return {"__callable__": seam}


def wrapped(ret):
    """A list RESULT must be wrapped: a bare array script is a per-call queue."""
    return {"__script__": True, "ret": ret}


def ref(raw="@file:f.py", kind="file", target="f.py", start=0, end=9, ls=None, le=None):
    return attr(raw=raw, kind=kind, target=target, start=start, end=end,
                line_start=ls, line_end=le)


def stat(size):
    return attr(st_size=size)


def proc(rc=0, kill="kill", wait="wait"):
    return attr(
        stdout=attr(read1=callable_("read1_stdout")),
        stderr=attr(read1=callable_("read1_stderr")),
        returncode=rc,
        wait=callable_(wait),
        kill=callable_(kill),
    )


def provider(token, prefix="issue"):
    return attr(token=token, prefix=prefix)


FH = attr(readline=callable_("readline"))

# ── __post_init__ ──
add("__post_init__", state={"self": attr(text="t.txt", display="")})
add("__post_init__", state={"self": attr(text="t.txt", display="Shown")})
add("__post_init__", state={"self": attr(text="t.txt", display=None)})
add("__post_init__", state={"self": attr(text="", display="")})
add("__post_init__", state={"self": attr(text="t.txt")})  # absent display -> AttributeError
add("__post_init__", state={"self": attr(display="D")})  # truthy display: text never read

# ── autocomplete (abstract body -> None) ──
for i, (q, lim) in enumerate([("", 10), ("abc", 5), ("x" * 40, 1), ("q", 0), ("issue", 10)]):
    add("autocomplete", self=attr(prefix="issue"), query=q, limit=lim)

# ── expand (abstract body -> None) ──
for t in ["", "abc", "x" * 40, "a:b", None]:
    add("expand", self=attr(prefix="issue"), target=t)

# ── publish ──
add("publish", prefix="issue", held=provider("p1"), provider=provider("p1"), owner=None,
    state={"providers": {}, "owners": {}})  # empty registry -> False
add("publish", prefix="issue", held=provider("p1"), provider=provider("p2"), owner="o1",
    state={"providers": {"issue": provider("p1")}, "owners": {"issue": "o1"}})  # True
add("publish", prefix="issue", held=provider("p9"), provider=provider("p2"), owner="o1",
    state={"providers": {"issue": provider("p1")}, "owners": {"issue": "o1"}})  # handed off -> False
add("publish", prefix="issue", held=provider("p1"), provider=provider("p2"), owner="o2",
    state={"providers": {"issue": provider("p1")}, "owners": {"issue": "o1"}})  # owner changed -> False
add("publish", prefix="issue", held=provider("p1"), provider=provider("p2"), owner=None,
    state={"providers": {"issue": provider("p1")}, "owners": {}})  # absent owner == None -> True
add("publish", prefix="issue", held=provider("p1"), provider=provider("p2"), owner=None,
    state={"providers": {"issue": provider("p1")}, "owners": {"issue": None}})  # present-null owner -> True
add("publish", prefix="issue", held=provider("p1"), provider=provider("p2"), owner="o1",
    state={"providers": {"other": provider("p9")}, "owners": {"other": "o9"}})  # prefix absent -> False

# ── git_args_diff / git_args_staged ──
for tgt in ["1", "5", None, "", "x"]:
    add("git_args_diff", ref=ref(target=tgt))
for tgt in ["1", "5", None, "", "x"]:
    add("git_args_staged", ref=ref(target=tgt))

# ── git_args_git ──
add("git_args_git", ref=ref(target="3"))
add("git_args_git", ref=ref(target=None))
add("git_args_git", ref=ref(target=""))
add("git_args_git", ref=ref(target="0"))
add("git_args_git", ref=ref(target="-5"))
add("git_args_git", ref=ref(target="99"))
add("git_args_git", ref=ref(target=" 7 "))
add("git_args_git", ref=ref(target="abc"))
add("git_args_git", ref=ref(target="1_0"))
add("git_args_git", ref=ref(target="+4"))
add("git_args_git", ref=ref(target="5.5"))
add("git_args_git", ref=ref(target="1\u001c"))  # int() does NOT strip \x1c
add("git_args_git", ref=ref(target="12x"))

# ── _is_under ──
add("_is_under", path="/a/b", root="/a")
add("_is_under", path="/a", root="/a")
add("_is_under", path="/ab", root="/a")
add("_is_under", path="/a/b", root="/")
add("_is_under", path="/a/b", root="/a/b/c")
add("_is_under", path="/a/b/", root="/a/")
add("_is_under", path="/a//b", root="/a")

# ── _composer_paste_roots ──
add("_composer_paste_roots", io={"_hermes_dirs": wrapped(["/home/u/.hermes"])})
add("_composer_paste_roots", io={"_hermes_dirs": wrapped(["/"])})
add("_composer_paste_roots", io={"_hermes_dirs": wrapped(["/a/", "/b"])})
add("_composer_paste_roots", io={"_hermes_dirs": wrapped([])})
add("_composer_paste_roots", io={"_hermes_dirs": raises("RuntimeError", "boom")})
add("_composer_paste_roots", io={"_hermes_dirs": None})  # TypeError: not iterable
add("_composer_paste_roots", io={})  # fail-closed

# ── _agent_staged_path ──
add("_agent_staged_path", path="/stage/f.txt",
    io={"get_cache_directory_mounts": wrapped([{"host_path": "/stage"}]),
        "expanduser": "/stage", "resolve": "/stage"})
add("_agent_staged_path", path="/elsewhere/f.txt",
    io={"get_cache_directory_mounts": wrapped([{"host_path": "/stage"}]),
        "expanduser": "/stage", "resolve": "/stage"})
add("_agent_staged_path", path="/stage/f.txt",
    io={"get_cache_directory_mounts": wrapped([])})
add("_agent_staged_path", path="/stage/f.txt",
    io={"get_cache_directory_mounts": raises("ValueError", "bad")})  # caught -> False
add("_agent_staged_path", path="/stage/f.txt",
    io={"get_cache_directory_mounts": raises("SystemExit", "bye")})  # BaseException passthrough
add("_agent_staged_path", path="/stage/f.txt",
    io={"get_cache_directory_mounts": wrapped([{"other": 1}])})  # KeyError caught -> False
add("_agent_staged_path", path="/stage/f.txt",
    io={"get_cache_directory_mounts": wrapped([{"host_path": "/stage"}]),
        "expanduser": raises("OSError", "no")})  # caught -> False
add("_agent_staged_path", path="/stage/f.txt",
    io={"get_cache_directory_mounts": wrapped([{"host_path": "/no"}, {"host_path": "/stage"}]),
        "expanduser": ["/no", "/stage"], "resolve": ["/no", "/stage"]})
add("_agent_staged_path", path="/stage/f.txt",
    io={"get_cache_directory_mounts": wrapped([{"host_path": "/stage"}, {"host_path": "/never"}]),
        "expanduser": "/stage", "resolve": "/stage"})  # short-circuit: 2nd entry untouched
add("_agent_staged_path", path="/stage/f.txt", io={})  # fail-closed RuntimeError caught -> False

# ── _on_disk_reference_block ──
OD = dict(ref=ref(raw="@file:x.bin", target="x.bin"), path="/w/x.bin",
          descriptor="application/octet-stream", reason="binary file, not inlined as text.",
          guidance="Use it.")
add("_on_disk_reference_block", **OD,
    io={"stat": stat(123), "format_bytes": "123 B", "_agent_visible_path": "/w/x.bin"})
add("_on_disk_reference_block", **OD,
    io={"stat": raises("OSError", "gone"), "format_bytes": "x", "_agent_visible_path": "/w/x.bin"})
add("_on_disk_reference_block", **OD,
    io={"stat": raises("FileNotFoundError", "gone"), "_agent_visible_path": "/w/x.bin"})
add("_on_disk_reference_block", **OD,
    io={"stat": raises("ValueError", "bad"), "_agent_visible_path": "/w/x.bin"})
add("_on_disk_reference_block", **OD,
    io={"stat": stat(5), "format_bytes": raises("OSError", "fmt"), "_agent_visible_path": "/w/x.bin"})
add("_on_disk_reference_block", **OD,
    io={"stat": attr(other=1), "_agent_visible_path": "/w/x.bin"})  # absent st_size -> AttributeError
add("_on_disk_reference_block", **OD,
    io={"stat": stat(5), "format_bytes": "5 B", "_agent_visible_path": raises("RuntimeError", "av")})
add("_on_disk_reference_block", **OD,
    io={"stat": raises("SystemExit", "bye")})  # BaseException past except OSError
add("_on_disk_reference_block", **OD,
    io={"stat": stat(5), "format_bytes": None, "_agent_visible_path": "/w/x.bin"})  # size None -> "None"
add("_on_disk_reference_block", **OD, io={})  # fail-closed

# ── _oversized_text_reference_block ──
OT = dict(ref=ref(raw="@file:big.txt", target="big.txt"), path="/w/big.txt")
add("_oversized_text_reference_block", **OT, text_tokens=500,
    io={"stat": stat(2000), "format_bytes": "2.0 KB", "_agent_visible_path": "/w/big.txt"})
add("_oversized_text_reference_block", **OT, text_tokens=0,
    io={"stat": stat(0), "format_bytes": "0 B", "_agent_visible_path": "/w/big.txt"})
add("_oversized_text_reference_block", **OT, text_tokens=42,
    io={"stat": raises("OSError", "gone"), "_agent_visible_path": "/w/big.txt"})
add("_oversized_text_reference_block", **OT, text_tokens=42,
    io={"stat": stat(9), "format_bytes": raises("ValueError", "bad"), "_agent_visible_path": "/w"})
add("_oversized_text_reference_block", **OT, text_tokens=42,
    io={"stat": stat(9), "format_bytes": "9 B", "_agent_visible_path": raises("KeyboardInterrupt", "")})
add("_oversized_text_reference_block", **OT, text_tokens=42, io={})  # fail-closed

# ── _next_line ──
add("_next_line", fh=FH, collect=True, remaining=None, line_cap=None,
    io={"readline": ["ab", "cd\n"]})
add("_next_line", fh=FH, collect=False, remaining=None, line_cap=None,
    io={"readline": ["x\n"]})
add("_next_line", fh=FH, collect=True, remaining=None, line_cap=None,
    io={"readline": [""]})  # EOF immediately -> None
add("_next_line", fh=FH, collect=True, remaining=3, line_cap=None,
    io={"readline": ["ab", "cd"]})  # collected 4 > 3 -> stop after append
add("_next_line", fh=FH, collect=True, remaining=None, line_cap=5,
    io={"readline": ["abcde", "fg\n"]})
add("_next_line", fh=FH, collect=True, remaining=None, line_cap=None,
    io={"readline": ["only", ""]})  # no newline, then EOF
add("_next_line", fh=FH, collect=True, remaining=None, line_cap=None,
    io={"readline": raises("ValueError", "read")})
add("_next_line", fh=FH, collect=False, remaining=None, line_cap=None,
    io={"readline": ["a", "b\n"]})
add("_next_line", fh=FH, collect=True, remaining=0, line_cap=None,
    io={"readline": ["a\n"]})
add("_next_line", fh=FH, collect=True, remaining=None, line_cap=None, io={})  # fail-closed

# ── _read_file_reference ──
add("_read_file_reference", ref=ref(), path="/w/f.py", max_inline_tokens=None,
    io={"_is_binary_file": True, "_binary_reference_block": "BLK"})
add("_read_file_reference", ref=ref(), path="/w/f.py", max_inline_tokens=None,
    io={"_is_binary_file": False, "stat": stat(10), "read_text": "hello"})
add("_read_file_reference", ref=ref(), path="/w/f.py", max_inline_tokens=2,
    io={"_is_binary_file": False, "stat": [stat(100), stat(100)],
        "format_bytes": "100 B", "_agent_visible_path": "/w/f.py"})  # size 100 > 8 -> oversized(25)
add("_read_file_reference", ref=ref(ls=2, le=3), path="/w/f.py", max_inline_tokens=None,
    io={"_is_binary_file": False, "open": None,
        "readline": ["skip\n", "l2\n", "l3\n"], "open.__exit__": None})
add("_read_file_reference", ref=ref(ls=5), path="/w/f.py", max_inline_tokens=None,
    io={"_is_binary_file": False, "open": None,
        "readline": ["a\n", "", ""], "open.__exit__": None})  # EOF during skip
add("_read_file_reference", ref=ref(ls=1, le=5), path="/w/f.py", max_inline_tokens=1,
    io={"_is_binary_file": False, "open": None, "readline": ["12345\n"],
        "stat": stat(5), "format_bytes": "5 B", "_agent_visible_path": "/w/f.py",
        "open.__exit__": None})  # budget 4, line 5 chars -> oversized(1) inside with
add("_read_file_reference", ref=ref(ls=1, le=2), path="/w/f.py", max_inline_tokens=None,
    io={"_is_binary_file": False, "open": None,
        "readline": raises("LiveConnectionError", "live db"), "open.__exit__": None})
add("_read_file_reference", ref=ref(ls=1, le=1), path="/w/f.py", max_inline_tokens=None,
    io={"_is_binary_file": False, "open": None, "readline": ["ok\n"],
        "open.__exit__": raises("RuntimeError", "exit")})  # exit raise replaces ret
add("_read_file_reference", ref=ref(), path="/w/f.py", max_inline_tokens=None,
    io={"_is_binary_file": False, "stat": raises("OSError", "gone")})
add("_read_file_reference", ref=ref(), path="/w/f.py", max_inline_tokens=None, io={})  # fail-closed
add("_read_file_reference", ref=ref(ls=1, le=1), path="/w/f.py", max_inline_tokens=None,
    io={"_is_binary_file": False, "open": raises("OSError", "open")})  # open raise: no __exit__

# ── _format_file_reference ──
FF = dict(ref=ref(raw="@file:f.py"), path="/w/f.py", text="hi")
add("_format_file_reference", **FF, max_inline_tokens=None, io={"estimate_tokens_rough": 2})
add("_format_file_reference", ref=ref(raw="@file:f.xyz"), path="/w/f.xyz", text="hi",
    max_inline_tokens=None, io={"estimate_tokens_rough": 2})  # unknown ext -> lang ""
add("_format_file_reference", **FF, max_inline_tokens=10,
    io={"estimate_tokens_rough": 100, "stat": stat(400), "format_bytes": "400 B",
        "_agent_visible_path": "/w/f.py"})  # oversized
add("_format_file_reference", **FF, max_inline_tokens=10, io={"estimate_tokens_rough": 10})  # equal -> inline
add("_format_file_reference", **FF, max_inline_tokens=None,
    io={"estimate_tokens_rough": raises("RuntimeError", "tok")})
add("_format_file_reference", ref=ref(raw="@file:F.PY"), path="/w/F.PY", text="hi",
    max_inline_tokens=None, io={"estimate_tokens_rough": 2})  # suffix lowercased
add("_format_file_reference", ref=ref(raw="@file:e.py"), path="/w/e.py", text="",
    max_inline_tokens=None, io={"estimate_tokens_rough": 0})
add("_format_file_reference", **FF, max_inline_tokens=None, io={})  # fail-closed

# ── _expand_path_reference ──
def ep_io(**over):
    io = {
        "_resolve_path": "/w/f.py",
        "_ensure_reference_path_allowed": None,
        "exists": True,
        "is_file": True,
        "offline_file_access": None,
        "_is_binary_file": False,
        "stat": stat(2),
        "read_text": "hi",
        "offline_file_access.__exit__": None,
        "estimate_tokens_rough": 2,
    }
    io.update(over)
    return io


add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root="/w", max_inline_tokens=None, io=ep_io())
add("_expand_path_reference", ref=ref(raw="@folder:d", kind="folder", target="d"), cwd="/w",
    allowed_root="/w", max_inline_tokens=None,
    io={"_resolve_path": "/w/d", "_ensure_reference_path_allowed": None, "exists": True,
        "is_dir": True, "_build_folder_listing": "d/\n- a (1 lines)", "estimate_tokens_rough": 5})
add("_expand_path_reference", ref=ref(raw="@file:x.py", target="x.py"), cwd="/w", allowed_root="/w",
    max_inline_tokens=None, io=ep_io(**{"exists": False}))
add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root="/w", max_inline_tokens=None,
    io=ep_io(is_file=False))
add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root="/w", max_inline_tokens=None,
    io=ep_io(_resolve_path=raises("ValueError", "path is outside the allowed workspace")))
add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root="/w", max_inline_tokens=None,
    io=ep_io(_ensure_reference_path_allowed=raises("ValueError", "sensitive")))
add("_expand_path_reference", ref=ref(raw="@folder:d", kind="folder", target="d"), cwd="/w",
    allowed_root="/w", max_inline_tokens=None,
    io={"_resolve_path": "/w/d", "_ensure_reference_path_allowed": None, "exists": True, "is_dir": False})
add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root="/w", max_inline_tokens=None,
    io=ep_io(offline_file_access=raises("LiveConnectionError", "live"),
             stat=stat(7), format_bytes="7 B", _agent_visible_path="/w/f.py"))
add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root="/w", max_inline_tokens=None,
    io=ep_io(read_text=raises("LiveConnectionError", "live"),
             stat=stat(7), format_bytes="7 B", _agent_visible_path="/w/f.py"))
add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root="/w", max_inline_tokens=None,
    io=ep_io(**{"offline_file_access.__exit__": raises("LiveConnectionError", "exit live"),
                "stat": stat(7), "format_bytes": "7 B", "_agent_visible_path": "/w/f.py"}))
add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root="/w", max_inline_tokens=None,
    io=ep_io(read_text=raises("ValueError", "decode")))
add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root="/w", max_inline_tokens=None,
    io=ep_io(**{"offline_file_access.__exit__": raises("RuntimeError", "exit")}))
add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root="/w", max_inline_tokens=None,
    io=ep_io(_is_binary_file=True, _binary_reference_block="BLK"))
add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root="/w", max_inline_tokens=None, io={})
add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root="/w", max_inline_tokens=None,
    io=ep_io(_ensure_reference_path_allowed=raises("SystemExit", "bye")))
add("_expand_path_reference", ref=ref(), cwd="/w", allowed_root=None, max_inline_tokens=None,
    io=ep_io())  # allowed_root None -> kwarg dropped

# ── _run_quiet ──
RQ = dict(cmd=["git", "diff"], cwd="/w", timeout=30)
add("_run_quiet", **RQ, env=None, is_windows=True,
    io={"windows_hide_flags": 134217728, "popen": proc(0),
        "read1_stdout": ["out", ""], "read1_stderr": ["err", ""], "wait": None})
add("_run_quiet", **RQ, env=None, is_windows=False,
    io={"popen": proc(0), "read1_stdout": ["out", ""], "read1_stderr": ["err", ""], "wait": None})
add("_run_quiet", **RQ, env=None, is_windows=True,
    io={"windows_hide_flags": 0, "popen": proc(0),
        "read1_stdout": ["", ""], "read1_stderr": ["", ""],
        "wait": [raises("TimeoutExpired", "ignored"), None], "kill": None})
add("_run_quiet", **RQ, env=None, is_windows=False,
    io={"popen": proc(0), "read1_stdout": ["x" * 4194305], "read1_stderr": [""],
        "wait": None, "kill": None})  # cap exceeded -> truncated, rc 0 -> 137, sink empty
add("_run_quiet", **RQ, env=None, is_windows=False,
    io={"popen": proc(0), "read1_stdout": [""], "read1_stderr": [""],
        "wait": raises("OSError", "wait")})  # not TimeoutExpired -> propagate
add("_run_quiet", **RQ, env=None, is_windows=False,
    io={"popen": raises("FileNotFoundError", "git")})
add("_run_quiet", **RQ, env=None, is_windows=False,
    io={"popen": proc(0), "read1_stdout": [""], "read1_stderr": [""],
        "wait": raises("TimeoutExpired", "x"), "kill": raises("RuntimeError", "kill")})
add("_run_quiet", **RQ, env=None, is_windows=False,
    io={"popen": attr(stdout=attr(read1=callable_("read1_stdout")),
                      stderr=attr(read1=callable_("read1_stderr")),
                      wait=callable_("wait"), kill=callable_("kill")),
        "read1_stdout": [""], "read1_stderr": [""], "wait": None})  # absent returncode
add("_run_quiet", **RQ, env={"A": "B"}, is_windows=False,
    io={"popen": proc(5), "read1_stdout": ["o", ""], "read1_stderr": ["e", ""], "wait": None})
add("_run_quiet", **RQ, env=None, is_windows=False,
    io={"popen": proc(0), "read1_stdout": [""], "read1_stderr": [""],
        "wait": [raises("TimeoutExpired", "x"), raises("ValueError", "w2")], "kill": None})
add("_run_quiet", **RQ, env=None, is_windows=False, io={})  # fail-closed popen

# ── _drain ──
add("_drain", stream="stdout", proc=proc(0), io={"read1_stdout": ["a", "b", ""]})
add("_drain", stream="stdout", proc=proc(0), io={"read1_stdout": [""]})
add("_drain", stream="stdout", proc=proc(0),
    io={"read1_stdout": ["x" * 4194305], "kill": None})  # cap -> truncated + kill
add("_drain", stream="stdout", proc=proc(0), io={"read1_stdout": raises("OSError", "read")})
add("_drain", stream="stdout", proc=proc(0),
    io={"read1_stdout": ["x" * 4194305], "kill": raises("RuntimeError", "kill")})
add("_drain", stream="stderr", proc=proc(0), io={"read1_stderr": ["e1", "e2", ""]})
add("_drain", stream="stdout", proc=proc(0), io={})  # fail-closed


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    print("wrote %s: %d cases, %d funcs" % (OUT, len(CASES), len(counts)))
    for fn in sorted(counts):
        flag = "  LOW" if counts[fn] < 5 else ""
        print("  %-32s %d%s" % (fn, counts[fn], flag))
    low = [fn for fn, n in counts.items() if n < 5]
    if low:
        raise SystemExit("funcs with <5 cases: %s" % low)


if __name__ == "__main__":
    main()