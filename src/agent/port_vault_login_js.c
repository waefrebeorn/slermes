/*
 * port_vault_login_js.c — Port of Python: agent/vault_login_classifier.py
 * (the two JS builders).
 *
 * GENERATED FILE — do not hand-edit. `scripts/gen_vault_login_js.py` emits the
 * templates byte-for-byte from the LIVE Python module (backslashes and
 * json.dumps spacing are the contract). Regenerate after any Python change.
 */

#include "port_vault_login_classifier.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char INSPECTION_TEMPLATE[] =
    "(() => {\n"
    "  const nonce = __NONCE__;\n"
    "  // Shadow-piercing collect: many logins (Home Assistant, custom-element SPAs) render their\n"
    "  // form inside a shadow root, where document.querySelectorAll never sees it. Walk every\n"
    "  // element (shadow hosts can be anything, not just inputs) and recurse into open shadow roots.\n"
    "  // Forms are collected in the same walk: document.forms misses a <form> inside a shadow root, which\n"
    "  // would leave its controls ungrouped (formIndex null) and merge split OTP boxes across forms.\n"
    "  const elements = [];\n"
    "  const forms = [];\n"
    "  (function walk(root) {\n"
    "    for (const e of root.querySelectorAll(\"*\")) {\n"
    "      if (e.tagName === \"INPUT\" || e.tagName === \"SELECT\") elements.push(e);\n"
    "      else if (e.tagName === \"FORM\") forms.push(e);\n"
    "      if (e.shadowRoot) walk(e.shadowRoot);\n"
    "    }\n"
    "  })(document);\n"
    "  elements.forEach((element, index) => element.setAttribute(\"data-hermes-vault-slot\", nonce + \":\" + index));\n"
    "  const out = elements.flatMap((element, index) => {\n"
    "    if (element.disabled || element.readOnly) return [];\n"
    "    if ([\"hidden\", \"submit\", \"button\", \"reset\", \"file\", \"image\", \"checkbox\", \"radio\"].includes(element.type)) return [];\n"
    "    const style = getComputedStyle(element);\n"
    "    if (style.display === \"none\" || style.visibility === \"hidden\" || element.getClientRects().length === 0) return [];\n"
    "    const labels = element.labels ? Array.from(element.labels, (l) => l.textContent || \"\") : [];\n"
    "    const ariaText = (element.getAttribute(\"aria-labelledby\") || \"\")\n"
    "      .split(/\\s+/).filter(Boolean)\n"
    "      .map((id) => { const r = element.getRootNode(); const n = (r.getElementById ? r.getElementById(id) : null) || document.getElementById(id); return n ? (n.textContent || \"\") : \"\"; })\n"
    "      .join(\" \");\n"
    "    const resolvedFormIndex = element.form ? forms.indexOf(element.form) : -1;\n"
    "    return [{\n"
    "      autocomplete: element.autocomplete || \"\",\n"
    "      formIndex: resolvedFormIndex >= 0 ? resolvedFormIndex : null,\n"
    "      index,\n"
    "      maxLength: element.maxLength > 0 ? element.maxLength : null,\n"
    "      label: [\n"
    "        ...labels,\n"
    "        element.getAttribute(\"aria-label\") || \"\",\n"
    "        ariaText,\n"
    "        element.getAttribute(\"placeholder\") || \"\",\n"
    "        element.getAttribute(\"title\") || \"\",\n"
    "      ].join(\" \"),\n"
    "      name: [element.name, element.id].join(\" \"),\n"
    "      type: element.tagName === \"SELECT\" ? \"select\" : (element.type || \"\"),\n"
    "    }];\n"
    "  });\n"
    "  return JSON.stringify(out);\n"
    "})()";

