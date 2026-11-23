/*
 * webapp_previews.c — faithful C11 port of upstream Hermes
 * hermes_cli/webapp_previews.py (wave14 gate w14_webappprev_71707e).
 *
 * The module's substance is its CSP/HTML constants and the three preview
 * asset routes they back; getters expose those constants verbatim and
 * route_manifest reproduces the registered paths, headers and media types.
 *
 * PoP: w14_webappprev_71707e_preview_policy @ hermes_cli/webapp_previews.py:PREVIEW_POLICY
 * PoP: w14_webappprev_71707e_guest_policy @ hermes_cli/webapp_previews.py:GUEST_POLICY
 * PoP: w14_webappprev_71707e_preview_html @ hermes_cli/webapp_previews.py:PREVIEW_HTML
 * PoP: w14_webappprev_71707e_preview_loader @ hermes_cli/webapp_previews.py:PREVIEW_LOADER
 * PoP: w14_webappprev_71707e_route_manifest @ hermes_cli/webapp_previews.py:mount_preview_assets
 */
#include "hermes_json.h"
#include <stdlib.h>

#define PREVIEW_HTML   "<!doctype html><html><head><meta charset=\"utf-8\">\n<style>html,body,iframe{width:100%;height:100%;margin:0;border:0;display:block}</style>\n</head><body><script>\nconst frame = document.createElement('iframe');\nframe.referrerPolicy = 'no-referrer';\nframe.title = 'Preview content';\nconst transferableTypes = [\n  'MessagePort', 'ReadableStream', 'WritableStream', 'TransformStream', 'ImageBitmap',\n  'OffscreenCanvas', 'VideoFrame', 'AudioData', 'ArrayBuffer', 'MediaStreamTrack', 'RTCDataChannel'\n].map(name => globalThis[name]).filter(type => typeof type === 'function');\n// An explicit stack: a deeply nested payload that cloning accepts must not\n// overflow the call stack. Children are pushed in reverse to keep their order.\nfunction transfersFor(event) {\n  const transfers = new Set(event.ports);\n  const seen = new Set();\n  const pending = [event.data];\n  while (pending.length) {\n    const value = pending.pop();\n    if (!value || typeof value !== 'object' || seen.has(value)) continue;\n    seen.add(value);\n    let children = [];\n    if (transferableTypes.some(type => value instanceof type)) {\n      transfers.add(value);\n    } else if (value instanceof Map) {\n      value.forEach((item, key) => children.push(key, item));\n    } else if (value instanceof Set) {\n      children = [...value];\n    } else if (value instanceof Error) {\n      children = [value.cause, value.errors];\n    } else if (Array.isArray(value) || Object.getPrototypeOf(value) === Object.prototype ||\n               Object.getPrototypeOf(value) === null) {\n      children = Object.values(value);\n    }\n    for (let index = children.length - 1; index >= 0; index--) pending.push(children[index]);\n  }\n  return [...transfers];\n}\n// A missing target (no guest frame yet) fails both posts, so its ports are closed.\nfunction relay(target, event) {\n  let transfers = [...event.ports];\n  try {\n    transfers = transfersFor(event);\n    target.postMessage(event.data, '*', transfers);\n  } catch {\n    try {\n      target.postMessage(event.data, '*');\n    } catch {\n      for (const value of transfers) {\n        if (typeof globalThis.MessagePort === 'function' && value instanceof MessagePort) value.close();\n      }\n    }\n  }\n}\naddEventListener('message', event => {\n  if (event.source === parent && parent !== window &&\n      event.origin === new URL(location.href).origin &&\n      event.data?.type === 'hermes-preview-document' && typeof event.data.html === 'string') {\n    const guest = typeof event.data.sandbox === 'string';\n    frame.setAttribute('sandbox', guest ? event.data.sandbox : event.data.scripts === true ? 'allow-scripts' : '');\n    // A plugin's onLoad means its guest document loaded, not this shell.\n    frame.onload = guest ? () => parent.postMessage({type: 'hermes-guest-loaded'}, '*') : null;\n    frame.srcdoc = event.data.html;\n    document.body.append(frame);\n  } else if (event.source === parent && event.origin === new URL(location.href).origin) {\n    relay(frame.contentWindow, event);\n  } else if (event.source === frame.contentWindow) {\n    relay(parent, event);\n  }\n});\n</script></body></html>"
#define PREVIEW_LOADER "const frame = document.querySelector('iframe[data-preview-html]');\nfunction send() {\n  frame.contentWindow.postMessage({type: 'hermes-preview-document',\n    html: frame.dataset.previewHtml, scripts: true}, '*');\n}\nframe.addEventListener('load', send);\nsend();\n"
#define PREVIEW_POLICY "default-src 'none'; script-src * data: blob: 'unsafe-inline' 'unsafe-eval' 'wasm-unsafe-eval'; style-src * data: 'unsafe-inline'; img-src * data: blob:; font-src * data:; media-src * data: blob:; connect-src * data: blob:; worker-src * data: blob:; frame-src * data: blob: about:; object-src 'none'; base-uri * about:; frame-ancestors 'self'; sandbox allow-scripts; form-action 'none'"
#define GUEST_POLICY   "default-src 'none'; script-src * data: blob: 'unsafe-inline' 'unsafe-eval' 'wasm-unsafe-eval'; style-src * data: 'unsafe-inline'; img-src * data: blob:; font-src * data:; media-src * data: blob:; connect-src * data: blob:; worker-src * data: blob:; frame-src * data: blob: about:; object-src 'none'; base-uri * about:; frame-ancestors 'self'; sandbox allow-downloads allow-forms allow-orientation-lock allow-pointer-lock allow-presentation allow-scripts; form-action *"
#define GUEST_SANDBOX  "allow-downloads allow-forms allow-orientation-lock allow-pointer-lock allow-presentation allow-scripts"

