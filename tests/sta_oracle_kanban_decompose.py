#!/usr/bin/env python3
"""Oracle for the pure triage/roster helpers in hermes_cli/kanban_decompose.py.

Reads the same fixture JSON the C harness reads, runs the LIVE Python module's
pure helpers, and prints the SAME JSON shape the C harness prints. The runner
diffs the two outputs byte-for-byte.

Only the LLM-free concern is covered here: _extract_json_blob, the two
_resolve_* helpers, _build_roster / _format_roster, and
_normalize_assignee_choice.
"""

import sys, os, json, re

# Keep HERMES_HOME set to the runner's temp dir. The Python profiles module
# resolves its home through HERMES_HOME (NOT SLERMES_HOME), so dropping it
# would make the oracle fall back to the developer's real ~/.hermes and never
# see the profiles the C harness wrote. The runner exports both SLERMES_HOME
# (C engine) and HERMES_HOME (Python profiles) to the same temp dir.

ROOT = os.path.expanduser("~/hermes-agent-dev")
# The C port under slermes/ is validated against the LIVE Python source of
# truth, which lives in the sibling hermes_cli/ package at the top of the
# hermes-agent-dev tree (one level ABOVE slermes/). A relative parent-of-tests
# computation would land inside slermes/ (no hermes_cli there) and silently
# fall through to a *stale* installed clone elsewhere — so we pin the dev root
# explicitly, matching every other sta_oracle_*.py in this suite.
sys.path.insert(0, ROOT)

import hermes_cli.kanban_decompose as kd

_FENCE_RE = re.compile(r"^```(?:json)?\s*|\s*```$", re.MULTILINE)


def extract_blob_raw(raw):
    """Mirror C's extraction: strip fences, take first { to last }."""
    if not raw:
        return "null"
    stripped = _FENCE_RE.sub("", raw.strip())
    first = stripped.find("{")
    last = stripped.rfind("}")
    if first == -1 or last == -1 or last <= first:
        return "null"
    return stripped[first:last + 1]


def jprint_str(s):
    out = ['"']
    for ch in (s if s is not None else ""):
        if ch in ('"', '\\'):
            out.append('\\')
        out.append(ch)
    out.append('"')
    return ''.join(out)


def main():
    if len(sys.argv) < 2:
        print("usage: sta_oracle_kanban_decompose.py fixture.json", file=sys.stderr)
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as fh:
        data = json.load(fh)

    # Ensure a couple of real profiles exist so _resolve_*/_build_roster behave
    # identically to the C harness. The Python profiles module reads from
    # <home>/profiles (top-level); the C engine also writes there. We mirror
    # BOTH <home>/profiles and <home>/.hermes/profiles (the harness writes
    # both) so the oracle reads exactly what the harness produced regardless of
    # which root either side consults.
    home = os.environ.get("SLERMES_HOME") or os.environ.get("HERMES_HOME") or os.environ.get("HOME")
    if home:
        for root in (os.path.join(home, "profiles"), os.path.join(home, ".hermes", "profiles")):
            for name in ("alice", "bob"):
                d = os.path.join(root, name)
                os.makedirs(d, exist_ok=True)
                cf = os.path.join(d, "config.yaml")
                if not os.path.exists(cf):
                    with open(cf, "w", encoding="utf-8") as f:
                        f.write("name: %s\n" % name)

    parts = []
    for a in data.get("ops", []):
        name = a.get("op")
        if name == "extract":
            raw = a.get("raw", "")
            blob = extract_blob_raw(raw)
            parts.append('{"op":"extract","value":%s}' % jprint_str(blob))
        elif name == "resolve_orch":
            # The fixture stores the *inner* kanban config (e.g.
            # {"orchestrator_profile": "alice"}); the C harness reads the
            # top-level field directly via json_get_field_str, while Python's
            # _resolve_orchestrator_profile expects the full config dict and
            # looks at cfg["kanban"]["orchestrator_profile"]. Wrap the inner
            # dict so Python sees the same logical value the C engine does.
            raw_cfg = a.get("cfg") or "{}"
            inner = json.loads(raw_cfg) if isinstance(raw_cfg, str) else raw_cfg
            cfg = {"kanban": inner}
            r = kd._resolve_orchestrator_profile(cfg)
            parts.append('{"op":"resolve_orch","value":%s}' % jprint_str(r))
        elif name == "resolve_def":
            raw_cfg = a.get("cfg") or "{}"
            inner = json.loads(raw_cfg) if isinstance(raw_cfg, str) else raw_cfg
            cfg = {"kanban": inner}
            r = kd._resolve_default_assignee(cfg)
            parts.append('{"op":"resolve_def","value":%s}' % jprint_str(r))
        elif name == "normalize":
            vn = a.get("valid_names") or []
            r = kd._normalize_assignee_choice(
                a.get("assignee"),
                default_assignee=a.get("default_assignee") or "default",
                valid_names=set(vn))
            parts.append('{"op":"normalize","value":%s}' % jprint_str(r))
        elif name == "build_roster":
            roster, _valid = kd._build_roster()
            parts.append('{"op":"build_roster","value":%s}' % jprint_str(json.dumps(roster, separators=(",", ":"))))
        elif name == "format_roster":
            rj = a.get("roster") or []
            r = kd._format_roster(rj)
            parts.append('{"op":"format_roster","value":%s}' % jprint_str(r))
        else:
            parts.append('{"op":"%s","ok":false}' % name)
    sys.stdout.write("[" + ",".join(parts) + "]\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
