#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 GXDE OS Team
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tiny stdio MCP server used by tests/mcptest.cpp."""

import json
import sys


def reply(message, result):
    print(json.dumps({"jsonrpc": "2.0", "id": message["id"], "result": result}), flush=True)


for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    message = json.loads(line)
    method = message.get("method")

    if method == "initialize":
        reply(message, {
            "protocolVersion": "2024-11-05",
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "test-server", "version": "0.1"},
        })
    elif method == "tools/list":
        reply(message, {
            "tools": [{
                "name": "echo",
                "description": "Echo the given text back",
                "inputSchema": {
                    "type": "object",
                    "properties": {"text": {"type": "string"}},
                    "required": ["text"],
                },
            }]
        })
    elif method == "tools/call":
        arguments = message.get("params", {}).get("arguments", {})
        reply(message, {
            "content": [{"type": "text", "text": "echo: " + str(arguments.get("text", ""))}]
        })
    elif "id" in message:
        reply(message, {})
