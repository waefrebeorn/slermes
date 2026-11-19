"""twin for api_server.py:_prefix_names_served_profile (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "api_server.py"
    PY_FN = "_prefix_names_served_profile"
    C_FN = "apisg_prefix_names_served_profile"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
