#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/google_chat/adapter.py (gate
w27_adapter_97bbcb_p4).

Gate 4 of 4: markdown / retry math / routing policies.

  * ``format_message`` - the live classmethod (which delegates to the package's
    ``cards.format_message``): fenced/inline code shielding, headers,
    bold/strike, Slack-style links, invisible-codepoint stripping;
  * ``_call_with_retry`` / ``_run_supervisor`` backoff - the arithmetic is
    transcribed from the loops with the live class constants (``random()`` /
    ``uniform()`` are replaced by the fixture's draw);
  * ``_resolve_bot_user_id`` candidate list - transcribed from the loop head;
  * ``_load_sa_credentials_from`` - driven for real: ``service_account`` and
    ``google.auth`` are stubs, ADC failure is the fixture's flag, and the
    file branches use real temp files;
  * ``_release_unadmitted`` / ``_track_outbound_thread`` /
    ``_invalidate_user_creds`` / ``_loop_accepts_callbacks`` - direct, on
    ``__new__`` instances whose stores only record.
"""
import json
import os
import sys
import tempfile
import types
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_97bbcb_p4"

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.google_chat import adapter as live  # noqa: E402

A = live.GoogleChatAdapter

WANTED = {"format_message", "retry_plan", "reconnect_delay", "reconnect_jitter", "bot_spaces",
          "sa_source_kind", "release_unadmitted", "outbound_thread_incr", "invalidate_identity",
          "loop_accepts"}


class Quiet:
    def debug(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass

    def warning(self, *a, **k):
        pass

    def error(self, *a, **k):
        pass


live.logger = Quiet()


def same(py, c):
    if isinstance(py, bool) or isinstance(c, bool):
        return isinstance(py, bool) and isinstance(c, bool) and py == c
    if isinstance(py, float) or isinstance(c, float):
        if not isinstance(py, (int, float)) or not isinstance(c, (int, float)):
            return False
        return abs(float(py) - float(c)) <= 1e-9 * max(1.0, abs(float(py)), abs(float(c)))
    if isinstance(py, dict) and isinstance(c, dict):
        if set(py) != set(c):
            return False
        return all(same(py[k], c[k]) for k in py)
    if isinstance(py, list) and isinstance(c, list):
        return len(py) == len(c) and all(same(x, y) for x, y in zip(py, c))
    return py == c


def retry_plan(req):
    """_call_with_retry's loop arithmetic with the fixture's random() draws."""
    randoms = list(req.get("randoms") or [])
    failures = req.get("failures") or 0
    delay = live._RETRY_BASE_DELAY
    waits, delays = [], []
    exhausted = False
    for attempt in range(1, live._RETRY_MAX_ATTEMPTS + 1):
        if attempt > failures:
            break
        if attempt >= live._RETRY_MAX_ATTEMPTS:
            exhausted = True
            break
        r = randoms[attempt - 1] if len(randoms) >= attempt else 0.0
        waits.append(min(delay + delay * live._RETRY_JITTER * r,
                         live._RETRY_MAX_DELAY + live._RETRY_JITTER))
        delay = min(delay * 2, live._RETRY_MAX_DELAY)
        delays.append(delay)
    return {"waits": waits, "delays": delays, "exhausted": exhausted}


def reconnect_delay(req):
    attempt = req.get("attempt") or 0
    return min(A._RECONNECT_MAX_DELAY, A._RECONNECT_BASE_DELAY * (2 ** (attempt - 1)))


def reconnect_jitter(req):
    """random.uniform(0, delay) body: a + (b - a) * random()."""
    delay = min(A._RECONNECT_MAX_DELAY, A._RECONNECT_BASE_DELAY * (2 ** ((req.get("attempt") or 0) - 1)))
    return 0.0 + (delay - 0.0) * (req.get("rand") or 0.0)


def bot_spaces(req):
    candidates = []
    home = req.get("home")
    if home:
        candidates.append(home)
    bootstrap = req.get("bootstrap") or ""
    if bootstrap:
        candidates.extend(s.strip() for s in bootstrap.split(",") if s.strip())
    return candidates


_GOOGLE_SAVED = {}


def _save_google_once():
    """Snapshot the real google modules the first time this gate touches them.

    Importability of google.auth is a *seam input* of _load_sa_credentials_from,
    not something the fixture may inherit from the host. Dropping the cached
    modules is not enough: the venv has google.auth installed, so `import
    google.auth` succeeds straight off disk and the ADC branch lands on
    `default()` (adc_failed) instead of ImportError (adc_no_auth) -- which made
    this gate pass or fail depending on what another oracle had already imported
    in the same process.
    """
    for key in ("google", "google.auth"):
        if key not in _GOOGLE_SAVED and key in sys.modules:
            _GOOGLE_SAVED[key] = sys.modules[key]


def _install_google_auth(raises):
    _save_google_once()
    pkg = types.ModuleType("google")
    pkg.__path__ = []
    mod = types.ModuleType("google.auth")

    def default(scopes=None):
        if raises:
            raise RuntimeError("no ADC available")
        return ("CREDS_ADC", "proj")

    mod.default = default
    pkg.auth = mod
    sys.modules["google"] = pkg
    sys.modules["google.auth"] = mod


def _drop_google_auth():
    """Force `import google.auth` to raise ImportError, deterministically."""
    _save_google_once()
    sys.modules["google"] = None
    sys.modules["google.auth"] = None


def _restore_google():
    for key in ("google", "google.auth"):
        if key in _GOOGLE_SAVED:
            sys.modules[key] = _GOOGLE_SAVED.pop(key)
        elif key in sys.modules and sys.modules[key] is None:
            del sys.modules[key]


def sa_source_kind(req):
    sa_value = req.get("sa_value")
    orig_sa = live.service_account
    orig_borrow = live._adc_would_borrow_foreign_credentials
    live.service_account = SimpleNamespace(
        Credentials=SimpleNamespace(from_service_account_info=lambda info, scopes=None: "CREDS_SA"))
    live._adc_would_borrow_foreign_credentials = lambda: bool(req.get("multiplex")) and bool(req.get("env_has_sa"))
    tmpdir = None
    path = sa_value
    source = None
    try:
        if sa_value:
            if str(sa_value).lstrip().startswith("{"):
                source = "inline"
            elif not req.get("exists"):
                path = os.path.join(tempfile.gettempdir(), "gchat_sa_does_not_exist.json")
            else:
                source = "file"
                tmpdir = tempfile.mkdtemp(prefix="gchat_sa_")
                path = os.path.join(tmpdir, "sa.json")
                with open(path, "w", encoding="utf-8") as fh:
                    fh.write('{"type": "service_account"}\n' if req.get("file_valid") else "{not json\n")
        else:
            if req.get("has_google_auth"):
                _install_google_auth(bool(req.get("adc_raises")))
                source = "adc"
            else:
                _drop_google_auth()
        try:
            live._load_sa_credentials_from(path)
        except live._SACredentialError as err:
            return {"source": None, "kind": err.kind}
        return {"source": source, "kind": "creds"}
    finally:
        live.service_account = orig_sa
        live._adc_would_borrow_foreign_credentials = orig_borrow
        _restore_google()
        if tmpdir:
            for name in os.listdir(tmpdir):
                os.unlink(os.path.join(tmpdir, name))
            os.rmdir(tmpdir)


class Dedup:
    def __init__(self):
        self.discarded = []

    def discard(self, msg_id):
        self.discarded.append(msg_id)


class Store:
    def __init__(self):
        self.incrs = []
        self.decrs = []

    def incr(self, chat_id, thread_name):
        self.incrs.append((chat_id, thread_name))
        return 0

    def decr(self, chat_id, thread_name):
        self.decrs.append((chat_id, thread_name))


def release_unadmitted(req):
    o = A.__new__(A)
    o._dedup = Dedup()
    o._thread_count_store = Store()
    A._release_unadmitted(o, req.get("msg") or {}, req.get("envelope") or {})
    decr = o._thread_count_store.decrs
    return {
        "dedup": o._dedup.discarded[0] if o._dedup.discarded else "",
        "decr": {"chat": decr[0][0], "thread": decr[0][1]} if decr else None,
    }


def outbound_thread_incr(req):
    o = A.__new__(A)
    o._thread_count_store = Store()
    A._track_outbound_thread(o, req.get("chat_id") or "", req.get("resp") or {})
    incr = o._thread_count_store.incrs
    return {"chat": incr[0][0], "thread": incr[0][1]} if incr else None


def invalidate_identity(req):
    identity = req.get("identity")
    o = A.__new__(A)
    o._user_credentials = "creds"
    o._user_chat_api = "api"
    o._user_creds_by_email = {"a@b.c": "c"}
    o._user_chat_api_by_email = {"a@b.c": "a"}
    A._invalidate_user_creds(o, identity)
    legacy_cleared = o._user_credentials is None and o._user_chat_api is None
    email_cleared = "a@b.c" not in o._user_creds_by_email and "a@b.c" not in o._user_chat_api_by_email
    if legacy_cleared:
        return "legacy"
    if email_cleared:
        return "email"
    return "none"


def loop_accepts(req):
    loop = None if not req.get("present") else SimpleNamespace(is_closed=lambda: bool(req.get("closed")))
    return bool(A._loop_accepts_callbacks(loop))


def expected(r):
    f = r.get("func")
    if f == "format_message":
        return A.format_message(r.get("content") or "")
    if f == "retry_plan":
        return retry_plan(r)
    if f == "reconnect_delay":
        return reconnect_delay(r)
    if f == "reconnect_jitter":
        return reconnect_jitter(r)
    if f == "bot_spaces":
        return bot_spaces(r)
    if f == "sa_source_kind":
        return sa_source_kind(r)
    if f == "release_unadmitted":
        return release_unadmitted(r)
    if f == "outbound_thread_incr":
        return outbound_thread_incr(r)
    if f == "invalidate_identity":
        return invalidate_identity(r)
    if f == "loop_accepts":
        return loop_accepts(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        if not same(py, r.get("out")):
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, r.get("out")), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
