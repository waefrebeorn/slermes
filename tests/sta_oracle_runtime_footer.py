"""Oracle for gateway/runtime_footer.py port.

Loads the LIVE upstream module and evaluates the fixture cases, printing a
compact JSON array of the resulting footer strings for byte-diffing.

argv[1] = fixture path. HOME is pinned so _home_relative_cwd is deterministic.
"""
import sys, os, json
from pathlib import Path

ROOT = Path("/home/wubu/hermes-agent-dev")
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import importlib.util
spec = importlib.util.spec_from_file_location(
    "runtime_footer_oracle", str(ROOT / "gateway" / "runtime_footer.py"))
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_runtime_footer.py <fixture>\n")
        return 1
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        data = json.load(f)

    # Pin HOME to match the C harness (which also sets HOME=/home/oracle), and
    # drop TERMINAL_CWD so the cwd fallback is deterministic.
    os.environ["HOME"] = "/home/oracle"
    os.environ.pop("TERMINAL_CWD", None)

    out = []
    for c in data.get("cases", []):
        fn = c["fn"]
        if fn == "model_short":
            out.append(mod._model_short(c.get("model")))
        elif fn == "home_relative_cwd":
            out.append(mod._home_relative_cwd(c.get("cwd") or ""))
        elif fn == "format_runtime_footer":
            out.append(mod.format_runtime_footer(
                model=c.get("model"),
                context_tokens=c.get("context_tokens", 0),
                context_length=c.get("context_length"),
                cwd=c.get("cwd"),
                fields=c.get("fields", mod._DEFAULT_FIELDS),
            ))
        elif fn == "build_footer_line":
            out.append(mod.build_footer_line(
                user_config=c.get("user_config"),
                platform_key=c.get("platform_key"),
                model=c.get("model"),
                context_tokens=c.get("context_tokens", 0),
                context_length=c.get("context_length"),
                cwd=c.get("cwd"),
            ))
    sys.stdout.write(json.dumps(out, ensure_ascii=False, separators=(",", ":")))


if __name__ == "__main__":
    sys.exit(main())
