#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 GXDE OS Team
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fake OpenAI compatible streaming endpoint used by tests/chattest.cpp.

First request streams a tool call for the ``echo`` MCP tool; once the request
contains a ``tool`` message it streams the final answer.
"""

import json
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(length) or b"{}")
        messages = body.get("messages", [])
        has_tool_result = any(message.get("role") == "tool" for message in messages)

        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.end_headers()

        if has_tool_result:
            delta = {"choices": [{"index": 0, "delta": {"content": "工具返回：echo: hello"},
                                  "finish_reason": None}]}
        else:
            delta = {"choices": [{
                "index": 0,
                "delta": {
                    "role": "assistant",
                    "tool_calls": [{
                        "index": 0,
                        "id": "call_1",
                        "type": "function",
                        "function": {"name": "echo",
                                     "arguments": json.dumps({"text": "hello"})},
                    }],
                },
                "finish_reason": None,
            }]}

        self.wfile.write(("data: " + json.dumps(delta) + "\n\n").encode())
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()

    def log_message(self, *args):
        pass


server = HTTPServer(("127.0.0.1", 0), Handler)
print("PORT", server.server_port, flush=True)
server.serve_forever()
