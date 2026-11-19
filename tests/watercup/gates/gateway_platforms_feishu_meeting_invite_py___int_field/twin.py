"""twin for gateway/platforms/feishu_meeting_invite.py:_int_field (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/feishu_meeting_invite.py"
    PY_FN = "_int_field"
    C_FN = "cli_gateway_platforms_feishu_meeting_invite__int_field"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
