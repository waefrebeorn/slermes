#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/usage.py (gate w25_usage_61b537).

The live module is loaded from the quarry by file path (its own top level is
stdlib only), and the two pure surfaces of ``hermes usage`` are compared:

  * ``usage_snapshot_document`` over reconstructed snapshot objects -- the
    credential fetch and the human renderer never run, because the snapshot is
    the seam;
  * the exact stderr advisory ``cmd_usage`` prints for a provider with no
    snapshot;
  * the argparse contract attached by ``build_usage_parser``, read back off a
    real parser built against a recording subparser holder.
"""
import argparse
import datetime
import importlib.util
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_usage_61b537"


def load_live():
    path = QUARRY / "hermes_cli" / "subcommands" / "usage.py"
    spec = importlib.util.spec_from_file_location("_live_usage", path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["_live_usage"] = mod
    spec.loader.exec_module(mod)
    return mod


live = load_live()

WANTED = {"snapshot_document", "no_usage_message", "constants"}
SNAPSHOT_KEYS = ("provider", "source", "title", "plan", "fetched_at",
                 "windows", "details", "unavailable_reason")
WINDOW_KEYS = ("label", "used_percent", "reset_at", "detail")


def _dt(value):
    """'' / None stay None (the document guards on truthiness); ISO strings parse."""
    if not value:
        return None
    return datetime.datetime.fromisoformat(value)


def snapshot_of(req):
    raw = req.get("snapshot")
    if not isinstance(raw, dict):
        return None  # the port's documented guard: no object, no document
    fields = {key: raw.get(key) for key in SNAPSHOT_KEYS}
    windows = fields["windows"]
    if windows is not None and not isinstance(windows, list):
        return None  # not iterable as windows: guard
    details = fields["details"]
    if details is not None and not isinstance(details, list):
        return None
    fields["fetched_at"] = _dt(fields["fetched_at"]) or datetime.datetime(1970, 1, 1)
    fields["details"] = list(details or [])
    fields["windows"] = [
        SimpleNamespace(**{key: (_dt(w.get(key)) if key == "reset_at" else w.get(key))
                           for key in WINDOW_KEYS})
        if isinstance(w, dict) else w
        for w in (windows or [])
    ]
    return SimpleNamespace(**fields)


def snapshot_document(req):
    snap = snapshot_of(req)
    if snap is None:
        return None
    return live.usage_snapshot_document(snap)


def no_usage_message(req):
    provider = req.get("provider")
    return (
        f"No account usage available for provider '{provider}': no credential is configured for it, "
        "the provider has no usage endpoint, or the fetch failed."
    )


class _RecordingSubparsers:
    def __init__(self):
        self.calls = []

    def add_parser(self, name, **kwargs):
        self.calls.append((name, kwargs))
        return argparse.ArgumentParser()


def constants():
    built = argparse.ArgumentParser()
    holder = _RecordingSubparsers()
    holder.add_parser = lambda n, **kw: (holder.calls.append((n, kw)), built)[1]
    live.build_usage_parser(holder)
    name, kwargs = holder.calls[0]
    actions = {tuple(a.option_strings): a for a in built._actions if a.option_strings}
    provider = actions[("--provider",)]
    as_json = actions[("--json",)]
    return {
        "name": name,
        "help": kwargs["help"],
        "description": kwargs["description"],
        "provider_flag": "--provider",
        "provider_default": provider.default,
        "provider_help": provider.help,
        "json_flag": "--json",
        "json_default": as_json.default,
        "json_action": type(as_json).__name__,
        "json_help": as_json.help,
        "defaults_func": built._defaults["func"].__name__,
        "unavailable_exit": 1,
    }


def expected(req):
    f = req.get("func")
    if f == "snapshot_document":
        return snapshot_document(req)
    if f == "no_usage_message":
        return no_usage_message(req)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
