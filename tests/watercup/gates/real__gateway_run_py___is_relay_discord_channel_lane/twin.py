"""twin for gateway/run.py:_is_relay_discord_channel_lane (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_is_relay_discord_channel_lane"
    C_FN = "_is_relay_discord_channel_lane"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
