#!/usr/bin/env python3
"""sta_oracle_bot_mode_probe.py — oracle twin for tools/bot_mode_probe.py.

Reads a JSON fixture (argv[1]) describing a filesystem layout to materialize,
writes it under a temp home, then calls the LIVE tools/bot_mode_probe.py
functions and emits one JSON line per op. The C harness (t_port_bot_mode_probe.c)
materializes the SAME layout and emits the same shape; run_oracle.sh diffs them.

Fixture shape:
{
  "home": "rel/root",            # relative to the isolated HERMES_HOME
  "ops": [                       # each op invokes one probe function
    {"op": "section", "home": "rel/root"},
    {"op": "fingerprint", "home": "rel/root"},
    {"op": "epoch_line", "home": "rel/root"},
    {"op": "stale", "stored": "...", "home": "rel/root"},
    {"op": "upgrade", "stored": "...", "home": "rel/root"}
  ],
  "files": {                     # path -> text content (relative to isolated HERMES_HOME)
     "root/profile.yaml": "...",
     "root/SOUL.md": "...",
     "root/config.yaml": "...",
     "root/skills/sk-a/SKILL.md": "...",
     "root/profiles/team2/profile.yaml": "...",
     "root/profiles/team2/SOUL.md": "..."
  }
}

The oracle resolves HERMES_HOME from env (the runner exports HERMES_HOME,
SLERMES_HOME, HOME to the isolated temp dir), materializes files relative to it,
and passes the resolved absolute home to the probe functions (which honor
HERMES_HOME only when their home arg is falsy — here we always pass explicit).
"""
import json
import os
import sys
import tempfile
import importlib.util


def _load_module():
    # Resolve the LIVE tools/bot_mode_probe.py from the dev repo (slermes parent).
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "tools", "bot_mode_probe.py")
    spec = importlib.util.spec_from_file_location("live_bot_mode_probe", cand)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def materialize(home_root, files):
    for rel, content in files.items():
        full = os.path.join(home_root, rel)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8") as f:
            f.write(content)


def main():
    path = sys.argv[1]
    with open(path, encoding="utf-8") as f:
        # The runner substitutes @SBX@ -> isolated HERMES_HOME; honor it.
        raw = f.read()
    sbx = os.environ.get("HERMES_HOME") or os.environ.get("HOME")
    raw = raw.replace("@SBX@", sbx)
    spec = json.loads(raw)

    mod = _load_module()
    home_root = os.path.join(sbx, spec.get("home", "."))
    files = spec.get("files", {})
    materialize(home_root, files)

    out = []
    for case in spec.get("ops", []):
        op = case.get("op")
        h = os.path.join(sbx, case.get("home", spec.get("home", ".")))
        if op == "section":
            res = mod.get_bot_mode_protocol_section(h, False)
            out.append({"op": "section", "out": res or ""})
        elif op == "fingerprint":
            res = mod.capability_fingerprint(h)
            out.append({"op": "fingerprint", "out": res or ""})
        elif op == "epoch_line":
            res = mod.epoch_line(h)
            out.append({"op": "epoch_line", "out": res or ""})
        elif op == "stale":
            res = mod.stored_prompt_capability_stale(case.get("stored"), h)
            out.append({"op": "stale", "out": bool(res)})
        elif op == "upgrade":
            res = mod.stored_bot_chat_prompt_needs_upgrade(case.get("stored"), h)
            out.append({"op": "upgrade", "out": bool(res)})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
