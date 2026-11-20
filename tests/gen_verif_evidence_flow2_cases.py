#!/usr/bin/env python3
"""Generate tests/fixtures/verif_evidence_flow2.jsonl for the
verif_evidence_flow2 oracle. Each case is {"func": <name>, "in": {...}}
with scripted seams in in.io (plain value reused; {"__seq__":[...]} per-call,
last repeats; {"__raise__":kind,"msg":text} raises; scripted null -> None;
ABSENT -> fail-closed RuntimeError). >=5 cases per ported function, covering
decision branches, short-circuits, fail-closed seams, BaseException passthrough
and unattributed-state mutation. All paths are POSIX-shaped resolved strings.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "verif_evidence_flow2.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def seq(*vals):
    return {"__seq__": list(vals)}


def io(**kw):
    return kw


# ── ledger_enabled ──
add("ledger_enabled", io=io(host_feature_enabled=False))
add("ledger_enabled", io=io(host_feature_enabled=True, verify_on_stop_enabled=True))
add("ledger_enabled", io=io(host_feature_enabled=True, verify_on_stop_enabled=False))
add("ledger_enabled", io=io(host_feature_enabled=R("SystemExit", "bye")))
add("ledger_enabled", io=io(host_feature_enabled=True, verify_on_stop_enabled=R("ValueError", "bad")))
add("ledger_enabled", io={})  # fail-closed both seams

# ── split_shell_segments ──
add("split_shell_segments", command="pytest -q", io=io(**{"shlex.split": ["pytest", "-q"]}))
add("split_shell_segments", command="pytest && echo ok",
    io=io(**{"shlex.split": seq(["pytest"], ["echo", "ok"])}))
add("split_shell_segments", command="build | tail",
    io=io(**{"shlex.split": seq(["build"], ["tail"])}))
add("split_shell_segments", command="a ; b ; c",
    io=io(**{"shlex.split": seq(["a"], ["b"], ["c"])}))
add("split_shell_segments", command="a || b && c",
    io=io(**{"shlex.split": seq(["a"], ["b"], ["c"])}))
add("split_shell_segments", command="unterminated 'quote")
add("split_shell_segments", command="a ; b |", io=io(**{"shlex.split": ["a"]}))
add("split_shell_segments", command="pytest", io=io(**{"shlex.split": R("ValueError", "no closing quotation")}))
add("split_shell_segments", command="pytest", io=io(**{"shlex.split": R("TypeError", "boom")}))
add("split_shell_segments", command="", io={})

# ── segment_status ──
S = lambda op: {"tokens": ["x"], "following_operator": op}
add("segment_status", segments=[S("&&")], match_index=0, exit_code=0)
add("segment_status", segments=[S("&&")], match_index=0, exit_code=3)
add("segment_status", segments=[S("||")], match_index=0, exit_code=0)
add("segment_status", segments=[S(";"), S(None)], match_index=0, exit_code=0)
add("segment_status", segments=[S(";"), S(None)], match_index=1, exit_code=5)
add("segment_status", segments=[S("&"), S(None)], match_index=0, exit_code=0)
add("segment_status", segments=[S("|"), S(None)], match_index=0, exit_code=0,
    pipeline_status=[7, 0])
add("segment_status", segments=[S("|"), S(None)], match_index=1, exit_code=0,
    pipeline_status=[7, 0])
add("segment_status", segments=[S("&&"), S("|"), S(None)], match_index=1, exit_code=0,
    pipeline_status=[9, 0])
add("segment_status", segments=[], match_index=0, exit_code=0)

# ── wrapper_option_width ──
add("wrapper_option_width", token="-p", flags=["-p", "--preserve-status"], valued=["-s"])
add("wrapper_option_width", token="-s", flags=["-p"], valued=["-s", "--signal"])
add("wrapper_option_width", token="-n5", flags=[], valued=["-n", "--adjustment"])
add("wrapper_option_width", token="--signal=KILL", flags=[], valued=["-s", "--signal"])
add("wrapper_option_width", token="unknown", flags=["-p"], valued=["-s"])
add("wrapper_option_width", token="-verbose", flags=[], valued=["-v"])

# ── skip_wrapper ──
add("skip_wrapper", tokens=["timeout", "300", "pytest"], i=0)
add("skip_wrapper", tokens=["timeout", "-p", "5m", "pytest"], i=0)
add("skip_wrapper", tokens=["timeout", "--signal=KILL", "1.5h", "pytest"], i=0)
add("skip_wrapper", tokens=["timeout", "--help"], i=0)
add("skip_wrapper", tokens=["nice", "-n", "5", "pytest"], i=0)
add("skip_wrapper", tokens=["nohup", "pytest"], i=0)
add("skip_wrapper", tokens=["stdbuf", "-oL", "pytest"], i=0)
add("skip_wrapper", tokens=["pytest"], i=0)

# ── interpreter_family ──
add("interpreter_family", token="python3")
add("interpreter_family", token="/usr/bin/python3.12")
add("interpreter_family", token="./venv/bin/python")
add("interpreter_family", token="python.exe")
add("interpreter_family", token="py")
add("interpreter_family", token="node")
add("interpreter_family", token="bash")
add("interpreter_family", token="notaninterpreter")

# ── is_harmless_interpreter_flag ──
for t in ["-I", "-uEsS", "-3", "-3.12", "-Wignore", "-Xdev", "-m", "--", "-z"]:
    add("is_harmless_interpreter_flag", token=t)

# ── normalize_tokens ──
add("normalize_tokens", tokens=["./pytest", "-q"])
add("normalize_tokens", tokens=["python3", "-W", "ignore", "script.py"])
add("normalize_tokens", tokens=["python", "-X", "dev", "-u", "-m", "pytest"])
add("normalize_tokens", tokens=["bash", "-s", "script.sh"])
add("normalize_tokens", tokens=["./venv/bin/python3.12", "test.py"])
add("normalize_tokens", tokens=["node", "-v"])
add("normalize_tokens", tokens=[])

# ── is_under ──
add("is_under", token="/repo/src/a.py", base="/repo",
    io=io(**{"path.is_absolute": True, "path.resolve": seq("/repo/src/a.py", "/repo")}))
add("is_under", token="/repo", base="/repo",
    io=io(**{"path.is_absolute": True, "path.resolve": seq("/repo", "/repo")}))
add("is_under", token="/other/a.py", base="/repo",
    io=io(**{"path.is_absolute": True, "path.resolve": seq("/other/a.py", "/repo")}))
add("is_under", token="rel/a.py", base="/repo",
    io=io(**{"path.is_absolute": False}))
add("is_under", token="/repo/a.py", base=None, io=io(**{"path.is_absolute": True}))
add("is_under", token="/repo/a.py", base="/repo",
    io=io(**{"path.is_absolute": R("ValueError", "bad")}))
add("is_under", token="/repo/a.py", base="/repo",
    io=io(**{"path.is_absolute": R("SystemExit", "bye")}))
add("is_under", token="/repo/a.py", base="/repo", io={})  # fail-closed

# ── is_temp_script_path ──
add("is_temp_script_path", token="/tmp/hermes-verify-abc.py", root="/repo",
    io=io(**{"path.name": "hermes-verify-abc.py", "tempfile.gettempdir": "/tmp",
             "path.is_absolute": True,
             "path.resolve": seq("/tmp/hermes-verify-abc.py", "/tmp", "/repo")}))
add("is_temp_script_path", token="/tmp/hermes-ad-hoc-x.sh", root=None,
    io=io(**{"path.name": "hermes-ad-hoc-x.sh", "tempfile.gettempdir": "/tmp",
             "path.is_absolute": True,
             "path.resolve": seq("/tmp/hermes-ad-hoc-x.sh", "/tmp")}))
add("is_temp_script_path", token="/tmp/normal.py", root="/repo",
    io=io(**{"path.name": "normal.py"}))
add("is_temp_script_path", token="/repo/hermes-verify-x.py", root="/repo",
    io=io(**{"path.name": "hermes-verify-x.py", "tempfile.gettempdir": "/tmp",
             "path.is_absolute": True,
             "path.resolve": seq("/repo/hermes-verify-x.py", "/tmp", "/repo")}))
add("is_temp_script_path", token="/tmp/hermes-verify-x.py", root="/repo",
    io=io(**{"path.name": R("OSError", "no name")}))
add("is_temp_script_path", token="/tmp/hermes-verify-x.py", root="/repo", io={})

# ── is_interpreter_token ──
for t in ["python3", "/usr/bin/python3.12", "python.exe", "./venv/bin/py",
          "node", "bash", "gcc"]:
    add("is_interpreter_token", token=t)

# ── ad_hoc_script_args ──
add("ad_hoc_script_args", tokens=["/tmp/hermes-verify-x.py", "-q", "--all"], root="/repo",
    io=io(**{"strip_command_prefix": ["/tmp/hermes-verify-x.py", "-q", "--all"],
             "path.name": "hermes-verify-x.py", "tempfile.gettempdir": "/tmp",
             "path.is_absolute": True,
             "path.resolve": seq("/tmp/hermes-verify-x.py", "/tmp", "/repo")}))
add("ad_hoc_script_args", tokens=["env", "FOO=1", "python3", "/tmp/hermes-ad-hoc-y.py", "arg"],
    root=None,
    io=io(**{"strip_command_prefix": seq(["env", "FOO=1", "python3", "/tmp/hermes-ad-hoc-y.py", "arg"],
                                        ["python3", "/tmp/hermes-ad-hoc-y.py", "arg"]),
             "tempfile.gettempdir": "/tmp", "path.is_absolute": True,
             "path.resolve": seq("/tmp/hermes-ad-hoc-y.py", "/tmp")}))
add("ad_hoc_script_args", tokens=["python3", "script.py"], root="/repo",
    io=io(**{"strip_command_prefix": ["python3", "script.py"], "path.is_absolute": True}))
add("ad_hoc_script_args", tokens=["python3", "/tmp/hermes-verify-z.py", "-k"], root="/repo",
    io=io(**{"strip_command_prefix": ["python3", "/tmp/hermes-verify-z.py", "-k"],
             "path.name": seq("python3", "hermes-verify-z.py"),
             "tempfile.gettempdir": "/tmp", "path.is_absolute": True,
             "path.resolve": seq("/tmp/hermes-verify-z.py", "/tmp", "/repo")}))
add("ad_hoc_script_args", tokens=[], root="/repo", io=io(**{"strip_command_prefix": []}))
add("ad_hoc_script_args", tokens=["pytest"], root="/repo", io={})  # fail-closed

# ── root_for ──
add("root_for", facts={"root": "/repo"}, cwd="/elsewhere", io=io(**{}))
add("root_for", facts={}, cwd="/repo", io=io(**{"path.resolve": "/repo"}))
add("root_for", facts=None, cwd=None, io=io(**{"path.resolve": "."}))
add("root_for", facts={"root": ""}, cwd="/repo", io=io(**{"path.resolve": "/repo"}))
add("root_for", facts={"root": "/repo"}, cwd=None, io=io(**{}))

# ── load_changed_paths ──
add("load_changed_paths", raw='["a.py", "b.py"]', io=io(**{"json.loads": ["a.py", "b.py"]}))
add("load_changed_paths", raw="[]", io=io(**{"json.loads": []}))
add("load_changed_paths", raw=None, io=io(**{"json.loads": []}))
add("load_changed_paths", raw="bad", io=io(**{"json.loads": R("ValueError", "bad json")}))
add("load_changed_paths", raw="bad", io=io(**{"json.loads": R("TypeError", "bad type")}))
add("load_changed_paths", raw="bad", io=io(**{"json.loads": R("SystemExit", "bye")}))
add("load_changed_paths", raw="x", io={})  # fail-closed

# ── classify_in_workspace ──
BASE_EV = dict(command="", canonical_command="", kind="", scope="", status="",
               exit_code=0, cwd="", root="", session_id="", output_summary="")
add("classify_in_workspace", command="pytest -q", facts={"verifyCommands": ["pytest"]},
    cwd="/repo", session_id="s1", exit_code=0, output="ok", pipeline_status=None,
    io=io(**{"find_canonical_match": ["pytest", ["-q"], 0], "kind_for_command": "test",
             "looks_like_target": True, "path.resolve": "/repo",
             "summarize_output": "ok"}))
add("classify_in_workspace", command="ruff check .", facts={"verifyCommands": ["ruff check"]},
    cwd="/repo", session_id=None, exit_code=2, output="err", pipeline_status=None,
    io=io(**{"find_canonical_match": ["ruff check", ["."], 2], "kind_for_command": "lint",
             "looks_like_target": False, "path.resolve": "/repo",
             "summarize_output": "err"}))
add("classify_in_workspace", command="pytest", facts={"verifyCommands": ["pytest"]},
    cwd="/repo", session_id="s", exit_code=0, output="", pipeline_status=None,
    io=io(**{"find_canonical_match": ["pytest", ["tests/test_x.py"], 0],
             "kind_for_command": "test", "looks_like_target": seq(False, True),
             "path.resolve": "/repo", "summarize_output": ""}))
add("classify_in_workspace", command="python -m pytest", facts={},
    cwd="/repo", session_id="s", exit_code=0, output="", pipeline_status=None,
    io=io(**{"find_canonical_match": None, "find_ad_hoc_match": None}))
add("classify_in_workspace", command="python /tmp/hermes-verify-x.py",
    facts={"root": "/repo"}, cwd="/repo", session_id="s", exit_code=0, output="",
    pipeline_status=None,
    io=io(**{"find_canonical_match": None, "find_ad_hoc_match": ["-q"],
             "path.resolve": "/repo", "summarize_output": ""}))
add("classify_in_workspace", command="pytest", facts={"verifyCommands": ["pytest"]},
    cwd="/repo", session_id="s", exit_code=0, output="", pipeline_status=None,
    io=io(**{"find_canonical_match": None, "find_ad_hoc_match": None,
             "path.resolve": "/repo", "summarize_output": ""}))  # verifyCommands -> None
add("classify_in_workspace", command="pytest", facts={"verifyCommands": ["pytest"]},
    cwd="/repo", session_id="s", exit_code=0, output="", pipeline_status=None,
    io=io(**{"find_canonical_match": ["pytest", [], 0], "kind_for_command": R("SystemExit", "bye")}))

# ── last_unattributed_run ──
U = [{"session_id": "s1", "root": "/repo", "command": "pytest || true", "at": "2026-01-01T00:00:00"},
     {"session_id": "s2", "root": "/repo", "command": "ruff || true", "at": "2026-01-02T00:00:00"}]
add("last_unattributed_run", session_id="s1", root="/repo", since=None, unattributed=U)
add("last_unattributed_run", session_id="s1", root="/repo", since="2026-01-01T00:00:00", unattributed=U)
add("last_unattributed_run", session_id="s1", root="/repo", since="2020-01-01T00:00:00", unattributed=U)
add("last_unattributed_run", session_id="nobody", root="/repo", since=None, unattributed=U)
add("last_unattributed_run", session_id=None, root="/repo", since=None, unattributed=[])
add("last_unattributed_run", session_id="s2", root="/other", since=None, unattributed=U)

# ── insert_evidence ──
EV = dict(command="pytest", canonical_command="pytest", kind="test", scope="full",
          status="passed", exit_code=0, cwd="/repo", root="/repo",
          session_id="s1", output_summary="ok")
add("insert_evidence", **EV, io=io(**{"utc_now": "2026-01-01T00:00:00",
    "transaction": None, "db.execute": seq({"lastrowid": 7}, None, None),
    "prune_old_events": None, "db.commit": None}), unattributed=U)
add("insert_evidence", **dict(EV, kind="verify", status="passed", scope="full"),
    io=io(**{"utc_now": "2026-01-01T00:00:00", "transaction": None,
             "db.execute": seq({"lastrowid": 9}, None, None, None),
             "prune_old_events": None, "db.commit": None}), unattributed=U)
add("insert_evidence", **dict(EV, kind="test", status="failed"),
    io=io(**{"utc_now": "2026-01-01T00:00:00", "transaction": None,
             "db.execute": seq({"lastrowid": 3}, None),
             "prune_old_events": None, "db.commit": None}))
add("insert_evidence", **EV, io=io(**{"utc_now": "2026-01-01T00:00:00",
    "transaction": None, "db.execute": seq(None), "prune_old_events": None,
    "db.commit": None}))  # no lastrowid -> RuntimeError
add("insert_evidence", **EV, io=io(**{"utc_now": "2026-01-01T00:00:00",
    "transaction": None, "db.execute": R("SystemExit", "bye")}))
add("insert_evidence", **EV, io={})  # fail-closed utc_now

# ── record_verify_run ──
add("record_verify_run", root="/repo", session_id="s1", ok=True,
    command="hermes verify", scope="full", output="ok",
    io=io(**{"host_feature_enabled": True, "verify_on_stop_enabled": True,
             "path.resolve": "/repo", "project_facts": {"root": "/repo"},
             "summarize_output": "ok", "utc_now": "2026-01-01T00:00:00",
             "transaction": None, "db.execute": seq({"lastrowid": 11}, None, None),
             "prune_old_events": None, "db.commit": None}), unattributed=U)
add("record_verify_run", root="/repo", session_id="s1", ok=False,
    command="hermes verify", scope="full", output="bad",
    io=io(**{"host_feature_enabled": True, "verify_on_stop_enabled": True,
             "path.resolve": "/repo", "project_facts": None,
             "summarize_output": "bad", "utc_now": "2026-01-01T00:00:00",
             "transaction": None, "db.execute": seq({"lastrowid": 12}, None),
             "prune_old_events": None, "db.commit": None}))
add("record_verify_run", root="/repo", session_id=None, ok=True,
    command="hermes verify", scope="unknown", output="",
    io=io(**{"host_feature_enabled": False, "verify_on_stop_enabled": True}))
add("record_verify_run", root="/repo", session_id="s", ok=True,
    io=io(**{"host_feature_enabled": True, "verify_on_stop_enabled": R("KeyboardInterrupt", "ctrl-c")}))
add("record_verify_run", root="/repo", session_id="s", ok=True, io={})

# ── newest_workspace_verify ──
add("newest_workspace_verify", root="/repo", since="2026-01-01T00:00:00",
    io=io(**{"db.execute": dict(EV, id=5, created_at="2026-01-02T00:00:00")}))
add("newest_workspace_verify", root="/repo", since=None,
    io=io(**{"db.execute": dict(EV, id=6, created_at="2026-01-02T00:00:00")}))
add("newest_workspace_verify", root="/repo", since="2026-01-01T00:00:00",
    io=io(**{"db.execute": None}))
add("newest_workspace_verify", root="/repo", since=None,
    io=io(**{"db.execute": R("ValueError", "bad")}))
add("newest_workspace_verify", root="/repo", since=None, io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-32s %d" % (k, counts[k]))
    assert all(v >= 5 for v in counts.values()), "need >=5 cases per func"
    assert len(counts) == 19, "need all 19 drive targets (17 unclaimed + 2 internals)"


if __name__ == "__main__":
    main()
