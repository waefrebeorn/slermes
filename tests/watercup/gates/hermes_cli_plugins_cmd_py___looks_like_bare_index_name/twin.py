"""twin for hermes_cli/plugins_cmd.py:_looks_like_bare_index_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins_cmd.py"
    PY_FN = "_looks_like_bare_index_name"
    C_FN = "_looks_like_bare_index_name"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
