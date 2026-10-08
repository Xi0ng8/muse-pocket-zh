#!/usr/bin/env python3
"""Sign/verify an app-only Muse image's CrossMux coexistence certificate.

Uses the existing host-side cryptography package. The board value is a signed
build declaration, not a board identity inferred from the standard ESP header.
Private keys are read only from a PEM file path and never printed.
"""
import argparse
from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import re
import struct

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

import package_private

MARKER = b'MUSE_POCKET_COEXIST_CERT_V1'.ljust(32, b'\0')
SLOT_SIZE = 384
SLOT = MARKER + bytes(SLOT_SIZE - len(MARKER))
MAGIC = b'MPXCERT1'
PROJECT = 'muse-gadget'
BOARD = 'xteink_x4_pro'
PAYLOAD_FORMAT = struct.Struct('<8s7I32s32s16s32s32sI')
PAYLOAD_SIZE = PAYLOAD_FORMAT.size
assert PAYLOAD_SIZE == 184
TOKEN_PATTERN = re.compile(rb'mgst_[A-Za-z0-9_-]{42}[AEIMQUYcgkosw048]')


@dataclass(frozen=True)
class Payload:
    protocol_version: int
    image_size: int
    token_offset: int
    certificate_offset: int
    checksum_offset: int
    digest_offset: int
    chip: int
    project: str
    app_version: str
    board: str
    canonical_sha: bytes
    peer_sha: bytes
    peer_size: int

    def encode(self):
        return PAYLOAD_FORMAT.pack(MAGIC, self.protocol_version, self.image_size,
                                  self.token_offset, self.certificate_offset,
                                  self.checksum_offset, self.digest_offset, self.chip,
                                  _string(self.project, 32), _string(self.app_version, 32),
                                  _string(self.board, 16), self.canonical_sha,
                                  self.peer_sha, self.peer_size)


def _string(value, size):
    try:
        raw = value.encode('ascii')
    except (AttributeError, UnicodeEncodeError):
        raise ValueError('Metadata must be ASCII') from None
    if not raw or b'\0' in raw or len(raw) >= size:
        raise ValueError('Metadata does not fit its NUL-terminated field')
    return raw.ljust(size, b'\0')


def _read_string(raw):
    value, sep, padding = raw.partition(b'\0')
    if not value or not sep or any(padding):
        raise ValueError('Invalid metadata string padding')
    try:
        return value.decode('ascii')
    except UnicodeDecodeError:
        raise ValueError('Invalid metadata encoding') from None


def parse_payload(raw):
    if len(raw) != PAYLOAD_SIZE:
        raise ValueError('Invalid certificate payload length')
    fields = PAYLOAD_FORMAT.unpack(raw)
    if fields[0] != MAGIC:
        raise ValueError('Invalid certificate payload magic')
    return Payload(*fields[1:8], *map(_read_string, fields[8:11]), *fields[11:])


def _metadata(data, segments):
    start, end = segments[0]
    if end - start < 256 or struct.unpack_from('<I', data, start)[0] != 0xabcd5432:
        raise ValueError('Missing ESP app descriptor in first segment')
    return _read_string(data[start + 48:start + 80]), _read_string(data[start + 16:start + 48])


def _inside(offset, size, segments):
    return any(start <= offset and offset + size <= end for start, end in segments)


def _token_valid(raw):
    return raw == package_private.SLOT or (
        len(raw) == package_private.SLOT_SIZE and TOKEN_PATTERN.fullmatch(raw[:48]) is not None
        and raw[48:] == bytes(package_private.SLOT_SIZE - 48))


def _locations(data, segments):
    cert_offsets = [m.start() for m in re.finditer(re.escape(MARKER), data)]
    token_offsets = [m.start() for m in re.finditer(re.escape(package_private.SLOT), data)]
    token_offsets += [m.start() for m in TOKEN_PATTERN.finditer(data)
                      if _token_valid(data[m.start():m.start() + package_private.SLOT_SIZE])]
    if len(cert_offsets) != 1 or len(token_offsets) != 1:
        raise ValueError('Expected exactly one certificate and one canonical token slot')
    token, certificate = token_offsets[0], cert_offsets[0]
    if not _inside(token, package_private.SLOT_SIZE, segments) or not _inside(certificate, SLOT_SIZE, segments):
        raise ValueError('Reserved slot is outside application segment data')
    if max(token, certificate) < min(token + package_private.SLOT_SIZE, certificate + SLOT_SIZE):
        raise ValueError('Reserved slots overlap')
    desc_start = segments[0][0]
    for offset, size in ((token, package_private.SLOT_SIZE), (certificate, SLOT_SIZE)):
        if max(offset, desc_start) < min(offset + size, desc_start + 256):
            raise ValueError('Reserved slot overlaps app descriptor')
    return token, certificate


def _layout(data, payload):
    segments, checksum, digest = package_private.image_layout(data)
    token, certificate = _locations(data, segments)
    project, version = _metadata(data, segments)
    if (payload.protocol_version != 1 or payload.chip != 9 or payload.project != PROJECT
            or payload.board != BOARD or payload.project != project or payload.app_version != version
            or payload.image_size != len(data) or payload.token_offset != token
            or payload.certificate_offset != certificate or payload.checksum_offset != checksum
            or payload.digest_offset != digest):
        raise ValueError('Certificate metadata or image geometry does not match')
    return segments


