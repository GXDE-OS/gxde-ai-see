#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 GXDE OS Team
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fake OpenAI compatible streaming endpoint used by tests/aicoretest.cpp.

GXDE AI Core talks to it like to a real provider.  The answer depends on the
last message:

* a tool result            -> "完成：<tool output>"
* user text with "shell"   -> a run_shell_command call that creates a file
* user text with "mcp"     -> a call of the MCP ``echo`` tool
* user text with "think"   -> reasoning_content first, then "想好了"
* anything else            -> "users=<n> replay=<yes|no>", which tells the
  test whether the session was reused (n grows) or rebuilt from a replay.
"""

import json
from http.server import BaseHTTPRequestHandler, HTTPServer


def text_of(message):
    content = message.get("content")
    if isinstance(content, list):
        return "".join(part.get("text", "") for part in content if isinstance(part, dict))
    return content or ""


def chunk(delta, finish=None):
    return {"choices": [{"index": 0, "delta": delta, "finish_reason": finish}]}


def tool_call(name, arguments):
    return chunk({"role": "assistant", "tool_calls": [{
        "index": 0, "id": "call_1", "type": "function",
        "function": {"name": name, "arguments": json.dumps(arguments)},
    }]}, "tool_calls")


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(length) or b"{}")
        messages = body.get("messages", [])
        tools = [tool.get("function", {}).get("name", "") for tool in body.get("tools", [])]
        last = messages[-1] if messages else {}
        users = [m for m in messages if m.get("role") == "user"]
        last_text = text_of(last)
        # a rebuilt session gets the old conversation in front of the new
        # message: only the final line is what the user just asked
        asked = last_text.strip().splitlines()[-1] if last_text.strip() else ""

        if last.get("role") == "tool":
            chunks = [chunk({"role": "assistant", "content": "完成："}),
                      chunk({"content": last_text[:200]}, "stop")]
        elif "shell" in asked:
            chunks = [tool_call("run_shell_command",
                                {"command": "touch touched_by_aicoretest",
                                 "description": "create a marker file"})]
        elif "mcp" in asked:
            echo = next((name for name in tools if name.endswith("echo")), None)
            if echo:
                chunks = [tool_call(echo, {"text": "hello"})]
            else:
                chunks = [chunk({"role": "assistant",
                                 "content": "no echo tool: " + ",".join(tools)}, "stop")]
        elif "think" in asked:
            chunks = [chunk({"role": "assistant", "reasoning_content": "先想一想"}),
                      chunk({"reasoning_content": "……"}),
                      chunk({"content": "想好了"}, "stop")]
        else:
            replay = any("此前的记录" in text_of(m) for m in users)
            chunks = [chunk({"role": "assistant",
                             "content": "users=%d replay=%s" % (len(users),
                                                                "yes" if replay else "no")},
                            "stop")]

        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.end_headers()
        for item in chunks:
            self.wfile.write(("data: " + json.dumps(item, ensure_ascii=False) + "\n\n").encode())
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()

    def do_GET(self):
        self.send_response(404)
        self.end_headers()

    def log_message(self, *args):
        pass


server = HTTPServer(("127.0.0.1", 0), Handler)
print("PORT", server.server_port, flush=True)
server.serve_forever()
