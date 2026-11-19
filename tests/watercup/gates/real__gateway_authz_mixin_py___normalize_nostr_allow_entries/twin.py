"""twin for gateway/authz_mixin.py:_normalize_nostr_allow_entries (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/authz_mixin.py"
    PY_FN = "_normalize_nostr_allow_entries"
    C_FN = "_normalize_nostr_allow_entries"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
