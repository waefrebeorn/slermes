"""twin for gateway/relay/media.py:is_relay_media_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/media.py"
    PY_FN = "is_relay_media_url"
    C_FN = "relay_media_client_is_relay_media_url"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
