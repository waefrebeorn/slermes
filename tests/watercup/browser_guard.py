"""
Browser guard for watercup tests.

Patches all browser-spawn paths so importing LIVE Python modules during
watercup runs can never launch a real browser (Chrome, Chromium, Edge,
Brave, etc.). This prevents the 15+ Chrome window explosion that happened
when 22,000 gates imported modules with browser-debug-launch side effects.

Import this module BEFORE any watercup twin imports:
    import tests.watercup.browser_guard  # noqa: F401

Or set WATERCUP_NO_BROWSER=1 in the environment before running wc_run.py /
wc_run_parallel.py (already done in those runners).
"""
import os
import subprocess


def _install_browser_guard():
    """Install no-op stubs for all browser-spawn entry points."""

    # --- 1. hermes_cli.browser_connect.launch_chrome_debug ----------------
    # Already gated by WATERCUP_NO_BROWSER in the source, but double-guard.
    try:
        import hermes_cli.browser_connect as _bc

        def _guarded_launch(port=9222, system=None):
            result = _bc.ChromeDebugLaunch()
            result.attempts.append(
                _bc.LaunchAttempt(
                    binary="killswitch",
                    state="disabled-via-browser-guard",
                )
            )
            return result

        _bc.launch_chrome_debug = _guarded_launch
        _bc.try_launch_chrome_debug = lambda port=9222, system=None: False
    except Exception:
        pass

    # --- 2. webbrowser.open (OAuth/auth flows) --------------------------
    try:
        import webbrowser

        def _guarded_open(url, new=0, autoraise=True):
            return False

        webbrowser.open = _guarded_open
        webbrowser.open_new = lambda url: False
        webbrowser.open_new_tab = lambda url: False
    except Exception:
        pass

    # --- 3. tools.browser_tool._real_profile_cdp ------------------------
    try:
        import tools.browser_tool as _bt

        _bt._real_profile_cdp = lambda: (None, "browser-guard-active")
    except Exception:
        pass

    # --- 4. tools.browser_use_cli._resolve_real_profile_cdp -------------
    try:
        import tools.browser_use_cli as _buc

        _buc._resolve_real_profile_cdp = lambda env, force_local=False: None
    except Exception:
        pass

    # --- 5. subprocess.Popen for known browser binaries ----------------
    _BROWSER_BINARIES = {
        "google-chrome",
        "google-chrome-stable",
        "chromium",
        "chromium-browser",
        "chrome",
        "brave",
        "brave-browser",
        "brave-browser-stable",
        "brave-origin",
        "brave-origin-nightly",
        "microsoft-edge",
        "microsoft-edge-stable",
        "msedge",
    }

    _orig_popen = subprocess.Popen

    class _GuardedPopen(_orig_popen):
        def __init__(self, args, **kwargs):
            if args:
                cmd = args[0] if isinstance(args, list) else args
                base = os.path.basename(cmd) if isinstance(cmd, str) else ""
                name = base.lower().replace(".exe", "")
                if name in _BROWSER_BINARIES:
                    raise OSError(
                        f"browser-guard: blocked Popen({base!r}) "
                        f"during watercup run"
                    )
            super().__init__(args, **kwargs)

    subprocess.Popen = _GuardedPopen


if os.environ.get("WATERCUP_NO_BROWSER"):
    _install_browser_guard()
