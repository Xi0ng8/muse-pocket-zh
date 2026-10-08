from pathlib import Path
import struct
import sys
import unittest
import hashlib
import subprocess
import tempfile

from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import package_private as package
import coexist_certificate as cert

VERSION = '0.1.5-coexist-zh'


def footer(data):
    out = bytearray(data)
    cursor, checksum = 24, 0xef
    for _ in range(out[1]):
        size = struct.unpack_from('<I', out, cursor + 4)[0]
        cursor += 8
        for byte in out[cursor:cursor + size]:
            checksum ^= byte
        cursor += size
    offset = cursor + (15 - cursor % 16)
    out[offset] = checksum
    out[offset + 1:] = hashlib.sha256(out[:offset + 1]).digest()
    return bytes(out)


def image(project='muse-gadget', version=VERSION, suffix=b''):
    desc = bytearray(256)
    struct.pack_into('<I', desc, 0, 0xabcd5432)
    desc[16:48] = version.encode() + bytes(32 - len(version))
    desc[48:80] = project.encode() + bytes(32 - len(project))
    body = bytes(desc) + b'code-before' + package.SLOT + b'code-middle' + cert.SLOT + b'code-after' + suffix
    header = bytearray(24)
    header[0], header[1], header[23] = 0xe9, 1, 1
    struct.pack_into('<H', header, 12, 9)
    data = header + struct.pack('<II', 0x3c000020, len(body)) + body
    data += bytes(16 - len(data) % 16) + bytes(32)
    return footer(data)


