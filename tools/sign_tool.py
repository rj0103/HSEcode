#!/usr/bin/env python3
"""
sign_tool.py - Off-device ECDSA (secp256r1) key generation and image signing
for the GSLU_APP <-> Bootlaoder secure boot chain (see BOOTLOADER_SECURE_BOOT_PLAN.md).

This is the "signing tool" concept from the plan: it runs on a build machine,
never on the ECU. The private key it produces must never be imported into HSE -
only the public key (raw X||Y bytes) is ever provisioned onto the device.

Output formats match what HSE's services expect directly (see
src/HSE_Mac_Ecc_Example.c for the on-device counterpart):
  - Public key: raw X || Y, big-endian, 32 bytes each (64 bytes total) -
    HSE_KEY_FORMAT_ECC_PUB_RAW / hseImportKeySrv_t.pKey[0].
  - Signature: raw r and s, big-endian, 32 bytes each (NOT DER - OpenSSL's
    default `openssl dgst -sign` output is DER-encoded and is NOT directly
    usable here) - hseSignSrv_t.pSignature[0]/[1].

Requires: pip install cryptography

Usage:
  python sign_tool.py genkey  --priv app_signing_key.pem --pub app_public_key_raw.bin
  python sign_tool.py slice   --in pflash_full.bin --base 0x482000 --start <measured __text_start> --end <measured __text_end> --pad-to 0x1E000 --out app2_main_slice.bin
  python sign_tool.py sign    --priv app_signing_key.pem --in app2_main_slice.bin --out-prefix app2_main
  python sign_tool.py verify  --pub  app_public_key_raw.bin --in app2_main_slice.bin --sig-prefix app2_main
  python sign_tool.py header  --pub  app_public_key_raw.bin --sig-prefix app2_main --out ../src/app_smr_2_provision_data.h
"""

import argparse
import sys
from pathlib import Path

from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.exceptions import InvalidSignature

CURVE = ec.SECP256R1()          # == HSE_EC_SEC_SECP256R1
CURVE_BYTE_LEN = 32              # 256-bit curve -> 32-byte components
HASH_ALGO = hashes.SHA256()      # == HSE_HASH_ALGO_SHA2_256


def cmd_genkey(args: argparse.Namespace) -> None:
    priv_path = Path(args.priv)
    pub_raw_path = Path(args.pub)
    pub_pem_path = pub_raw_path.with_suffix(".pem") if args.pub_pem is None else Path(args.pub_pem)

    if priv_path.exists() and not args.force:
        sys.exit(f"error: {priv_path} already exists (pass --force to overwrite)")

    private_key = ec.generate_private_key(CURVE)
    public_key = private_key.public_key()

    priv_pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption(),
    )
    priv_path.write_bytes(priv_pem)

    # Standard PEM copy, for reference/backup only - not what gets imported into HSE.
    pub_pem = public_key.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    pub_pem_path.write_bytes(pub_pem)

    # Raw X || Y, 64 bytes - this is what HSE_Ecc_ImportPublicKey_Ram() / a real
    # provisioning tool imports as HSE_KEY_TYPE_ECC_PUB (HSE_KEY_FORMAT_ECC_PUB_RAW).
    uncompressed = public_key.public_bytes(
        encoding=serialization.Encoding.X962,
        format=serialization.PublicFormat.UncompressedPoint,
    )
    assert uncompressed[0] == 0x04, "unexpected point encoding"
    raw_xy = uncompressed[1:]
    assert len(raw_xy) == 2 * CURVE_BYTE_LEN
    pub_raw_path.write_bytes(raw_xy)

    print(f"Private key (KEEP OFFLINE, never import into HSE): {priv_path}")
    print(f"Public key, raw X||Y (64B, for HSE import):        {pub_raw_path}")
    print(f"Public key, PEM (reference only):                  {pub_pem_path}")
    print(f"Public key X||Y hex: {raw_xy.hex()}")