static const char FILL_TEMPLATE[] =
    "(() => {\n"
    "  const expectedOrigin = __EXPECTED_ORIGIN__;\n"
    "  if (window.location.origin !== expectedOrigin) {\n"
    "    return JSON.stringify({ refused: \"origin_changed\", found: window.location.origin });\n"
    "  }\n"
    "  const fills = __FILLS__;\n"
    "  const nonce = __NONCE__;\n"
    "  // Stamped controls may live inside a shadow root (see the inspection script's walk): collect the\n"
    "  // document and every open shadow root once, then look each stamp up in those roots.\n"
    "  const roots = [];\n"
    "  (function walk(root) {\n"
    "    roots.push(root);\n"
    "    for (const e of root.querySelectorAll(\"*\")) if (e.shadowRoot) walk(e.shadowRoot);\n"
    "  })(document);\n"
    "  const find = (selector) => roots.flatMap((r) => Array.from(r.querySelectorAll(selector)));\n"
    "  let filled = 0;\n"
    "  const norm = (t) => String(t || \"\").trim().toLowerCase();\n"
    "  for (const f of fills) {\n"
    "    const el = find('[data-hermes-vault-slot=\"' + nonce + ':' + f.index + '\"]')[0];\n"
    "    if (!el || (f.token === \"current-password\" && el.type !== \"password\")) continue;\n"
    "    try {\n"
    "      if (el.tagName === \"SELECT\") {\n"
    "        const want = norm(f.value);\n"
    "        const opt = Array.from(el.options).find((o) => [o.value, o.textContent].some((t) => norm(t) === want || norm(t) === want.replace(/^0/, \"\")));\n"
    "        if (opt) { el.value = opt.value; el.dispatchEvent(new Event(\"change\", { bubbles: true, composed: true })); filled += 1; }\n"
    "        continue;\n"
    "      }\n"
    "      el.focus();\n"
    "      const setter = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, \"value\");\n"
    "      // one-time-code split into single-character boxes: f.value is the slice for THIS box (see build_otp_fills)\n"
    "      if (setter && setter.set) { setter.set.call(el, f.value); } else { el.value = f.value; }\n"
    "      // composed: a component listening on its shadow host must see the value change too.\n"
    "      el.dispatchEvent(new InputEvent(\"input\", { bubbles: true, composed: true, inputType: \"insertText\" }));\n"
    "      el.dispatchEvent(new Event(\"change\", { bubbles: true, composed: true }));\n"
    "      if (el.value.length > 0) filled += 1;\n"
    "    } catch (e) { /* skip */ }\n"
    "  }\n"
    "  find(\"[data-hermes-vault-slot]\").forEach((n) => n.removeAttribute(\"data-hermes-vault-slot\"));\n"
    "  return JSON.stringify({ filled });\n"
    "})()";

/* Python json.dumps(str), ensure_ascii=True, no extra padding. */
static char *js_quote(const char *s)
{
    if (!s) s = "";
    size_t cap = strlen(s) * 8 + 8;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = '"';
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  out[o++] = '\\'; out[o++] = '"';  break;
        case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
        case '\n': out[o++] = '\\'; out[o++] = 'n';  break;
        case '\r': out[o++] = '\\'; out[o++] = 'r';  break;
        case '\t': out[o++] = '\\'; out[o++] = 't';  break;
        case '\b': out[o++] = '\\'; out[o++] = 'b';  break;
        case '\f': out[o++] = '\\'; out[o++] = 'f';  break;
        default:
            if (*p < 0x20) {
                o += (size_t)snprintf(out + o, cap - o, "\\u%04x", *p);
            } else if (*p < 0x80) {
                out[o++] = (char)*p;
            } else {
                /* ensure_ascii: decode UTF-8 and emit \\uXXXX (surrogate pair
                 * for astral planes), exactly like json.dumps. */
                unsigned cp = 0;
                int extra = 0;
                if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; extra = 1; }
                else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; extra = 2; }
                else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; extra = 3; }
                for (int k = 1; k <= extra; k++)
                    cp = (cp << 6) | (p[k] & 0x3F);
                p += extra;              /* the for-loop advances the last byte */
                if (cp > 0xFFFF) {
                    unsigned v = cp - 0x10000;
                    o += (size_t)snprintf(out + o, cap - o, "\\u%04x\\u%04x",
                                          0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF));
                } else {
                    o += (size_t)snprintf(out + o, cap - o, "\\u%04x", cp);
                }
            }
        }
    }
    out[o++] = '"';
    out[o] = '\0';
    return out;
}

