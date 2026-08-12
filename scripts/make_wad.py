#!/usr/bin/env python3
"""
make_wad.py — Wii Homebrew WAD Packer

Creates a Wii channel WAD from a boot.dol file.
The resulting WAD can be opened directly in Dolphin Emulator (iOS/iPad/PC).

Usage:
    python3 make_wad.py ABCD "App Name" boot.dol output.wad

    ABCD : exactly 4 ASCII characters used as the unique channel ID
           (e.g. CUBE → title 00010008 43554245)

Requires: openssl (pre-installed in devkitPro container and GitHub Actions)
"""

import sys, os, struct, hashlib, subprocess, tempfile

# ── Wii constants ──────────────────────────────────────────────────────────────
# Common key: public knowledge, shipped with every Wii, used to encrypt title keys
COMMON_KEY = bytes.fromhex("ebe42a225e8593e448d9c5457381aaf7")

# ── Crypto helpers ─────────────────────────────────────────────────────────────

def aes128_cbc_enc(key: bytes, iv: bytes, data: bytes) -> bytes:
    """AES-128-CBC encrypt via openssl (zero external Python deps)."""
    assert len(key) == 16 and len(iv) == 16
    assert len(data) % 16 == 0, "data must be a multiple of 16 bytes"
    r = subprocess.run(
        ["openssl", "enc", "-aes-128-cbc", "-nosalt", "-nopad",
         "-K", key.hex(), "-iv", iv.hex()],
        input=data, capture_output=True, check=True
    )
    return r.stdout

def sha1(data: bytes) -> bytes:
    return hashlib.sha1(data).digest()

# ── WAD layout helpers ─────────────────────────────────────────────────────────

def pad64(data: bytes) -> bytes:
    """Pad data to the next 64-byte boundary."""
    rem = len(data) % 64
    return data + (b"\x00" * (64 - rem) if rem else b"")

# ── Certificate chain ──────────────────────────────────────────────────────────

def make_cert_chain() -> bytes:
    """
    Minimal zeroed-out cert chain understood by Dolphin's WAD installer.
    Three certs: Root CA (RSA-4096, 0x500 B) + XS (RSA-2048, 0x300 B) +
    CP (RSA-2048, 0x300 B) = 0xB00 bytes, all zeroed (fakesigned).
    Dolphin does not enforce certificate signatures for homebrew WADs.
    """
    return b"\x00" * (0x500 + 0x300 + 0x300)  # 0xB00 total

# ── Ticket (0x2C4 bytes) ───────────────────────────────────────────────────────

def make_ticket(title_id: bytes, title_key_enc: bytes) -> bytes:
    """
    Build a fakesigned ticket.
    RSA signature is zeroed — Dolphin accepts this for homebrew.
    """
    assert len(title_id) == 8
    assert len(title_key_enc) == 16

    t = bytearray(0x2C4)

    # Signature type: RSA-2048 / SHA1
    struct.pack_into(">I", t, 0x000, 0x00010001)
    # 0x004..0x103 : RSA-2048 signature  (zeroed = fakesigned)
    # 0x104..0x13F : padding              (zeroed)

    # Issuer (64 bytes at 0x140)
    issuer = b"Root-CA00000001-XS00000003"
    t[0x140 : 0x140 + len(issuer)] = issuer

    # ECDH data  0x180..0x1BB  (zeroed)
    # format_ver 0x1BC = 0
    # ca_crl_ver 0x1BD = 0
    # sign_crl   0x1BE = 0

    # Encrypted title key (16 bytes at 0x1BF)
    t[0x1BF : 0x1BF + 16] = title_key_enc

    # 0x1CF : unknown = 0
    # Ticket ID  0x1D0 (8 bytes, zeroed)
    # Console ID 0x1D8 (4 bytes, zeroed)

    # Title ID (8 bytes at 0x1DC)
    t[0x1DC : 0x1DC + 8] = title_id

    # Unknown 0x1E4 (u16) = 0xFFFF
    struct.pack_into(">H", t, 0x1E4, 0xFFFF)
    # Title version 0x1E6 (u16) = 0

    # Content access permissions: 0x222..0x261 = all 0xFF
    t[0x222 : 0x262] = b"\xFF" * 0x40

    # Limits 0x264..0x2A3 = zeroed

    return bytes(t)

# ── TMD (Title Metadata) ───────────────────────────────────────────────────────