def canonical_digest(data, payload):
    """Hash the complete validated image after the four exact virtual replacements."""
    if isinstance(payload, bytes):
        payload = parse_payload(payload)
    _layout(data, payload)
    normalized = bytearray(data)
    normalized[payload.token_offset:payload.token_offset + package_private.SLOT_SIZE] = package_private.SLOT
    normalized[payload.certificate_offset:payload.certificate_offset + SLOT_SIZE] = SLOT
    normalized[payload.checksum_offset] = 0
    normalized[payload.digest_offset:payload.digest_offset + 32] = bytes(32)
    return hashlib.sha256(normalized).digest()


def _key(key, private=False):
    expected = ec.EllipticCurvePrivateKey if private else ec.EllipticCurvePublicKey
    if not isinstance(key, expected) or not isinstance(key.curve, ec.SECP256R1):
        raise ValueError('A P-256 key is required')
    return key


def _footer(data, segments, checksum, digest):
    value = 0xef
    for start, end in segments:
        for byte in data[start:end]:
            value ^= byte
    data[checksum] = value
    data[digest:] = hashlib.sha256(data[:digest]).digest()
    package_private.image_layout(data)
    return bytes(data)


def stamp(data, peer_data, private_key, version):
    """Sign an unmodified certificate slot; public or canonical private tokens work."""
    _key(private_key, private=True)
    _string(version, 32)
    segments, checksum, digest = package_private.image_layout(data)
    package_private.image_layout(peer_data)
    token, certificate = _locations(data, segments)
    if data[certificate:certificate + SLOT_SIZE] != SLOT:
        raise ValueError('Certificate slot must be unmodified before signing')
    payload = Payload(1, len(data), token, certificate, checksum, digest, 9,
                      PROJECT, version, BOARD, bytes(32), hashlib.sha256(peer_data).digest(), len(peer_data))
    payload = Payload(**{**payload.__dict__, 'canonical_sha': canonical_digest(data, payload)})
    raw = payload.encode()
    signature = private_key.sign(raw, ec.ECDSA(hashes.SHA256()))
    if not 1 <= len(signature) <= 80:
        raise ValueError('DER signature does not fit certificate slot')
    out = bytearray(data)
    contents = MARKER + raw + struct.pack('<H', len(signature)) + signature
    out[certificate:certificate + SLOT_SIZE] = contents.ljust(SLOT_SIZE, b'\0')
    result = _footer(out, segments, checksum, digest)
    verify(result, private_key.public_key(), peer_data)
    return result


def verify(data, public_key, peer_data):
    """Validate footer, exact geometry, signature, normalized image, and peer binding."""
    _key(public_key)
    segments, _, _ = package_private.image_layout(data)
    package_private.image_layout(peer_data)
    _, certificate = _locations(data, segments)
    slot = data[certificate:certificate + SLOT_SIZE]
    payload = parse_payload(slot[32:32 + PAYLOAD_SIZE])
    _layout(data, payload)
    sig_start = 32 + PAYLOAD_SIZE + 2
    sig_size = struct.unpack_from('<H', slot, sig_start - 2)[0]
    if not 1 <= sig_size <= 80 or any(slot[sig_start + sig_size:]):
        raise ValueError('Invalid signature length or certificate padding')
    signature = bytes(slot[sig_start:sig_start + sig_size])
    try:
        r, s = utils.decode_dss_signature(signature)
        if utils.encode_dss_signature(r, s) != signature:
            raise ValueError('Noncanonical DER signature')
        public_key.verify(signature, payload.encode(), ec.ECDSA(hashes.SHA256()))
    except (InvalidSignature, ValueError):
        raise ValueError('Certificate signature is invalid') from None
    if payload.canonical_sha != canonical_digest(data, payload):
        raise ValueError('Canonical image digest does not match')
    if payload.peer_size != len(peer_data) or payload.peer_sha != hashlib.sha256(peer_data).digest():
        raise ValueError('Peer image does not match certificate')
    return payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    sign = sub.add_parser('stamp')
    sign.add_argument('input', type=Path)
    sign.add_argument('peer', type=Path)
    sign.add_argument('output', type=Path)
    sign.add_argument('--keyfile', type=Path, required=True)
    sign.add_argument('--version', required=True)
    check = sub.add_parser('verify')
    check.add_argument('input', type=Path)
    check.add_argument('peer', type=Path)
    check.add_argument('--keyfile', type=Path, required=True)
    args = parser.parse_args()
    try:
        key_bytes = args.keyfile.read_bytes()
        if args.command == 'stamp':
            key = serialization.load_pem_private_key(key_bytes, password=None)
            result = stamp(args.input.read_bytes(), args.peer.read_bytes(), key, args.version)
            args.output.parent.mkdir(parents=True, exist_ok=True)
            fd = os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            with os.fdopen(fd, 'wb') as output:
                output.write(result)
            print(f'Signed app image: {args.output}; size: {len(result)}; SHA256: {hashlib.sha256(result).hexdigest()}')
        else:
            key = serialization.load_pem_public_key(key_bytes)
            payload = verify(args.input.read_bytes(), key, args.peer.read_bytes())
            print(f'Certificate valid: {payload.project} {payload.app_version}; board declaration: {payload.board}')
    except (ValueError, TypeError, OSError):
        parser.exit(1, 'Certificate operation failed: invalid input, key, or output path.\n')


if __name__ == '__main__':
    main()
