"""twin for hermes_cli/process_identity.py:spawn_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/process_identity.py"
    PY_FN = "spawn_env"
    C_FN = "process_identity_spawn_env"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
