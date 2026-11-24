#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_dashboard.py — gate
w28_webserverd_919ecc_p1 (themes, bootstrap CSS, SPA fallback + rewrites).

The live module is imported BY FILE PATH behind the repo's seam stubs (this
host refuses pydantic's native extension, which the quarry drags in through
fastapi).  Every I/O seam is a fixture value:

  * the config read and the dashboard-themes directory become the
    ``active``/``themes`` inputs of ``_render_active_theme_bootstrap_css``
    (``cfg_get``/``_discover_user_themes`` are replaced for the call);
  * WEB_DIST and its files become the ``index_exists``/``exists``/
    ``samefile``/``is_static``/``font_found`` flags of the SPA ladder;
  * the request headers become ``accept``/``full_path``.

``_parse_theme_layer``, ``_layer_hex``, ``_normalise_theme_definition`` and
``_render_active_theme_bootstrap_css`` are called live.  The slices that exist
only inline inside ``mount_spa``/``_serve_index``/``serve_css`` (the Accept
sniff, the headless token page, the prefix rewrites, the serve_spa ladder) are
mirrored here statement-for-statement, and every constant they use is read
from the live module or derived from a live call, never invented.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_webserverd_919ecc_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

if os.name == "nt" and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)

sys.path.append(str(Path(__file__).resolve().parent))   # stub helper only; must NOT shadow the quarry packages
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()


def _load(name, rel):
    spec = importlib.util.spec_from_file_location(name, QUARRY / rel)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


live = _load("w28_dash_p1_live", "hermes_cli/web_server_dashboard.py")

WANTED = {
    "parse_theme_layer", "layer_hex", "normalise_theme", "bootstrap_css",
    "missing_message", "wants_html", "headless", "root_token_page",
    "prefix_rewrite", "css_url_rewrite", "spa_route", "constants",
}


def parse_theme_layer(r):
    return live._parse_theme_layer(r.get("value"),
                                   r.get("default_hex", "#000000"),
                                   float(r.get("default_alpha", 1.0)))


def layer_hex(r):
    return live._layer_hex(r.get("palette") or {}, r.get("key") or "", r.get("default"))


def normalise_theme(r):
    return live._normalise_theme_definition(r.get("data"))


def bootstrap_css(r):
    """Drive the live renderer with the config read and theme scan replaced."""
    active = r.get("active")
    themes = r.get("themes")
    live.cfg_get = lambda *a, **k: active
    live._discover_user_themes = lambda: themes if isinstance(themes, list) else []
    return live._render_active_theme_bootstrap_css()


def missing_message(r):
    webapp = r.get("ui_surface", "dashboard") == "webapp"
    return ("The Webapp is not built. Run: hermes webapp" if webapp
            else "Frontend not built. Run: pnpm --dir web run build")


def wants_html(r):
    # _bundle_unavailable: if "text/html" not in request.headers.get("accept", "")
    return "text/html" in r.get("accept", "")


def headless(r):
    value = r.get("value")
    if value is None:
        os.environ.pop("HERMES_SERVE_HEADLESS", None)
    else:
        os.environ["HERMES_SERVE_HEADLESS"] = value
    return os.environ.get("HERMES_SERVE_HEADLESS") == "1"


def root_token_page(r):
    serve = r.get("full_path", "") == "" and not (bool(r.get("gated")) or bool(r.get("withheld")))
    return {"serve": serve, "status": 200 if serve else 404,
            "error": None if serve else live._HEADLESS_MSG}


def prefix_rewrite(r):
    html, prefix = r.get("html", ""), r.get("prefix") or ""
    if prefix:
        for attr in ('href="/assets/', 'src="/assets/', 'href="/favicon.ico"', 'href="/fonts/',
                     'href="/ds-assets/', 'src="/ds-assets/'):
            html = html.replace(attr, attr.replace('"/', f'"{prefix}/', 1))
    return html


def css_url_rewrite(r):
    css, prefix = r.get("css", ""), r.get("prefix") or ""
    if prefix:
        for asset_dir in ("/fonts/", "/fonts-terminal/", "/ds-assets/", "/assets/"):
            for quote in ("", '"', "'"):
                css = css.replace(f"url({quote}{asset_dir}", f"url({quote}{prefix}{asset_dir}")
    return css


def spa_route(r):
    full_path = r.get("full_path", "")
    if full_path == "api" or full_path.startswith("api/"):
        return {"kind": "api_404", "detail": f"No such API endpoint: /{full_path}"}
    if (not r.get("html_token") and r.get("file_is_file") and r.get("index_exists")
            and r.get("samefile")):
        return {"kind": "index", "detail": None}
    if full_path and r.get("under_dist") and r.get("exists") and r.get("file_is_file"):
        return {"kind": "file", "detail": None}
    if r.get("is_static"):
        return {"kind": "font", "detail": None} if r.get("font_found") else \
               {"kind": "static_404", "detail": "not found"}
    return {"kind": "index_fallback", "detail": None}


def constants():
    """Every value derived from the live module (defaults via a live render)."""
    base = live._normalise_theme_definition({"name": "probe"})
    palette, typo, layout = base["palette"], base["typography"], base["layout"]
    probe_css = bootstrap_css({"active": "probe", "themes": [{"name": "probe"}]})
    fallback_bg = probe_css.split("--background-base:")[1].split(";")[0]
    fallback_mg = probe_css.split("--midground-base:")[1].split(";")[0]
    return {
        "immutable_cache_control": live._IMMUTABLE_ASSET_CACHE_CONTROL,
        "no_store_cache_control": live._NO_STORE["Cache-Control"],
        "headless_message": live._HEADLESS_MSG,
        "theme_custom_css_max": live._THEME_CUSTOM_CSS_MAX,
        "theme_default_background": palette["background"]["hex"],
        "theme_default_midground": palette["midground"]["hex"],
        "theme_default_foreground": palette["foreground"]["hex"],
        "theme_default_warm_glow": palette["warmGlow"],
        "bootstrap_background_fallback": fallback_bg,
        "bootstrap_midground_fallback": fallback_mg,
        "theme_default_font_sans": typo["fontSans"],
        "theme_default_font_mono": typo["fontMono"],
        "theme_default_base_size": typo["baseSize"],
        "theme_default_line_height": typo["lineHeight"],
        "theme_default_letter_spacing": typo["letterSpacing"],
        "theme_default_radius": layout["radius"],
        "theme_default_density": layout["density"],
        "builtin_themes": [b["name"] for b in live._BUILTIN_DASHBOARD_THEMES],
        "layout_variants": sorted(live._THEME_LAYOUT_VARIANTS),
    }


def expected(r):
    f = r.get("func")
    if f == "parse_theme_layer":
        return parse_theme_layer(r)
    if f == "layer_hex":
        return layer_hex(r)
    if f == "normalise_theme":
        return normalise_theme(r)
    if f == "bootstrap_css":
        return bootstrap_css(r)
    if f == "missing_message":
        return missing_message(r)
    if f == "wants_html":
        return wants_html(r)
    if f == "headless":
        return headless(r)
    if f == "root_token_page":
        return root_token_page(r)
    if f == "prefix_rewrite":
        return prefix_rewrite(r)
    if f == "css_url_rewrite":
        return css_url_rewrite(r)
    if f == "spa_route":
        return spa_route(r)
    if f == "constants":
        return constants()
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
