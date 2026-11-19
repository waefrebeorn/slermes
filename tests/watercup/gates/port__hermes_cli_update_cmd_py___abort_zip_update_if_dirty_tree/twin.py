"""twin for hermes_cli/update_cmd.py:_abort_zip_update_if_dirty_tree (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_abort_zip_update_if_dirty_tree"
    C_FN = "_abort_zip_update_if_dirty_tree"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
