"""twin for agent_verify_recipes_detect_python_recipe.

Traces the LIVE agent/verify/recipes.py::_detect_python_recipe ladder with
the same rung order as the C harness:
  B1 marker guard / L per package-manager candidate examined /
  B2 uv, B3 poetry, B4 pipenv, B5 (pyproject&&!requirements) install chain /
  B6 django (B7 manage_py evidence) /
  B7 fastapi (B8 main.py, B9 app.py app_module) /
  B10 flask (B11 app.py, B12 main.py app_module) /
  B13 generic has_tests.
Value event: the LIVE Recipe serialized as kind|name|bootstrap0|tests|start|port|evidence.
Input: one JSON fixture spec per argv item
({"files": {name: content}, "dirs": [names]}); each side materializes its own
identical mkdtemp dir.
"""
import json as _json
import os, sys, tempfile, shutil, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_recipes", os.path.join(DEV, "agent", "verify", "recipes.py"))
_rec = ilu.module_from_spec(_spec)
sys.modules["wc_live_recipes"] = _rec
_spec.loader.exec_module(_rec)

_live_fn = _rec._detect_python_recipe
_PM_CANDIDATES = [
    ("pnpm-lock.yaml", "pnpm"), ("bun.lock", "bun"), ("bun.lockb", "bun"),
    ("yarn.lock", "yarn"), ("package-lock.json", "npm"), ("uv.lock", "uv"),
    ("poetry.lock", "poetry"), ("Pipfile.lock", "pipenv"),
]


def _val(recipe):
    if recipe is None:
        return "none"
    return "%s|%s|%s|%s|%s|%d|%s" % (
        recipe.kind, recipe.name,
        recipe.bootstrap[0] if recipe.bootstrap else "-",
        ",".join(recipe.test), recipe.start or "-",
        recipe.port if recipe.port is not None else 0,
        ";".join(recipe.evidence))


def trace(spec):
    c = Cup("detect_python_recipe")
    root = tempfile.mkdtemp(prefix="wcup_recipe_")
    try:
        for name, content in (spec.get("files") or {}).items():
            with open(os.path.join(root, name), "w") as f:
                f.write(content)
        for d in spec.get("dirs") or []:
            os.mkdir(os.path.join(root, d))
        rdir = _rec.Path(root)

        pyproject = _rec._read_text(rdir, "pyproject.toml")
        requirements = _rec._read_text(rdir, "requirements.txt")
        manage_py = (rdir / "manage.py").exists()
        setup_py = (rdir / "setup.py").exists()
        lower = ("%s\n%s" % (pyproject or "", requirements or "")).lower()

        if not (pyproject or requirements or manage_py or setup_py):   # B1
            c.branch(False)
            live = None
        else:
            c.branch(True)
            pm = None
            for fname, manager in _PM_CANDIDATES:                      # LOOP
                c.loop()
                if (rdir / fname).exists():
                    pm = manager
                    break

            c.branch(pm == "uv")                                       # B2
            if pm != "uv":
                c.branch(pm == "poetry")                               # B3
                if pm != "poetry":
                    c.branch(pm == "pipenv")                           # B4
                    if pm != "pipenv":
                        c.branch(bool(pyproject) and not requirements)  # B5

            is_django = manage_py or "django" in lower
            is_fastapi = "fastapi" in lower or "uvicorn" in lower
            is_flask = "flask" in lower

            if is_django:                                              # B6
                c.branch(True)
                c.branch(manage_py)                                    # B7
            else:
                c.branch(False)
                if is_fastapi:                                         # B7
                    c.branch(True)
                    has_main = (rdir / "main.py").exists()
                    has_app = (rdir / "app.py").exists()
                    c.branch(has_main)                                 # B8
                    c.branch(not has_main and has_app)                 # B9
                else:
                    c.branch(False)
                    if is_flask:                                       # B10
                        c.branch(True)
                        has_app = (rdir / "app.py").exists()
                        has_main = (rdir / "main.py").exists()
                        c.branch(has_app)                              # B11
                        c.branch(not has_app and has_main)             # B12
                    else:
                        c.branch(False)
                        c.branch((rdir / "tests").exists())            # B13
            live = _live_fn(rdir)
        val = _val(live)
    finally:
        shutil.rmtree(root, ignore_errors=True)
    c.valstr(val)
    return c.finalize()


def run(inputs):
    cups = []
    for item in inputs:
        cups.append(trace(_json.loads(item)))
    return cups