def make_tmd(title_id: bytes, title_ver: int, contents: list) -> bytes:
    """
    Build a fakesigned TMD.
    contents: list of dicts with keys: id, index, type, sha1, size
    """
    assert len(title_id) == 8

    tmd = bytearray()

    # ── Signed blob ──
    # Signature type: RSA-2048 / SHA1
    tmd += struct.pack(">I", 0x00010001)
    tmd += b"\x00" * 0x100  # RSA-2048 signature (zeroed = fakesigned)
    tmd += b"\x00" * 0x3C   # padding

    # ── TMD header ──
    issuer = b"Root-CA00000001-CP00000004"
    tmd += issuer + b"\x00" * (0x40 - len(issuer))  # issuer, padded to 64 bytes

    tmd += b"\x01"           # version
    tmd += b"\x00"           # ca_crl_version
    tmd += b"\x00"           # signer_crl_version
    tmd += b"\x00"           # vWii flag

    tmd += struct.pack(">Q", 0x0000000100000002)  # sys_version: IOS2
    tmd += title_id                                # title_id (8 bytes)
    tmd += struct.pack(">I", 0x00000001)           # title_type: normal channel
    tmd += struct.pack(">H", 0)                    # group_id
    tmd += struct.pack(">H", 0)                    # region: global
    tmd += b"\x00" * 16                            # ratings
    tmd += b"\x00" * 12                            # reserved
    tmd += b"\x00" * 12                            # ipc_mask
    tmd += b"\x00" * 18                            # reserved2
    tmd += struct.pack(">I", 0)                    # access_rights
    tmd += struct.pack(">H", title_ver)            # title_version
    tmd += struct.pack(">H", len(contents))        # num_contents
    tmd += struct.pack(">H", 0)                    # boot_index
    tmd += struct.pack(">H", 0)                    # minor_version / padding

    # ── Content records (0x24 bytes each) ──
    for c in contents:
        tmd += struct.pack(">I", c["id"])      # content_id
        tmd += struct.pack(">H", c["index"])   # index
        tmd += struct.pack(">H", c["type"])    # type (1 = normal)
        tmd += struct.pack(">Q", c["size"])    # size (unpadded)
        tmd += c["sha1"]                       # SHA1 of plaintext content

    return bytes(tmd)

# ── WAD assembly ───────────────────────────────────────────────────────────────

def pack_wad(channel_id: str, app_name: str, dol_data: bytes) -> bytes:
    """
    Build a complete fakesigned Wii channel WAD.

    channel_id : 4 ASCII chars (e.g. "CUBE")
    app_name   : display name (unused in bare WAD, kept for reference)
    dol_data   : raw bytes of boot.dol
    """
    assert len(channel_id) == 4, "channel_id must be exactly 4 ASCII chars"

    # Title ID: 00010008 + 4-char channel ID
    title_id = b"\x00\x01\x00\x08" + channel_id.encode("ascii")

    # Null title key — all zeros
    title_key = b"\x00" * 16

    # Encrypt title key with common key; IV = title_id zero-padded to 16 bytes
    title_key_iv = title_id + b"\x00" * 8
    title_key_enc = aes128_cbc_enc(COMMON_KEY, title_key_iv, title_key)

    # Pad DOL to 16-byte boundary for AES
    content_plain = dol_data
    rem = len(content_plain) % 16
    if rem:
        content_plain += b"\x00" * (16 - rem)

    # SHA1 of plaintext content (used in TMD)
    content_sha1 = sha1(content_plain)

    # Encrypt content: AES-128-CBC, title_key, IV = content_index (u16) + 14 zeros
    content_index = 0
    content_iv = struct.pack(">H", content_index) + b"\x00" * 14
    content_enc = aes128_cbc_enc(title_key, content_iv, content_plain)

    # Build each WAD section
    cert_chain = make_cert_chain()
    ticket     = make_ticket(title_id, title_key_enc)
    tmd_data   = make_tmd(
        title_id, 1,
        [{"id": 0, "index": content_index, "type": 1,
          "sha1": content_sha1, "size": len(dol_data)}]
    )
    data_section = pad64(content_enc)   # contents padded to 64 bytes

    # WAD header (0x20 bytes)
    header = struct.pack(">IIIIIIII",
        0x00000020,        # header_size
        0x49730000,        # type 'Is\x00\x00'
        len(cert_chain),   # cert_chain_size
        0,                 # crl_size
        len(ticket),       # ticket_size (0x2C4)
        len(tmd_data),     # tmd_size
        len(data_section), # data_size
        0,                 # footer_size
    )

    # Assemble: header + each section, every section 64-byte aligned
    wad  = pad64(header)
    wad += pad64(cert_chain)
    # CRL is 0 bytes — omitted (no padding needed)
    wad += pad64(ticket)
    wad += pad64(tmd_data)
    wad += data_section
    # footer is 0 bytes — omitted

    return wad

# ── Entry point ────────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) != 5:
        print(f"Usage: {sys.argv[0]} ABCD 'App Name' boot.dol output.wad")
        print(f"  ABCD : 4 ASCII characters for the unique channel ID")
        sys.exit(1)

    channel_id = sys.argv[1]
    app_name   = sys.argv[2]
    dol_path   = sys.argv[3]
    out_path   = sys.argv[4]

    if len(channel_id) != 4:
        print("Error: channel_id must be exactly 4 ASCII characters")
        sys.exit(1)

    with open(dol_path, "rb") as f:
        dol_data = f.read()

    title_id_hex = "00010008" + channel_id.encode("ascii").hex().upper()
    print(f"Packing WAD: title_id={title_id_hex}  name={app_name!r}  dol={len(dol_data):,} bytes")

    wad = pack_wad(channel_id, app_name, dol_data)

    with open(out_path, "wb") as f:
        f.write(wad)

    print(f"WAD created: {out_path}  ({len(wad):,} bytes)")

if __name__ == "__main__":
    main()
