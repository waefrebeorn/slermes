"""twin for hermes_cli_profiles_profiles_to_serve.

Imports LIVE hermes_cli/profiles.py::profiles_to_serve and traces the SAME
branch ladder as the C harness (which is a traced copy of the REAL port
src/hermes_cli/profiles.c::profile_profiles_to_serve).

Ladder: B1 single-profile mode, B2 profiles root is a dir,
per-entry L tick + B3 default-skip + B4 id-valid.
Value: one V event per served record "name\\thome", sorted by name on BOTH
sides (C emits readdir order; glue sort applied identically).

HERMES_HOME is forced to "" for every input: the C port's slermes_home()
hard rule ignores HERMES_HOME while LIVE honors it for active-profile
inference — any other value is a KNOWN DIVERGENCE (documented in
gate.json notes).
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
if DEV not in sys.path:
    sys.path.insert(0, DEV)

os.environ.pop("HERMES_HOME", None)

from watercup_py import Cup  # noqa: E402

import importlib.util as ilu  # noqa: E402

spec = ilu.spec_from_file_location("wc_profiles", os.path.join(DEV, "hermes_cli", "profiles.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_profiles"] = mod
spec.loader.exec_module(mod)

_PROFILE_ID_RE = re.compile(r"^[a-z0-9][a-z0-9_-]{0,63}$")


def run(inputs):
    # mirror the harness env decision: HERMES_HOME forced off
    os.environ.pop("HERMES_HOME", None)
    home = str(mod._get_default_hermes_home())
    root = mod._get_profiles_root()

    cups = []
    for raw in inputs:
        multiplex = str(raw).strip() == "1"
        c = Cup("profiles_to_serve")

        recs = []
        c.branch(not multiplex)                          # B1
        if not multiplex:
            active = mod.get_active_profile_name() or "default"
            recs.append((active, str(mod.get_profile_dir(active))))
        else:
            recs.append(("default", home))
            c.branch(root.is_dir())                      # B2
            if root.is_dir():
                for entry in sorted(root.iterdir()):
                    c.loop()                             # L per entry
                    if not entry.is_dir():
                        continue
                    name = entry.name
                    c.branch(name == "default")          # B3
                    if name == "default":
                        continue
                    c.branch(bool(_PROFILE_ID_RE.match(name)))   # B4
                    if not _PROFILE_ID_RE.match(name):
                        continue
                    recs.append((name, str(entry)))

            # allowlist=None path: no filtering branches on either side

        for name, hp in sorted(recs, key=lambda r: r[0]):
            c.valstr("%s\t%s" % (name, hp))
        cups.append(c.finalize())
    return cups
