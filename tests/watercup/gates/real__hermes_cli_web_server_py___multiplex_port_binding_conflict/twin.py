"""twin for hermes_cli/web_server.py:_multiplex_port_binding_conflict (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_multiplex_port_binding_conflict"
    C_FN = "_multiplex_port_binding_conflict"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
