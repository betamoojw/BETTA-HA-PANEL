# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import tempfile
import unittest
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import license_issuer as issuer
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec, utils


class IssuerTest(unittest.TestCase):
    def test_device_binding_and_signature(self):
        key = ec.generate_private_key(ec.SECP256R1())
        device = "ab" * 32
        data = issuer.sign(key, device)
        self.assertEqual(len(data), 200)
        raw = bytes.fromhex(data.decode().split(":")[2])
        der = utils.encode_dss_signature(int.from_bytes(raw[:32], "big"), int.from_bytes(raw[32:], "big"))
        key.public_key().verify(der, issuer.message(device), ec.ECDSA(hashes.SHA256()))
        with self.assertRaises(InvalidSignature):
            key.public_key().verify(der, issuer.message("cd" * 32), ec.ECDSA(hashes.SHA256()))
        self.assertEqual(len(issuer.public_hex(key)), 130)

    def test_reject_noncanonical_id_and_curve(self):
        for device in ("", "AB" * 32, "ab" * 31, "ab" * 32 + "\n", "a" * 63 + "g"):
            with self.assertRaises(ValueError): issuer.message(device)
        with self.assertRaises(ValueError): issuer.sign(ec.generate_private_key(ec.SECP384R1()), "a" * 64)

    def test_never_overwrite(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "key.pem"
            issuer.write_new(path, b"original")
            with self.assertRaises(FileExistsError): issuer.write_new(path, b"replacement")
            self.assertEqual(path.read_bytes(), b"original")


if __name__ == "__main__": unittest.main()
