"""twin for agent/proxy_sources/iron_proxy.py:_verify_checksums_signature (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/proxy_sources/iron_proxy.py"
    PY_FN = "_verify_checksums_signature"
    C_FN = "ipx_verify_checksums_signature"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
