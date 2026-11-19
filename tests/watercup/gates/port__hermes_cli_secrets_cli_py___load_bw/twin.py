"""twin for hermes_cli/secrets_cli.py:_load_bw (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/secrets_cli.py"
    PY_FN = "_load_bw"
    C_FN = "_load_bw"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
