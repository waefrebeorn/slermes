"""twin for hermes_cli/update_cmd.py:_sync_with_upstream_if_needed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_sync_with_upstream_if_needed"
    C_FN = "uc_sync_with_upstream_if_needed"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