/* Replace every occurrence of `needle` with `with`. */
static char *replace_all(const char *text, const char *needle, const char *with)
{
    size_t nl = strlen(needle), wl = strlen(with);
    size_t cap = strlen(text) * 2 + 64;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    const char *p = text;
    while (*p) {
        const char *hit = strstr(p, needle);
        size_t chunk = hit ? (size_t)(hit - p) : strlen(p);
        if (o + chunk + wl + 1 > cap) {
            cap = (o + chunk + wl + 1) * 2;
            char *tmp = (char *)realloc(out, cap);
            if (!tmp) { free(out); return NULL; }
            out = tmp;
        }
        memcpy(out + o, p, chunk);
        o += chunk;
        if (!hit) break;
        memcpy(out + o, with, wl);
        o += wl;
        p = hit + nl;
    }
    out[o] = '\0';
    return out;
}

/* PoP: vl_build_inspection_js @ agent/vault_login_classifier.py:build_inspection_js */
char *vl_build_inspection_js(const char *nonce)
{
    char *quoted = js_quote(nonce);
    if (!quoted) return NULL;
    char *out = replace_all(INSPECTION_TEMPLATE, "__NONCE__", quoted);
    free(quoted);
    return out;
}

/* PoP: vl_build_fill_js @ agent/vault_login_classifier.py:build_fill_js */
char *vl_build_fill_js(const char *fills_json, const char *expected_origin,
                       const char *nonce)
{
    json_t *fills = fills_json ? json_parse(fills_json, NULL) : NULL;
    if (!fills || fills->type != JSON_ARRAY) {
        if (fills) json_free(fills);
        return NULL;
    }

    /* payload = json.dumps([{"index": i, "token": t, "value": v}, ...]) with the
     * default separators and the key order the comprehension produces. */
    size_t cap = 64;
    for (size_t i = 0; i < fills->c.count; i++) {
        json_t *f = fills->c.items[i];
        const char *v = f ? json_get_str(f, "value", "") : "";
        const char *t = f ? json_get_str(f, "token", "") : "";
        cap += strlen(v) * 8 + strlen(t) * 8 + 96;
    }
    char *payload = (char *)malloc(cap);
    if (!payload) { json_free(fills); return NULL; }
    size_t o = 0;
    o += (size_t)snprintf(payload + o, cap - o, "[");
    for (size_t i = 0; i < fills->c.count; i++) {
        json_t *f = fills->c.items[i];
        json_t *idx = f ? json_obj_get(f, "index") : NULL;
        const char *declared = f ? json_get_str(f, "token", NULL) : NULL;
        const char *token = declared ? declared : "current-password";
        const char *value = f ? json_get_str(f, "value", "") : "";
        char *qt = js_quote(token);
        char *qv = js_quote(value);
        o += (size_t)snprintf(payload + o, cap - o,
                              "%s{\"index\": %ld, \"token\": %s, \"value\": %s}",
                              i ? ", " : "",
                              (long)(idx && idx->type == JSON_NUMBER ? idx->num_val : 0),
                              qt ? qt : "\"\"", qv ? qv : "\"\"");
        free(qt);
        free(qv);
    }
    o += (size_t)snprintf(payload + o, cap - o, "]");
    json_free(fills);

    char *q_origin = js_quote(expected_origin);
    char *q_nonce = js_quote(nonce);
    if (!q_origin || !q_nonce) {
        free(q_origin); free(q_nonce); free(payload);
        return NULL;
    }
    char *step1 = replace_all(FILL_TEMPLATE, "__EXPECTED_ORIGIN__", q_origin);
    char *step2 = step1 ? replace_all(step1, "__FILLS__", payload) : NULL;
    char *out = step2 ? replace_all(step2, "__NONCE__", q_nonce) : NULL;
    free(q_origin); free(q_nonce); free(payload); free(step1); free(step2);
    return out;
}
