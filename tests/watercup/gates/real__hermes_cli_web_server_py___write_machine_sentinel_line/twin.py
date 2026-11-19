"""twin for hermes_cli/web_server.py:_write_machine_sentinel_line (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_write_machine_sentinel_line"
    C_FN = "_write_machine_sentinel_line"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