struct json_t *w14_webappprev_71707e_preview_policy(void) {
    return json_string(PREVIEW_POLICY);
}
struct json_t *w14_webappprev_71707e_guest_policy(void) {
    return json_string(GUEST_POLICY);
}
struct json_t *w14_webappprev_71707e_preview_html(void) {
    return json_string(PREVIEW_HTML);
}
struct json_t *w14_webappprev_71707e_preview_loader(void) {
    return json_string(PREVIEW_LOADER);
}

static struct json_t *doc_headers(const char *csp) {
    struct json_t *h = json_object();
    json_set(h, "Cache-Control", json_string("no-store"));
    json_set(h, "Content-Security-Policy", json_string(csp));
    json_set(h, "Referrer-Policy", json_string("no-referrer"));
    json_set(h, "X-Content-Type-Options", json_string("nosniff"));
    return h;
}

/* The three routes mounted by mount_preview_assets(application). */
struct json_t *w14_webappprev_71707e_route_manifest(void) {
    struct json_t *arr = json_array();
    struct json_t *r;

    r = json_object();
    json_set(r, "path", json_string("/assets/webapp-preview.html"));
    json_set(r, "content", json_string(PREVIEW_HTML));
    json_set(r, "headers", doc_headers(PREVIEW_POLICY));
    json_set(r, "media_type", json_null());
    json_append(arr, r);

    r = json_object();
    json_set(r, "path", json_string("/assets/webapp-guest.html"));
    json_set(r, "content", json_string(PREVIEW_HTML));
    json_set(r, "headers", doc_headers(GUEST_POLICY));
    json_set(r, "media_type", json_null());
    json_append(arr, r);

    r = json_object();
    json_set(r, "path", json_string("/assets/webapp-preview.js"));
    json_set(r, "content", json_string(PREVIEW_LOADER));
    {
        struct json_t *h = json_object();
        json_set(h, "Cache-Control", json_string("no-store"));
        json_set(h, "X-Content-Type-Options", json_string("nosniff"));
        json_set(r, "headers", h);
    }
    json_set(r, "media_type", json_string("text/javascript"));
    json_append(arr, r);

    return arr;
}
