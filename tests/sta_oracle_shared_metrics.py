"""Oracle for hermes_cli/observability/shared_metrics.py pure helpers.

Reads JSON array fixture from argv[1]; each element {"op":<fn>, ...}.
Recomputes from LIVE Python source.
"""
import importlib, json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT") or os.path.expanduser("~/.hermes/hermes-agent")
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

# Prevent the real shared_metrics from touching the user's ~/.hermes during
# oracle runs; we test pure helpers only.
_mod = importlib.import_module("hermes_cli.observability.shared_metrics")


def _utc_now():
    from datetime import datetime, timezone
    dt = datetime.now(timezone.utc)
    return {"year": dt.year, "mon": dt.month, "day": dt.day,
            "hour": dt.hour, "min": dt.minute, "sec": dt.second}


def _isoformat(dt):
    from datetime import datetime, timezone, timedelta
    d = datetime(dt["year"], dt["mon"], dt["day"],
                 dt.get("hour", 0), dt.get("min", 0), dt.get("sec", 0),
                 tzinfo=timezone.utc)
    return d.isoformat().replace("+00:00", "Z")


def _ensure_private_directory(path):
    from pathlib import Path
    p = Path(path)
    p.mkdir(parents=True, exist_ok=True)
    try: p.chmod(0o700)
    except OSError: pass
    import os
    return oct(p.stat().st_mode & 0o777) if p.exists() else "none"


def _ensure_private_file(path):
    from pathlib import Path
    p = Path(path)
    p.touch(mode=0o600, exist_ok=True)
    try: p.chmod(0o600)
    except OSError: pass
    import os
    return oct(p.stat().st_mode & 0o777) if p.exists() else "none"


def run(c):
    op = c.get("op")
    if op == "_utc_now":
        return _utc_now()
    if op == "_isoformat":
        return _isoformat(c["dt"])
    if op == "_ensure_private_directory":
        from pathlib import Path
        path = Path(c.get("path", ""))
        _mod.SharedMetricsStore._ensure_private_directory(path)
        import os
        try:
            mode = os.stat(path).st_mode & 0o777
            mode_val = mode
        except OSError:
            mode_val = None
        return {"rc": 0, "mode": mode_val}
    if op == "_ensure_private_file":
        from pathlib import Path
        path = Path(c.get("path", ""))
        _mod.SharedMetricsStore._ensure_private_file(path)
        import os
        try:
            mode = os.stat(path).st_mode & 0o777
            mode_val = mode
        except OSError:
            mode_val = None
        return {"rc": 0, "mode": mode_val}
    return None


def main():
    with open(sys.argv[1]) as f:
        cases = json.load(f)
    for c in cases:
        r = run(c)
        if r is None:
            print("null")
        elif isinstance(r, bool):
            print("true" if r else "false")
        elif isinstance(r, (int, float)):
            if isinstance(r, float) and r == int(r):
                print(int(r))
            else:
                print(r)
        elif isinstance(r, str):
            print(json.dumps(r))
        elif isinstance(r, dict):
            print(json.dumps(r, sort_keys=True))
        elif isinstance(r, list):
            print(json.dumps(r, sort_keys=True))
        else:
            print(json.dumps(r, sort_keys=True))


if __name__ == "__main__":
    main()