def _sign_raw(private_key: ec.EllipticCurvePrivateKey, data: bytes) -> tuple[bytes, bytes]:
    der_sig = private_key.sign(data, ec.ECDSA(HASH_ALGO))
    r, s = _decode_der_signature(der_sig)
    return (
        r.to_bytes(CURVE_BYTE_LEN, "big"),
        s.to_bytes(CURVE_BYTE_LEN, "big"),
    )


def _decode_der_signature(der_sig: bytes) -> tuple[int, int]:
    from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature
    return decode_dss_signature(der_sig)


def _encode_der_signature(r: int, s: int) -> bytes:
    from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature
    return encode_dss_signature(r, s)


def cmd_sign(args: argparse.Namespace) -> None:
    priv_path = Path(args.priv)
    in_path = Path(args.__dict__["in"])
    out_prefix = Path(args.out_prefix)

    priv_pem = priv_path.read_bytes()
    private_key = serialization.load_pem_private_key(priv_pem, password=None)
    if not isinstance(private_key, ec.EllipticCurvePrivateKey):
        sys.exit(f"error: {priv_path} is not an EC private key")

    data = in_path.read_bytes()
    r_bytes, s_bytes = _sign_raw(private_key, data)

    r_path = out_prefix.with_name(out_prefix.name + "_sig_r.bin")
    s_path = out_prefix.with_name(out_prefix.name + "_sig_s.bin")
    r_path.write_bytes(r_bytes)
    s_path.write_bytes(s_bytes)

    print(f"Signed {in_path} ({len(data)} bytes, SHA-256/ECDSA)")
    print(f"  r (32B): {r_path}  {r_bytes.hex()}")
    print(f"  s (32B): {s_path}  {s_bytes.hex()}")


def cmd_verify(args: argparse.Namespace) -> None:
    pub_raw_path = Path(args.pub)
    in_path = Path(args.__dict__["in"])
    sig_prefix = Path(args.sig_prefix)

    raw_xy = pub_raw_path.read_bytes()
    if len(raw_xy) != 2 * CURVE_BYTE_LEN:
        sys.exit(f"error: {pub_raw_path} is not {2 * CURVE_BYTE_LEN} raw bytes (got {len(raw_xy)})")

    uncompressed = b"\x04" + raw_xy
    public_key = ec.EllipticCurvePublicKey.from_encoded_point(CURVE, uncompressed)

    r_path = sig_prefix.with_name(sig_prefix.name + "_sig_r.bin")
    s_path = sig_prefix.with_name(sig_prefix.name + "_sig_s.bin")
    r = int.from_bytes(r_path.read_bytes(), "big")
    s = int.from_bytes(s_path.read_bytes(), "big")
    der_sig = _encode_der_signature(r, s)

    data = in_path.read_bytes()
    try:
        public_key.verify(der_sig, data, ec.ECDSA(HASH_ALGO))
    except InvalidSignature:
        print("FAIL: signature does NOT match this file and public key.")
        sys.exit(1)

    print(f"OK: signature over {in_path} verifies against {pub_raw_path}.")


def _c_byte_array(name: str, data: bytes) -> str:
    rows = ["    " + ", ".join(f"0x{b:02X}U" for b in data[i:i + 12]) + "," for i in range(0, len(data), 12)]
    body = "\n".join(rows)
    # Section placement must match the all-zero fallback arrays in HSE_AppSmrProvision.c -
    # both variants belong in Data Flash, outside the SMR-measured [__text_start, __text_end)
    # range, or the install-time signature check can never pass (see that file's own comment).
    section_attr = ' __attribute__((section(".appsmr_keydata")))'
    return f"static const uint8_t {name}[{len(data)}]{section_attr} =\n{{\n{body}\n}};\n"