class CertificateTests(unittest.TestCase):
    def setUp(self):
        self.key = ec.generate_private_key(ec.SECP256R1())
        self.data = image()
        self.peer = image(project='CrossMux', version='coexist')

    def test_roundtrip_and_private_token_normalization(self):
        signed = cert.stamp(self.data, self.peer, self.key, VERSION)
        payload = cert.verify(signed, self.key.public_key(), self.peer)
        self.assertEqual(payload.board, 'xteink_x4_pro')
        self.assertEqual(payload.app_version, VERSION)
        self.assertEqual(payload.canonical_sha, cert.canonical_digest(self.data, payload))
        token = 'mgst_' + 'A' * 43
        private = package.package(signed, token)
        self.assertEqual(cert.verify(private, self.key.public_key(), self.peer), payload)
        self.assertEqual(cert.canonical_digest(private, payload), payload.canonical_sha)
        signed_private = cert.stamp(package.package(self.data, token), self.peer, self.key, VERSION)
        cert.verify(signed_private, self.key.public_key(), self.peer)

    def test_code_signature_padding_token_and_footer_tampering(self):
        signed = cert.stamp(self.data, self.peer, self.key, VERSION)
        payload = cert.verify(signed, self.key.public_key(), self.peer)
        positions = [288, payload.certificate_offset + 32 + 100,
                     payload.certificate_offset + 32 + 184 + 2,
                     payload.certificate_offset + 383, payload.token_offset + 60]
        for pos in positions:
            with self.subTest(pos=pos):
                bad = bytearray(signed)
                bad[pos] ^= 1
                with self.assertRaises(ValueError):
                    cert.verify(footer(bad), self.key.public_key(), self.peer)
        bad = bytearray(signed)
        bad[payload.checksum_offset] ^= 1
        with self.assertRaises(ValueError):
            cert.verify(bad, self.key.public_key(), self.peer)

    def test_peer_and_key_binding(self):
        signed = cert.stamp(self.data, self.peer, self.key, VERSION)
        other = footer(bytearray(self.peer[:288]) + bytes([self.peer[288] ^ 1]) + self.peer[289:])
        with self.assertRaises(ValueError):
            cert.verify(signed, self.key.public_key(), other)
        with self.assertRaises(ValueError):
            cert.verify(signed, ec.generate_private_key(ec.SECP256R1()).public_key(), self.peer)
        with self.assertRaises(ValueError):
            cert.stamp(self.data, self.peer, ec.generate_private_key(ec.SECP384R1()), VERSION)

    def test_metadata_geometry_and_duplicates_rejected(self):
        for data, version in [(image('wrong'), VERSION), (image(version='other'), VERSION),
                              (image(), 'x' * 32)]:
            with self.assertRaises(ValueError):
                cert.stamp(data, self.peer, self.key, version)
        signed = cert.stamp(self.data, self.peer, self.key, VERSION)
        payload = cert.verify(signed, self.key.public_key(), self.peer)
        for field_offset in (12, 16, 20, 24, 28, 32):
            bad = bytearray(signed)
            pos = payload.certificate_offset + 32 + field_offset
            struct.pack_into('<I', bad, pos, 0xffffffff)
            with self.assertRaises(ValueError):
                cert.verify(footer(bad), self.key.public_key(), self.peer)
        for data in (self.data[:-1], self.data + b'x'):
            with self.assertRaises(ValueError):
                cert.stamp(data, self.peer, self.key, VERSION)
        for suffix in (package.SLOT, cert.SLOT, b'mgst_' + b'A' * 43 + bytes(80)):
            with self.assertRaises(ValueError):
                cert.stamp(image(suffix=suffix), self.peer, self.key, VERSION)
        # A marker crossing a segment boundary may not be normalized or signed.
        bad = bytearray(self.data)
        cert_offset = self.data.index(cert.SLOT)
        struct.pack_into('<I', bad, 28, cert_offset + 32 - 32)
        bad = bad[:cert_offset + 32]
        bad += bytes(16 - len(bad) % 16) + bytes(32)
        with self.assertRaises(ValueError):
            cert.stamp(footer(bad), self.peer, self.key, VERSION)

    def test_signature_lengths_and_exact_metadata_padding(self):
        signed = cert.stamp(self.data, self.peer, self.key, VERSION)
        payload = cert.verify(signed, self.key.public_key(), self.peer)
        for length in (0, 81, 65535):
            bad = bytearray(signed)
            struct.pack_into('<H', bad, payload.certificate_offset + 32 + 184, length)
            with self.assertRaises(ValueError):
                cert.verify(footer(bad), self.key.public_key(), self.peer)
        # Even an image with a valid ESP footer must expose a real app descriptor.
        bad = bytearray(self.data)
        bad[32] = 0
        with self.assertRaises(ValueError):
            cert.stamp(footer(bad), self.peer, self.key, VERSION)
        bad = bytearray(self.data)
        bad[32 + 48 + 31] = ord('x')
        with self.assertRaises(ValueError):
            cert.stamp(footer(bad), self.peer, self.key, VERSION)

    def test_cli_key_paths_exclusive_private_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            paths = {name: root / name for name in ('app.bin', 'peer.bin', 'private.pem', 'public.pem', 'signed.bin')}
            paths['app.bin'].write_bytes(self.data)
            paths['peer.bin'].write_bytes(self.peer)
            paths['private.pem'].write_bytes(self.key.private_bytes(
                serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
            paths['public.pem'].write_bytes(self.key.public_key().public_bytes(
                serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo))
            command = [sys.executable, str(TOOLS / 'coexist_certificate.py'), 'stamp',
                       str(paths['app.bin']), str(paths['peer.bin']), str(paths['signed.bin']),
                       '--keyfile', str(paths['private.pem']), '--version', VERSION]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn('PRIVATE KEY', result.stdout + result.stderr)
            self.assertEqual(paths['signed.bin'].stat().st_mode & 0o777, 0o600)
            original = paths['signed.bin'].read_bytes()
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(paths['signed.bin'].read_bytes(), original)
            result = subprocess.run([sys.executable, str(TOOLS / 'coexist_certificate.py'), 'verify',
                                     str(paths['signed.bin']), str(paths['peer.bin']), '--keyfile',
                                     str(paths['public.pem'])], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
