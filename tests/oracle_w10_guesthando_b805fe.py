#!/usr/bin/env python3
"""Differential oracle for hermes_cli/guest_handoff.py (gate w10_guesthando_b805fe)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w10_guesthando_b805fe"
sys.path.insert(0, QUARRY)

try:
    import hermes_cli  # noqa: F401
except Exception:
    pass


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


# result_lines lazily imports tools.tool_labels.app_title. Seed an exact copy so
# the oracle is deterministic even when the full tools package is not installed.
_APP_TITLES = {
    "discord": "Discord", "figma": "Figma", "github": "GitHub", "gmail": "Gmail",
    "googlecalendar": "Google Calendar", "googledocs": "Google Docs",
    "googledrive": "Google Drive", "jira": "Jira", "linear": "Linear", "notion": "Notion",
    "outlook": "Outlook", "slack": "Slack", "stripe_mcp": "Stripe", "todoist": "Todoist",
}


def app_title(slug):
    key = (slug or "").strip()
    known = _APP_TITLES.get(key.lower())
    if known:
        return known
    words = [word for word in key.replace("-", " ").replace("_", " ").split() if word]
    return " ".join(word[:1].upper() + word[1:] for word in words) or key


_seed("tools")
_seed("tools.tool_labels", app_title=app_title)

SRC = QUARRY + "/hermes_cli/guest_handoff.py"
_spec = importlib.util.spec_from_file_location("guest_handoff_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["guest_handoff_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "classify":
        return live._classify(rec.get("status", 0), rec.get("payload", {}))
    if func == "result_lines":
        return live.result_lines(rec.get("status_obj", {}))
    if func == "status_lines":
        return live.status_lines(rec.get("status_obj", {}))
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("classify", "result_lines", "status_lines"):
            continue
        cases += 1
        py = live_out(rec)
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
