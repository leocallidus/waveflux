#!/usr/bin/env python3
"""Regenerate the synthetic CC0 tracker corpus used by WaveFlux tests."""

from __future__ import annotations

import json
import struct
from pathlib import Path


BASE = Path(__file__).resolve().parent


def fixed(value: str, width: int) -> bytes:
    data = value.encode("ascii")[:width]
    return data + (b"\0" * (width - len(data)))


def be16(value: int) -> bytes:
    return struct.pack(">H", value)


def le16(value: int) -> bytes:
    return struct.pack("<H", value)


def le32(value: int) -> bytes:
    return struct.pack("<I", value)


def create_mod(title: str, sample_len_words: int = 2, rows: int = 64, note: bool = True) -> bytes:
    data = bytearray()
    data += fixed(title, 20)

    for sample_index in range(31):
        has_sample = sample_index == 0 and sample_len_words > 0
        data += fixed("Pulse" if has_sample else "", 22)
        data += be16(sample_len_words if has_sample else 0)
        data += bytes([0, 64 if has_sample else 0])
        data += be16(0)
        data += be16(1 if has_sample else 0)

    data += bytes([1, 127])
    data += bytes(128)
    data += b"M.K."

    pattern = bytearray(rows * 4 * 4)
    if note and sample_len_words:
        pattern[0] = 0x01
        pattern[1] = 0xAC
        pattern[2] = 0x10
        pattern[3] = 0x00
    data += pattern

    if sample_len_words:
        data += bytes([0, 64, 192, 0])
    return bytes(data)


def create_669(title: str) -> bytes:
    data = bytearray()
    data += b"if"
    data += fixed(title, 108)
    data += bytes([0, 1, 0])
    data += bytes([0] + [0xFF] * 127)
    data += bytes([6] * 128)
    data += bytes([63] * 128)
    data += bytes([0xFF, 0xFF, 0xFF]) * (64 * 8)
    return bytes(data)


def write_fixtures() -> None:
    (BASE / "tiny.mod").write_bytes(create_mod("WaveFlux MOD Tiny"))
    (BASE / "tiny.669").write_bytes(create_669("WaveFlux 669 Tiny"))
    (BASE / "stress-short-silent.mod").write_bytes(
        create_mod("WF Short Silent", sample_len_words=0, note=False)
    )

    long_module = bytearray(create_mod("WF Long Loopish"))
    order_count_offset = 20 + 31 * 30
    long_module[order_count_offset] = 8
    for index in range(8):
        long_module[order_count_offset + 2 + index] = 0
    (BASE / "stress-long-loopish.mod").write_bytes(bytes(long_module))

    seek_module = bytearray(create_mod("WF Seek Stress"))
    pattern_offset = 20 + 31 * 30 + 2 + 128 + 4
    for row in range(0, 64, 8):
        offset = pattern_offset + row * 16
        seek_module[offset] = 0x01
        seek_module[offset + 1] = 0xAC
        seek_module[offset + 2] = 0x10
        seek_module[offset + 3] = 0x00
    (BASE / "stress-rapid-seek.mod").write_bytes(bytes(seek_module))

    xm = bytearray()
    xm += b"Extended Module: "
    xm += fixed("WaveFlux XM Tiny", 20)
    xm += b"\x1a"
    xm += fixed("WaveFlux", 20)
    xm += le16(0x0104)
    xm += le32(276)
    xm += le16(1)
    xm += le16(0)
    xm += le16(2)
    xm += le16(1)
    xm += le16(0)
    xm += le16(0)
    xm += le16(6)
    xm += le16(125)
    xm += bytes(256)
    xm += le32(9) + bytes([0]) + le16(64) + le16(0)
    (BASE / "tiny.xm").write_bytes(bytes(xm))

    s3m = bytearray()
    s3m += fixed("WaveFlux S3M Tiny", 28)
    s3m += bytes([0x1A, 0x10])
    s3m += le16(0)
    s3m += le16(1) + le16(0) + le16(1)
    s3m += le16(0) + le16(0x1300) + le16(1)
    s3m += b"SCRM"
    s3m += bytes([64, 6, 125, 0xFC, 0, 0, 0, 0])
    s3m += le16(0) + le32(0)
    s3m += bytes([0, 1] + [0xFF] * 30)
    s3m += bytes([0])
    while len(s3m) < 7 * 16:
        s3m += b"\0"
    s3m[97:99] = le16(7)
    s3m += le16(66) + bytes([0] * 64)
    (BASE / "tiny.s3m").write_bytes(bytes(s3m))

    it = bytearray()
    it += b"IMPM"
    it += fixed("WaveFlux IT Tiny", 26)
    it += bytes([0, 0])
    it += le16(1) + le16(0) + le16(0) + le16(1)
    it += le16(0x0217) + le16(0x0200)
    it += le16(0) + le16(0)
    it += bytes([128, 48, 6, 125, 128, 0])
    it += le16(0) + le32(0) + le32(0)
    it += bytes([32] * 64)
    it += bytes([64] * 64)
    it += bytes([0])
    pattern_offset_table = 192 + 1
    pattern_data_offset = pattern_offset_table + 4
    it += le32(pattern_data_offset)
    it += le16(0) + le16(64) + le32(0)
    (BASE / "tiny.it").write_bytes(bytes(it))


