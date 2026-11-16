"""Bootstrap for slermes oracle Python references.

There are two Hermes Python checkouts on this machine:
  * the DEVELOPER tree  -> /home/wubu/hermes-agent-dev  (git at v6xx, contains slermes/)
  * a non-developer pip editable install -> /home/wubu/.hermes/hermes-agent
    (registered on sys.path via an __editable__.hermes_agent-*.pth finder hook)

The oracles must resolve ALL imports (hermes_state, utils, agent.*, hermes_cli.*,
...) from the DEVELOPER tree so they are internally consistent. By default the
editable install shadows hermes_state/utils while agent.skill_commands resolves
to the dev tree -> version skew -> ImportError.

This bootstrap:
  1. keeps the non-dev *venv site-packages* (so third-party deps like yaml work),
  2. STRIPS the non-dev Hermes *source* root from sys.path,
  3. removes any editable meta-path finder bound to that non-dev source,
  4. puts the developer tree first,
then runs the requested oracle module.
"""
import os
import sys

# The C harness writes UTF-8 JSON. On Windows the default stdio codec is
# cp1252, which raises UnicodeEncodeError the moment a case emits non-ASCII
# and takes down the whole oracle run. Force UTF-8 on all three streams.
for _stream in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

# Developer tree = the Hermes checkout, a SIBLING of the slermes repo.
# Four dirnames up from this file is C:/Users/eman5 -- the parent of slermes,
# not the checkout -- so `from tools.X import ...` resolved to
# C:\Users\eman5\tools\X.py and every such oracle failed. Pick the sibling
# that actually exists.
_PARENT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
DEV_ROOT = next(
    (os.path.join(_PARENT, name) for name in
     ("hermes-agent", "hermes-agent-dev", "hermes")
     if os.path.isdir(os.path.join(_PARENT, name))),
    _PARENT,
)

NONDEV_MARKER = ".hermes/hermes-agent"  # only the non-dev SOURCE root


def _is_nondev_source(p):
    rp = os.path.realpath(p)
    if NONDEV_MARKER not in rp:
        return False
    # Keep the venv site-packages (third-party deps like yaml/pyyaml).
    if "/venv/" in rp or rp.endswith("/venv"):
        return False
    # Keep the runtime stdlib (a hermetic CPython under
    # .hermes/hermes-agent/.hermes-runtime/...) — stripping it kills
    # pkgutil and every other stdlib module.
    if ".hermes-runtime/" in rp:
        return False
    return True


# 1+2) Drop the non-dev source root from sys.path, but keep its venv.
sys.path = [p for p in sys.path if not _is_nondev_source(p)]

# 3) Remove any editable meta-path finder bound to the non-dev source.
kept = []
for finder in sys.meta_path:
    rep = repr(finder)
    if NONDEV_MARKER in rep and "venv" not in rep:
        continue
    kept.append(finder)
sys.meta_path[:] = kept

# 4) Developer tree first.
os.environ.setdefault("SLERMES_DEV_ROOT", DEV_ROOT)
sys.path.insert(0, DEV_ROOT)

if __name__ == "__main__":
    import contextlib
    import io
    import json
    import runpy
    import traceback
    from pathlib import Path

    args = sys.argv[1:]

    # Batch mode: one interpreter runs every case for a port.
    #
    # The fixture loop used to spawn `python3 _oracle_boot.py <oracle> <case>`
    # once per case; a 182-case port paid 182 interpreter+import startups, which
    # dwarfed the C side. Here the runner hands over a JSON spec and gets one
    # JSON verdict list back, so the oracle's modules are imported once.
    if args and args[0] == "--batch":
        raw = args[1]
        spec = json.loads(Path(raw[1:]).read_text(encoding="utf-8")
                         if raw.startswith("@") else raw)
        oracle = spec["oracle"]
        results = []
        for case in spec["cases"]:
            sys.argv = [oracle, case["fixture"]] + list(case.get("extra", []))
            buf = io.StringIO()
            stdin_path = case.get("stdin")
            rc = 0
            old_stdin = sys.stdin
            try:
                if stdin_path:
                    sys.stdin = open(stdin_path, "r", encoding="utf-8",
                                     errors="replace")
                with contextlib.redirect_stdout(buf):
                    runpy.run_path(oracle, run_name="__main__")
            except SystemExit as exc:
                rc = exc.code if isinstance(exc.code, int) else 0
            except Exception:
                traceback.print_exc(file=sys.stderr)
                rc = 1
            finally:
                if stdin_path and sys.stdin is not old_stdin:
                    sys.stdin.close()
                sys.stdin = old_stdin
            with open(case["out"], "w", encoding="utf-8", newline="") as fh:
                fh.write(buf.getvalue())
            results.append({"case": case["name"], "rc": rc})
        print(json.dumps(results))
        sys.exit(0)

    # argv: [boot.py, <oracle_script.py>, ...rest]
    oracle = args[0]
    sys.argv = args  # runpy sees [oracle, ...rest]
    import runpy
    runpy.run_path(oracle, run_name="__main__")
