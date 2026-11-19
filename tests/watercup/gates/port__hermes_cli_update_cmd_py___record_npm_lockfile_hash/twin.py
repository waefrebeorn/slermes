"""twin for hermes_cli/update_cmd.py:_record_npm_lockfile_hash (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_record_npm_lockfile_hash"
    C_FN = "uc_record_npm_lockfile_hash"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
