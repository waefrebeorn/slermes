"""twin for hermes_cli/install_identity.py:_install_id_file_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/install_identity.py"
    PY_FN = "_install_id_file_lock"
    C_FN = "_install_id_file_lock"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
