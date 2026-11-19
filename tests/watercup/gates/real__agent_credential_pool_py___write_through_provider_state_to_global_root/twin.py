"""twin for agent/credential_pool.py:_write_through_provider_state_to_global_root (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"
    PY_FN = "_write_through_provider_state_to_global_root"
    C_FN = "credential_pool_write_through_provider_state_to_global_root"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
