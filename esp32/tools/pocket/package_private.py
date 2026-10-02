#!/usr/bin/env python3
"""Create a private app-only image without putting the SDK token in build files."""
import argparse
import hashlib
import getpass
import os
from pathlib import Path
import re
import struct
import sys
import warnings

MARKER = b"MUSE_POCKET_TOKEN_SLOT_V1"
SLOT_SIZE = 128
SLOT = MARKER + bytes(SLOT_SIZE - len(MARKER))


def image_layout(data):
    """Validate the S3 app checksum and digest before any mutation."""
    if len(data) < 24 or data[0] != 0xE9:
        raise ValueError("Not an ESP application image")
    if struct.unpack_from("<H", data, 12)[0] != 9:
        raise ValueError("This image is not for ESP32-S3")
    if not 1 <= data[1] <= 16 or data[23] != 1:
        raise ValueError("Unsupported segment count or missing application digest")
    cursor, checksum, segments = 24, 0xEF, []
    for _ in range(data[1]):
        if cursor + 8 > len(data):
            raise ValueError("Truncated segment header")
        size = struct.unpack_from("<I", data, cursor + 4)[0]
        cursor += 8
        end = cursor + size
        if end > len(data):
            raise ValueError("Truncated segment")
        segments.append((cursor, end))
        for value in data[cursor:end]:
            checksum ^= value
        cursor = end
    checksum_offset = cursor + (15 - cursor % 16)
    digest_offset = checksum_offset + 1
    if digest_offset + 32 != len(data):
        raise ValueError("Unexpected trailing data; only an unsigned app image is accepted")
    if data[checksum_offset] != checksum:
        raise ValueError("Application checksum is invalid")
    if hashlib.sha256(data[:digest_offset]).digest() != data[digest_offset:]:
        raise ValueError("Application digest is invalid")
    return segments, checksum_offset, digest_offset


def package(data, token):
    """Patch one reserved field, preserving the app geometry and every header."""
    if not re.fullmatch(r"mgst_[A-Za-z0-9_-]{42}[AEIMQUYcgkosw048]", token):
        raise ValueError("Not a canonical Muse SDK token")
    segments, checksum_offset, digest_offset = image_layout(data)
    if data.count(SLOT) != 1:
        raise ValueError("Expected exactly one unmodified Muse Pocket token slot")
    offset = data.index(SLOT)
    if not any(start <= offset and offset + SLOT_SIZE <= end for start, end in segments):
        raise ValueError("Token slot is outside application data")
    out = bytearray(data)
    encoded = token.encode("ascii")
    out[offset:offset + SLOT_SIZE] = encoded + bytes(SLOT_SIZE - len(encoded))
    checksum = 0xEF
    for start, end in segments:
        for value in out[start:end]:
            checksum ^= value
    out[checksum_offset] = checksum
    out[digest_offset:] = hashlib.sha256(out[:digest_offset]).digest()
    image_layout(out)
    return bytes(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--token-stdin", action="store_true",
                        help="Read the SDK token from standard input instead of a hidden prompt")
    args = parser.parse_args()
    # Never put the token in command arguments, environment files or build config.
    if args.token_stdin:
        token = sys.stdin.read().strip()
    else:
        try:
            with warnings.catch_warnings():
                warnings.simplefilter("error", getpass.GetPassWarning)
                token = getpass.getpass("Muse SDK token (hidden): ").strip()
        except (getpass.GetPassWarning, EOFError):
            raise SystemExit("A private terminal is required; for a pipe use --token-stdin") from None
    try:
        private = package(args.input.read_bytes(), token)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive create prevents replacing an existing private build by accident.
    fd = os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "wb") as output:
        output.write(private)
    print(f"Private app image: {args.output}")
    print(f"Size: {len(private)} bytes; SHA256: {hashlib.sha256(private).hexdigest()}")


if __name__ == "__main__":
    main()
