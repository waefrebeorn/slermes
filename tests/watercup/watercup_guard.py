"""Comprehensive watercup runner guard.

Blocks ALL interactive-flow paths so the trace-cup oracle can run project-wide
without ever launching a browser, spawning a child process, or touching a
live, interactive hermes CLI.

Patched entry points (in priority order):

1. hermes CLI interactive flows  — approvals / kanban / curator / hello / OIDC
2. hermes CLI subprocess spawns   — hermes CLI shells spawn subprocesses
3. web-research tool flows        — DuckDuckGo / Brave / Google CSE HTTP
4. browser-spawn paths            — launch_chrome_debug, webbrowser.open,
                                    _real_profile_cdp, subprocess.Popen for browsers
"""

import os
import subprocess
import sys
import threading

# ---------------------------------------------------------------------------
# 1. hermes CLI interactive flows
# ---------------------------------------------------------------------------

_HERMES_CLI_BLOCKED = False
_HERMES_CLI_LOCK = threading.Lock()


def _install_hermes_cli_guard():
    global _HERMES_CLI_BLOCKED
    with _HERMES_CLI_LOCK:
        if _HERMES_CLI_BLOCKED:
            return
        _HERMES_CLI_BLOCKED = True

    patches = []
    try:
        import hermes_cli.approval as _app
        patches.append(_app)
    except Exception:
        pass
    try:
        import hermes_cli.kanban_boards as _kb
        patches.append(_kb)
    except Exception:
        pass
    try:
        import hermes_cli.curator as _cur
        patches.append(_cur)
    except Exception:
        pass
    try:
        import hermes_cli.hello as _hello
        patches.append(_hello)
    except Exception:
        pass
    try:
        import hermes_cli.oidc as _oidc
        patches.append(_oidc)
    except Exception:
        pass

    for _mod in patches:
        for _name in dir(_mod):
            if _name.startswith("_"):
                continue
            try:
                _obj = getattr(_mod, _name)
                if callable(_obj):
                    setattr(_mod, _name, lambda *a, **k: None)
            except Exception:
                pass


# ---------------------------------------------------------------------------
# 2. hermes CLI subprocess spawns
# ---------------------------------------------------------------------------

class _HermesCLIGuard:
    _orig_popen = None
    _installed = False
    _lock = threading.Lock()

    def _install(self):
        with self._lock:
            if self._installed:
                return
            self._orig_popen = subprocess.Popen
            self._installed = True

        HERME_CRITERIA = {"hermes", "hermes-agent", "hermes_cli", "hermes_slave"}

        def _guarded_popen(args, **kwargs):
            cmd = args[0] if isinstance(args, (list, tuple)) else args
            base = os.path.basename(str(cmd)) if isinstance(cmd, str) else ""
            if base.split(".")[0] in HERME_CRITERIA:
                class _NoOp:
                    def __init__(self, _a, **_k):
                        self.returncode = 0

                    def poll(self):
                        return self.returncode

                    def wait(self):
                        return self.returncode

                    def communicate(self, *a, **k):
                        return "", ""

                    def stdout(self):
                        return None

                    def stderr(self):
                        return None

                    def pid(self):
                        return 0

                    def kill(self):
                        pass

                    def __enter__(self):
                        return self

                    def __exit__(self, *a):
                        return False

                return _NoOp(args, **kwargs)
            return self._orig_popen(args, **kwargs)

        subprocess.Popen = _guarded_popen


# ---------------------------------------------------------------------------
# 3. web-research tool flows  (DuckDuckGo / Brave / Google CSE)
# ---------------------------------------------------------------------------

class _WebResearchGuard:
    _installed = False
    _lock = threading.Lock()

    def _install(self):
        with self._lock:
            if self._installed:
                return
            self._installed = True

        try:
            import hermes_cli.web as _web
            for _name in ["search_duckduckgo", "search_brave", "search_google"]:
                if hasattr(_web, _name):
                    setattr(_web, _name, lambda *a, **k: [])
        except Exception:
            pass

        try:
            import hermes_http as _http
        except Exception:
            _http = None

        if _http is not None:
            try:
                _orig = getattr(_http, "http_request", None)
                if _orig is not None:
                    def _guarded(method=None, url=None, *a, **k):
                        return None

                    _http.http_request = _guarded
            except Exception:
                pass


# ---------------------------------------------------------------------------
# 4. browser-spawn paths
# ---------------------------------------------------------------------------

class _BrowserGuard:
    _orig_popen = None
    _installed = False
    _lock = threading.Lock()

    def _install(self):
        with self._lock:
            if self._installed:
                return
            self._orig_popen = subprocess.Popen
            self._installed = True

        BROWSER_BINARIES = {
            "google-chrome", "google-chrome-stable", "chromium", "chromium-browser",
            "chrome", "brave", "brave-browser", "brave-browser-stable", "brave-origin",
            "brave-origin-nightly", "microsoft-edge", "microsoft-edge-stable", "msedge",
        }

        def _guarded_popen(args, **kwargs):
            cmd = args[0] if isinstance(args, (list, tuple)) else args
            base = os.path.basename(str(cmd)) if isinstance(cmd, str) else ""
            name = base.lower().replace(".exe", "")
            if name in BROWSER_BINARIES:
                class _NoOp:
                    def __init__(self, _a, **_k):
                        self.returncode = 0

                    def poll(self):
                        return self.returncode

                    def wait(self):
                        return self.returncode

                    def communicate(self, *a, **k):
                        return "", ""

                    def stdout(self):
                        return None

                    def stderr(self):
                        return None

                    def pid(self):
                        return 0

                    def kill(self):
                        pass

                    def __enter__(self):
                        return self

                    def __exit__(self, *a):
                        return False

                return _NoOp(args, **kwargs)
            return self._orig_popen(args, **kwargs)

        subprocess.Popen = _guarded_popen


# ---------------------------------------------------------------------------
# public API
# ---------------------------------------------------------------------------

_guards = [
    _install_hermes_cli_guard,
    _HermesCLIGuard()._install,
    _WebResearchGuard()._install,
    _BrowserGuard()._install,
]
_installed = []


def install():
    """Install every guard. Safe to call many times (idempotent)."""
    for _fn in _guards:
        try:
            _fn()
            _installed.append(_fn)
        except Exception:
            pass


def installed():
    return len(_installed) > 0


if __name__ == "__main__":
    install()
    print("[watercup_guard] interactive-flow guards installed:", installed())
