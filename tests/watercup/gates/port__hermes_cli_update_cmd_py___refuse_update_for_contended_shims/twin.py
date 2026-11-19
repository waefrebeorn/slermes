"""twin for hermes_cli/update_cmd.py:_refuse_update_for_contended_shims (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_refuse_update_for_contended_shims"
    C_FN = "upd_refuse_update_for_contended_shims"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
