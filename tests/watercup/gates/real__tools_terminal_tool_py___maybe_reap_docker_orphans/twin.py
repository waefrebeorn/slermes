"""twin for tools/terminal_tool.py:_maybe_reap_docker_orphans (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_tool.py"
    PY_FN = "_maybe_reap_docker_orphans"
    C_FN = "_maybe_reap_docker_orphans"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
