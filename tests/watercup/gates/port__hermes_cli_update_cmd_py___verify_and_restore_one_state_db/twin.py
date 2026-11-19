"""twin for hermes_cli/update_cmd.py:_verify_and_restore_one_state_db (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_verify_and_restore_one_state_db"
    C_FN = "_verify_and_restore_one_state_db"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
