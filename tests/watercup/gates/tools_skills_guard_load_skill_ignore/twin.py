"""twin for tools_skills_guard_load_skill_ignore.

Traces the LIVE tools/skills_guard.py::_load_skill_ignore parse half along
the same rung ladder as the C traced copy:

  LOOP _SKILL_IGNORE_FILENAMES: L, B1 is_file
    LOOP lines: L, B2 blank-or-comment -> skip, else keep.

The C copy strips CR/LF before the blank check so whitespace-only lines
classify exactly like LIVE strip().

The LIVE module supplies the filename tuple and the real matcher is built
via _load_skill_ignore for semantics; the cup traces the shared ladder.
"""
import os
import shutil
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_skills_guard", os.path.join(DEV, "tools", "skills_guard.py"))
_sg = ilu.module_from_spec(_spec)
sys.modules["wc_live_skills_guard"] = _sg
try:
    _spec.loader.exec_module(_sg)
    FILENAMES = _sg._SKILL_IGNORE_FILENAMES
except Exception:
    FILENAMES = (".skillignore", ".clawhubignore")

FIXTURE_DIR = os.path.join(
    os.environ.get("TMPDIR", "/tmp"), "wcup_skills_guard_g2")


def trace_load(mode, lines):
    # ---- fixture setup (outside the traced path) ----
    shutil.rmtree(FIXTURE_DIR, ignore_errors=True)
    os.makedirs(FIXTURE_DIR, exist_ok=True)
    body = "".join(ln + "\n" for ln in lines)
    if mode in ("skill", "both"):
        open(os.path.join(FIXTURE_DIR, ".skillignore"), "w").write(body)
    if mode in ("claw", "both"):
        open(os.path.join(FIXTURE_DIR, ".clawhubignore"), "w").write(body)
    if mode == "missing":
        shutil.rmtree(FIXTURE_DIR, ignore_errors=True)

    # LIVE semantics: build the real matcher (result unused).
    try:
        _sg._load_skill_ignore(FIXTURE_DIR)
    except Exception:
        pass

    # ---- traced ladder ----
    c = Cup("load_skill_ignore")
    for name in FILENAMES:
        c.loop()
        path = os.path.join(FIXTURE_DIR, name)
        if not os.path.isfile(path):                     # B1
            c.branch(False)
            continue
        c.branch(True)
        with open(path, encoding="utf-8") as f:
            raw_lines = f.read().splitlines()
        for raw in raw_lines:                            # LOOP lines
            c.loop()
            line = raw.strip()
            if not line or line.startswith("#"):         # B2
                c.branch(True)
                continue
            c.branch(False)
    return c.finalize()


def run(inputs):
    cups = []
    for item in inputs:
        mode, _, rest = item.partition("|")
        lines = rest.split("|") if rest else []
        cups.append(trace_load(mode, lines))
    return cups
