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

# Developer tree = parent of the slermes checkout that contains this file.
DEV_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

NONDEV_MARKER = ".hermes/hermes-agent"  # only the non-dev SOURCE root


def _is_nondev_source(p):
    rp = os.path.realpath(p)
    if NONDEV_MARKER not in rp:
        return False
    # Keep the venv site-packages (third-party deps like yaml/pyyaml).
    if "/venv/" in rp or rp.endswith("/venv"):
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
sys.path.insert(0, DEV_ROOT)

if __name__ == "__main__":
    # argv: [boot.py, <oracle_script.py>, ...rest]
    oracle = sys.argv[1]
    sys.argv = sys.argv[1:]  # runpy sees [oracle, ...rest]
    import runpy
    runpy.run_path(oracle, run_name="__main__")