def write_manifest() -> None:
    manifest = {
        "version": 1,
        "generatedBy": "tests/testdata/tracker/generate_tracker_fixtures.py",
        "license": "CC0-1.0",
        "source": "Generated synthetic tracker fixtures for WaveFlux regression tests; no third-party samples.",
        "fixtures": [
            {"file": "tiny.mod", "format": "mod", "role": "format", "license": "CC0-1.0", "source": "generated", "expectedTitle": "WaveFlux MOD Tiny", "expectedDurationMs": 7680, "expectedChannels": 4, "expectedPatterns": 1, "stress": False},
            {"file": "tiny.xm", "format": "xm", "role": "format", "license": "CC0-1.0", "source": "generated", "expectedTitle": "WaveFlux XM Tiny", "expectedDurationMs": 7680, "expectedChannels": 2, "expectedPatterns": 1, "stress": False},
            {"file": "tiny.s3m", "format": "s3m", "role": "format", "license": "CC0-1.0", "source": "generated", "expectedTitle": "WaveFlux S3M Tiny", "expectedDurationMs": 7680, "expectedChannels": 32, "expectedPatterns": 1, "stress": False},
            {"file": "tiny.it", "format": "it", "role": "format", "license": "CC0-1.0", "source": "generated", "expectedTitle": "WaveFlux IT Tiny", "expectedDurationMs": 7680, "expectedChannels": 1, "expectedPatterns": 1, "stress": False},
            {"file": "tiny.669", "format": "669", "role": "format", "license": "CC0-1.0", "source": "generated", "expectedTitle": "WaveFlux 669 Tiny", "expectedDurationMs": 12303, "expectedChannels": 8, "expectedPatterns": 1, "stress": False},
            {"file": "stress-short-silent.mod", "format": "mod", "role": "very-short-silent", "license": "CC0-1.0", "source": "generated", "expectedTitle": "WF Short Silent", "expectedDurationMs": 7680, "expectedChannels": 4, "expectedPatterns": 1, "stress": True},
            {"file": "stress-long-loopish.mod", "format": "mod", "role": "long-looping-like", "license": "CC0-1.0", "source": "generated", "expectedTitle": "WF Long Loopish", "expectedDurationMs": 61439, "expectedChannels": 4, "expectedPatterns": 1, "stress": True},
            {"file": "stress-rapid-seek.mod", "format": "mod", "role": "rapid-seek", "license": "CC0-1.0", "source": "generated", "expectedTitle": "WF Seek Stress", "expectedDurationMs": 7680, "expectedChannels": 4, "expectedPatterns": 1, "stress": True},
        ],
    }
    (BASE / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    write_fixtures()
    write_manifest()
