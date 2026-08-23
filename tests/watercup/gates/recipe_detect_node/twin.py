"""twin for recipe_detect_node.

Imports LIVE agent/verify/recipes.py::_detect_node_recipe and traces the SAME
rung ladder as the C harness (merged-dependency framework ladder with
short-circuit elifs, dev/start script selection, build/test loops). The V
event is a canonical rendering of the LIVE Recipe.to_dict() matching the
harness's canon over the port's recipe_to_json().
"""
import json as _json
import os
import sys
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "agent", "verify"))

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_verify_recipes",
                                   os.path.join(DEV, "agent", "verify", "recipes.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_verify_recipes"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

_FRAMEWORKS = ["next", "@sveltejs/kit", "astro", "@remix-run/dev", "react-scripts", "vite"]
_KINDS = ["nextjs", "sveltekit", "astro", "remix", "cra", "vite"]


def _canon(d):
    def lst(v):
        return "|".join(v)
    return "name=%s|kind=%s|bootstrap=%s|build=%s|test=%s|start=%s|port=%s|readiness=%s|evidence=%s" % (
        d["name"], d["kind"], lst(d["bootstrap"]), lst(d["build"]), lst(d["test"]),
        d["start"] if d["start"] else "null",
        str(d["port"]) if d["port"] is not None else "null",
        d.get("readinessPath", "/"),
        "~".join(d["evidence"]))


def run(inputs):
    cups = []
    fn = mod._detect_node_recipe
    root = Path("/nonexistent-wc-root")
    for raw in inputs:
        pkg = _json.loads(raw)

        c = Cup("detect_node_recipe")
        deps = {}
        for key in ("dependencies", "devDependencies"):
            section = pkg.get(key)
            if isinstance(section, dict):
                deps.update(section)

        fw = None
        for i, name in enumerate(_FRAMEWORKS):
            hit = name in deps or (i == 3 and "@remix-run/react" in deps)
            c.branch(hit)                                   # framework rungs
            if hit:
                fw = _KINDS[i]
                break

        raw_scripts = pkg.get("scripts")
        scripts = raw_scripts if isinstance(raw_scripts, dict) else {}
        has_dev = bool(scripts.get("dev"))
        c.branch(has_dev)                                   # dev script?
        if not has_dev:
            c.branch(bool(scripts.get("start")))            # start script?

        for s in ("build", "typecheck"):
            if scripts.get(s):
                c.loop()
        for s in ("test", "check", "lint"):
            if scripts.get(s):
                c.loop()

        rec = fn(root, pkg)
        assert rec.kind == (fw or "node"), (raw, rec.kind, fw)
        d = rec.to_dict()
        c.valstr(_canon(d))
        cups.append(c.finalize())
    return cups
