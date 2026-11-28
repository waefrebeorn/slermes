"""twin: _item_id."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "cron/scripts/classify_items.py"; PY_FN = "_item_id"
    C_FN = "cron_scripts_classify_items_u_item_id"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp)
        return self._fn(d.get("item") or {}, d.get("index", 0))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
