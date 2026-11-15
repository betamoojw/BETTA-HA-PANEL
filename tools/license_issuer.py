#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
"""Offline P-256 issuer. Requires cryptography (available in the ESP-IDF environment)."""
import argparse
import getpass
import os
import re
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils


def message(device_id: str) -> bytes:
    if not re.fullmatch(r"[0-9a-f]{64}", device_id):
        raise ValueError("Device ID must be 64 lowercase hexadecimal characters")
    return f"BETTA-HA-PANEL-LICENSE/v1\n{device_id}\nperpetual\n".encode("ascii")


def public_hex(key) -> str:
    public = key.public_key() if isinstance(key, ec.EllipticCurvePrivateKey) else key
    if not isinstance(public, ec.EllipticCurvePublicKey) or not isinstance(public.curve, ec.SECP256R1):
        raise ValueError("Issuer key must use P-256 (secp256r1)")
    return public.public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint).hex()


def sign(key, device_id: str) -> bytes:
    public_hex(key)  # Reject unsupported curves before signing.
    signature = key.sign(message(device_id), ec.ECDSA(hashes.SHA256()))
    r, s = utils.decode_dss_signature(signature)
    raw = r.to_bytes(32, "big") + s.to_bytes(32, "big")
    return f"BETTA1:{device_id}:{raw.hex()}".encode("ascii")


def write_new(path: Path, data: bytes) -> None:
    # Exclusive creation prevents accidentally replacing a production issuer key.
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "wb") as stream:
        stream.write(data)


def load_private(path: Path):
    data = path.read_bytes()
    try:
        return serialization.load_pem_private_key(data, password=None)
    except TypeError:
        return serialization.load_pem_private_key(data, getpass.getpass("Issuer key passphrase: ").encode())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    generate = commands.add_parser("keygen", help="Create an encrypted private key and public PEM; never overwrite")
    generate.add_argument("--private-key", type=Path, required=True)
    generate.add_argument("--public-key", type=Path, required=True)
    public = commands.add_parser("public", help="Print firmware configuration from a public PEM")
    public.add_argument("--public-key", type=Path, required=True)
    issue = commands.add_parser("sign", help="Sign a perpetual, device-bound DEVICE_KEY file")
    issue.add_argument("--private-key", type=Path, required=True)
    issue.add_argument("--device-id", required=True)
    issue.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "keygen":
            if args.private_key.resolve() == args.public_key.resolve() or args.private_key.exists() or args.public_key.exists():
                raise ValueError("Choose two distinct paths that do not already exist")
            password = getpass.getpass("New issuer key passphrase: ")
            if len(password) < 12 or password != getpass.getpass("Repeat passphrase: "):
                raise ValueError("Matching passphrases of at least 12 characters are required")
            key = ec.generate_private_key(ec.SECP256R1())
            write_new(args.private_key, key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                serialization.BestAvailableEncryption(password.encode())))
            write_new(args.public_key, key.public_key().public_bytes(serialization.Encoding.PEM,
                serialization.PublicFormat.SubjectPublicKeyInfo))
        elif args.command == "public":
            key = serialization.load_pem_public_key(args.public_key.read_bytes())
            print(f'CONFIG_APP_LICENSE_PUBLIC_KEY_HEX="{public_hex(key)}"')
        else:
            message(args.device_id)
            write_new(args.output, sign(load_private(args.private_key), args.device_id))
    except (ValueError, TypeError, OSError) as error:
        parser.exit(1, f"License issuer: {error}\n")


if __name__ == "__main__":
    main()
