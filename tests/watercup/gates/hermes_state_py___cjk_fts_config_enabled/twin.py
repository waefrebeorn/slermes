"""twin for hermes_state.py:_cjk_fts_config_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_cjk_fts_config_enabled"
    C_FN = "hermes_state_cjk_fts_config_enabled"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
