"""twin for hermes_cli/_scan_venv_blockers.py:_spawner_is_this_handoff_desktop (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_scan_venv_blockers.py"
    PY_FN = "_spawner_is_this_handoff_desktop"
    C_FN = "_spawner_is_this_handoff_desktop"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