def cmd_header(args: argparse.Namespace) -> None:
    pub_raw_path = Path(args.pub)
    sig_prefix = Path(args.sig_prefix)
    out_path = Path(args.out)

    raw_xy = pub_raw_path.read_bytes()
    if len(raw_xy) != 2 * CURVE_BYTE_LEN:
        sys.exit(f"error: {pub_raw_path} is not {2 * CURVE_BYTE_LEN} raw bytes (got {len(raw_xy)})")

    r_path = sig_prefix.with_name(sig_prefix.name + "_sig_r.bin")
    s_path = sig_prefix.with_name(sig_prefix.name + "_sig_s.bin")
    r_bytes = r_path.read_bytes()
    s_bytes = s_path.read_bytes()
    if len(r_bytes) != CURVE_BYTE_LEN or len(s_bytes) != CURVE_BYTE_LEN:
        sys.exit(f"error: {r_path}/{s_path} must be {CURVE_BYTE_LEN} bytes each")

    region_defines = ""
    if (args.region_start is None) != (args.region_end is None):
        sys.exit("error: --region-start and --region-end must be supplied together")
    if args.region_start is not None:
        region_start = _parse_int(args.region_start)
        region_end = _parse_int(args.region_end)
        if region_end <= region_start:
            sys.exit("error: --region-end must be greater than --region-start")
        region_defines = f'''\n/* SMR region described and signed by this payload. */
#define APP_SMR_REGION_START  (0x{region_start:08X}UL)
#define APP_SMR_REGION_END    (0x{region_end:08X}UL)
#define APP_SMR_REGION_SIZE   (APP_SMR_REGION_END - APP_SMR_REGION_START)
'''

    guard = "APP_SMR_PROVISION_DATA_H"
    header = f"""/**
 * AUTO-GENERATED by tools/sign_tool.py - DO NOT EDIT BY HAND.
 * Regenerate whenever the signing key changes or GSLU_APP is rebuilt/resigned:
 *   python tools/sign_tool.py sign   --priv <priv.pem> --in <built image> --out-prefix {sig_prefix.name}
 *   python tools/sign_tool.py header --pub {pub_raw_path.name} --sig-prefix {sig_prefix.name} --out {out_path.name}
 *
 * Public key: raw X||Y, secp256r1 (HSE_EC_SEC_SECP256R1), from {pub_raw_path.name}
 * Signature: raw (r,s), ECDSA/SHA-256, over the file signed with sign_tool.py's `sign` command
 */
#ifndef {guard}
#define {guard}

#include <stdint.h>
{region_defines}

{_c_byte_array("AppSmr_PublicKeyXY", raw_xy)}
{_c_byte_array("AppSmr_SignatureR", r_bytes)}
{_c_byte_array("AppSmr_SignatureS", s_bytes)}
#endif /* {guard} */
"""
    out_path.write_text(header)
    print(f"Wrote {out_path}")


def _parse_int(s: str) -> int:
    return int(s, 0)  # accepts 0x-prefixed hex or plain decimal


