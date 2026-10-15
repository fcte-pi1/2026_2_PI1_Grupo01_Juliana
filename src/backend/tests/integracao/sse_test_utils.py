"""Utilities for reading SSE frames in integration tests (BACK-07)."""

import json


def parse_sse_frames(body: str) -> list[dict]:
    """Parse SSE frames, excluding comment lines such as keep-alives."""
    frames = []
    for raw_frame in body.split("\n\n"):
        lines = [line for line in raw_frame.split("\n") if line and not line.startswith(":")]
        if not lines:
            continue
        frame: dict = {}
        for line in lines:
            if line.startswith("id: "):
                frame["id"] = int(line.removeprefix("id: "))
            elif line.startswith("event: "):
                frame["event"] = line.removeprefix("event: ")
            elif line.startswith("data: "):
                frame["data"] = json.loads(line.removeprefix("data: "))
        frames.append(frame)
    return frames


def count_keep_alives(body: str) -> int:
    return body.count(": keep-alive")