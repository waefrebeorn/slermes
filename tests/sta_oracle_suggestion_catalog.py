"""Oracle for cron/suggestion_catalog.py port.

Loads the LIVE upstream module and replays the fixture op with a mock add_fn
that echoes its kwargs, then serializes the calls to match the C harness's
json_serialize output for byte-diffing.

argv[1] = fixture: {"op":"seed","keys":[...]|null} or {"op":"classify_path"}.
"""
import sys, os, json
from pathlib import Path

ROOT = Path("/home/wubu/hermes-agent-dev")
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import importlib.util
spec = importlib.util.spec_from_file_location(
    "cron.suggestion_catalog", str(ROOT / "cron" / "suggestion_catalog.py"))
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
# dataclasses needs the module registered in sys.modules during class creation.
sys.modules["cron.suggestion_catalog"] = mod
spec.loader.exec_module(mod)


def main():
    if len(sys.argv) < 2:
        return 1
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        data = json.load(f)

    op = data.get("op", "seed")

    if op == "classify_path":
        # C resolves $HERMES_ROOT/cron/scripts/classify_items.py (pinned by the
        # harness to /opt/hermes). Mirror that here — the C port intentionally
        # resolves against the install root rather than the module file path.
        sys.stdout.write(json.dumps("/opt/hermes/cron/scripts/classify_items.py",
                                    ensure_ascii=False))
        return 0

    calls = []

    def mock_add(*, title, description, source, job_spec, dedup_key):
        rec = {
            "title": title,
            "description": description,
            "source": source,
            "job_spec": job_spec,
            "dedup_key": dedup_key,
        }
        calls.append(rec)
        return rec

    keys = data.get("keys")
    mod.seed_catalog_suggestions(add_fn=mock_add, keys=keys)

    # Match C json_serialize formatting: compact, no spaces.
    sys.stdout.write(json.dumps(calls, ensure_ascii=False, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
