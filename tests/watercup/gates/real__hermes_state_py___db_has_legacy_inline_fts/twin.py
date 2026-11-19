"""twin for hermes_state.py:_db_has_legacy_inline_fts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_db_has_legacy_inline_fts"
    C_FN = "hermes_state_db_has_legacy_inline_fts"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
