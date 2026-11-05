"""Loopback OpenAI-compatible stub for verifying the slermes conversation loop.

Returns a fixed completion, then a tool_call on request, so the whole turn --
HTTP request, response parse, tool dispatch, second request -- can be observed
without a real provider or credentials.

Logs every request to stub_requests.log so the test can assert what slermes
actually sent (model, messages, tool schemas).
"""
import json
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

LOG = "stub_requests.log"
STATE = {"n": 0}


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(n).decode("utf-8", "replace")
        STATE["n"] += 1
        turn = STATE["n"]
        try:
            req = json.loads(body)
        except Exception:
            req = {}
        with open(LOG, "a", encoding="utf-8") as f:
            f.write(json.dumps({"turn": turn, "path": self.path,
                                "tools": len(req.get("tools", [])),
                                "messages": len(req.get("messages", [])),
                                "model": req.get("model"),
                                "body_len": len(body),
                                "stream": req.get("stream", False),
                                "body_snippet": body[:300] if body else ""}) + "\n")

        if turn == 1:
            msg = {"role": "assistant", "content": "stub-reply-one"}
            finish = "stop"
        else:
            # Second request should already contain the tool result.
            msg = {"role": "assistant", "content": "stub-reply-final"}
            finish = "stop"

        out = {
            "id": "chatcmpl-stub",
            "object": "chat.completion",
            "created": 0,
            "model": req.get("model", "stub"),
            "choices": [{"index": 0, "message": msg, "finish_reason": finish}],
            "usage": {"prompt_tokens": 1, "completion_tokens": 1,
                      "total_tokens": 2},
        }
        if req.get("stream"):
            # slermes asks for SSE by default; a plain JSON body looks like a
            # dropped stream ("Stream drop: / after 0.0s").
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            for piece in ("stub-", "reply-", "streamed"):
                chunk = {"id": "chatcmpl-stub", "object": "chat.completion.chunk",
                         "created": 0, "model": req.get("model", "stub"),
                         "choices": [{"index": 0, "delta": {"content": piece},
                                      "finish_reason": None}]}
                payload = "data: " + json.dumps(chunk) + "\n\n"
                self.wfile.write(payload.encode())
                self.wfile.flush()
            done = {"id": "chatcmpl-stub", "object": "chat.completion.chunk",
                    "created": 0, "model": req.get("model", "stub"),
                    "choices": [{"index": 0, "delta": {}, "finish_reason": "stop"}]}
            self.wfile.write(("data: " + json.dumps(done) + "\n\n").encode())
            self.wfile.write(b"data: [DONE]\n\n")
            self.wfile.flush()
            return

        data = json.dumps(out).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8899
    HTTPServer(("127.0.0.1", port), H).serve_forever()
