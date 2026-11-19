"""twin for agent/lsp/servers.py:_spawn_prisma (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/servers.py"
    PY_FN = "_spawn_prisma"
    C_FN = "_spawn_prisma"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