def cmd_slice(args: argparse.Namespace) -> None:
    in_path = Path(args.__dict__["in"])
    out_path = Path(args.out)
    base = _parse_int(args.base)
    start = _parse_int(args.start)
    end = _parse_int(args.end)
    pad_to = _parse_int(args.pad_to) if args.pad_to is not None else None

    if end <= start:
        sys.exit(f"error: --end (0x{end:X}) must be greater than --start (0x{start:X})")
    if start < base:
        sys.exit(f"error: --start (0x{start:X}) must be >= --base (0x{base:X})")

    offset = start - base
    length = end - start

    data = in_path.read_bytes()
    if offset + length > len(data):
        sys.exit(
            f"error: {in_path} is only {len(data)} bytes, but --start/--base/--end implies "
            f"reading up to byte {offset + length} - check the addresses match this exact file"
        )

    sliced = data[offset:offset + length]

    if pad_to is not None:
        # Fixed-size SMR region (e.g. Main app's [0x00482000, 0x004C2000), 0x00040000 bytes) -
        # the actual compiled image is always <= this size and gets zero-padded out to it, so the
        # SMR entry's smrSize is a compile-time constant that never needs recalculating per build
        # (only the (r,s) signature changes when the app is rebuilt/resigned).
        if length > pad_to:
            sys.exit(
                f"error: sliced region is {length} bytes (0x{length:X}), which exceeds "
                f"--pad-to (0x{pad_to:X}) - the built image no longer fits in the fixed SMR region"
            )
        sliced = sliced + (b"\x00" * (pad_to - length))
        print(f"Zero-padded from {length} bytes (0x{length:X}) to {pad_to} bytes (0x{pad_to:X})")

    out_path.write_bytes(sliced)
    print(f"Wrote {out_path}: {len(sliced)} bytes (0x{len(sliced):X}), sliced from offset 0x{offset:X} in {in_path}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    p_genkey = sub.add_parser("genkey", help="Generate a new secp256r1 key pair.")
    p_genkey.add_argument("--priv", required=True, help="Output path for the private key (PEM, PKCS8). Keep offline.")
    p_genkey.add_argument("--pub", required=True, help="Output path for the public key, raw X||Y (64 bytes).")
    p_genkey.add_argument("--pub-pem", default=None, help="Optional output path for a PEM copy of the public key (reference only).")
    p_genkey.add_argument("--force", action="store_true", help="Overwrite an existing private key file.")
    p_genkey.set_defaults(func=cmd_genkey)

    p_sign = sub.add_parser("sign", help="Sign a file (e.g. the built image) with an existing private key.")
    p_sign.add_argument("--priv", required=True, help="Path to the private key (PEM, PKCS8).")
    p_sign.add_argument("--in", dest="in", required=True, help="Path to the file/image to sign.")
    p_sign.add_argument("--out-prefix", required=True, help="Prefix for the output <prefix>_sig_r.bin / _sig_s.bin files.")
    p_sign.set_defaults(func=cmd_sign)

    p_verify = sub.add_parser("verify", help="Verify a signature against a file and public key (sanity check before flashing).")
    p_verify.add_argument("--pub", required=True, help="Path to the public key, raw X||Y (64 bytes).")
    p_verify.add_argument("--in", dest="in", required=True, help="Path to the file/image that was signed.")
    p_verify.add_argument("--sig-prefix", required=True, help="Prefix matching the --out-prefix used at sign time.")
    p_verify.set_defaults(func=cmd_verify)

    p_header = sub.add_parser("header", help="Bundle a public key + signature into a C header GSLU_APP can #include.")
    p_header.add_argument("--pub", required=True, help="Path to the public key, raw X||Y (64 bytes).")
    p_header.add_argument("--sig-prefix", required=True, help="Prefix matching the --out-prefix used at sign time.")
    p_header.add_argument("--out", required=True, help="Output path for the generated C header.")
    p_header.add_argument("--region-start", default=None, help="Optional signed region start; emits Boot 1 SMR region macros.")
    p_header.add_argument("--region-end", default=None, help="Optional signed region end; required with --region-start.")
    p_header.set_defaults(func=cmd_header)

    p_slice = sub.add_parser("slice", help="Slice out [start,end) from a flat binary dump, given its base load address.")
    p_slice.add_argument("--in", dest="in", required=True, help="Path to the flat binary (e.g. from objcopy -O binary -j .pflash).")
    p_slice.add_argument("--base", required=True, help="Load address of byte 0 of --in (e.g. ORIGIN(int_pflash), hex or decimal).")
    p_slice.add_argument("--start", required=True, help="Address to start the slice at (e.g. __text_start).")
    p_slice.add_argument("--end", required=True, help="Address to end the slice at, exclusive (e.g. __text_end).")
    p_slice.add_argument("--pad-to", default=None, help="Zero-pad the slice out to this many bytes (e.g. a fixed SMR region size like 0x40000) - errors if the slice is already larger.")
    p_slice.add_argument("--out", required=True, help="Output path for the sliced bytes.")
    p_slice.set_defaults(func=cmd_slice)

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
