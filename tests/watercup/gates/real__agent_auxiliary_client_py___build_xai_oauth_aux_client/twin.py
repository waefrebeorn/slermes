"""twin for agent/auxiliary_client.py:_build_xai_oauth_aux_client (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_build_xai_oauth_aux_client"
    C_FN = "_build_xai_oauth_aux_client"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
