"""twin for tools/skills_sync_client.py:_decode_jwt_payload_unverified (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync_client.py"
    PY_FN = "_decode_jwt_payload_unverified"
    C_FN = "_decode_jwt_payload_unverified"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
