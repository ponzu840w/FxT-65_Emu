#!/usr/bin/env python3
"""tools/to_dynamic_vhd.py - Fixed VHD / フラット .img を Dynamic VHD に変換

Dynamic VHD はブロック単位（デフォルト 2MB）でスパース割り当てを行うため、
未使用領域が多い FAT32 イメージを大幅に圧縮できる。
macOS / Windows / Linux から標準的にマウント可能な形式。

Usage:
  python3 tools/to_dynamic_vhd.py <input.vhd|input.img> <output.vhd>

VHD Dynamic Disk 形式 (big-endian):
  [512  bytes] Footer copy
  [1024 bytes] Dynamic Disk Header
  [BAT        ] Max Table Entries × 4 bytes (512B にパディング)
  [Data blocks] 割り当て済みブロックのみ
  [512  bytes] Footer (フッターコピーと同一内容)

Data block 構造:
  [bitmap_sectors × 512 bytes] セクタビットマップ (全ビット 1)
  [sectors_per_block × 512 bytes] セクタデータ
"""

import sys
import struct
import os
import uuid
import time


SECTOR      = 512
BLOCK_SIZE  = 2 * 1024 * 1024   # 2MB デフォルトブロックサイズ
BLOCK_SECS  = BLOCK_SIZE // SECTOR   # 4096 sectors/block
BITMAP_BYTES = (BLOCK_SECS + 7) // 8  # 512 bytes
BITMAP_SECS  = (BITMAP_BYTES + SECTOR - 1) // SECTOR  # 1 sector
BLOCK_ON_DISK = (BITMAP_SECS + BLOCK_SECS) * SECTOR  # バイト単位のブロックサイズ


def checksum(data: bytes) -> int:
    """VHD チェックサム: 全バイトの和の 1 の補数 (uint32)"""
    s = sum(data) & 0xFFFFFFFF
    return (~s) & 0xFFFFFFFF


def make_footer(disk_size: int, disk_type: int, data_offset: int) -> bytes:
    """VHD フッター (512 bytes) を生成"""
    buf = bytearray(512)

    buf[0:8]   = b"conectix"                          # Cookie
    struct.pack_into(">I", buf,  8, 0x00000002)        # Features
    struct.pack_into(">I", buf, 12, 0x00010000)        # File Format Version
    struct.pack_into(">Q", buf, 16, data_offset)       # Data Offset
    struct.pack_into(">I", buf, 24,
        int(time.time()) - 946684800)                  # Time Stamp (seconds since Jan 1 2000)
    buf[28:32] = b"py  "                              # Creator Application
    struct.pack_into(">I", buf, 32, 0x00010000)        # Creator Version
    struct.pack_into(">I", buf, 36, 0x5769326B)        # Creator Host OS ("Wi2k" = Windows)
    struct.pack_into(">Q", buf, 40, disk_size)         # Original Size
    struct.pack_into(">Q", buf, 48, disk_size)         # Current Size

    # Disk Geometry: CHS (シリンダ・ヘッド・セクタ) 近似計算
    total_sectors = disk_size // SECTOR
    if total_sectors > 65535 * 16 * 255:
        total_sectors = 65535 * 16 * 255
    if total_sectors >= 65535 * 16 * 63:
        spt = 255; heads = 16
    else:
        spt = 17
        heads = max(4, (total_sectors + spt * 1024 - 1) // (spt * 1024))
        if heads > 16:
            spt = 31; heads = 16
        if heads > 16:
            spt = 63; heads = 16
    cyls = total_sectors // (heads * spt)
    struct.pack_into(">H", buf, 56, cyls)
    buf[58] = heads
    buf[59] = spt

    struct.pack_into(">I", buf, 60, disk_type)         # Disk Type
    buf[68:84] = uuid.uuid4().bytes                    # Unique ID

    # Checksum (offset 64) を計算してセット
    struct.pack_into(">I", buf, 64, 0)
    struct.pack_into(">I", buf, 64, checksum(bytes(buf)))

    return bytes(buf)


def make_dynamic_header(table_offset: int, max_entries: int) -> bytes:
    """Dynamic Disk Header (1024 bytes) を生成"""
    buf = bytearray(1024)

    buf[0:8]  = b"cxsparse"                           # Cookie
    struct.pack_into(">Q", buf,  8, 0xFFFFFFFFFFFFFFFF)  # Data Offset (unused)
    struct.pack_into(">Q", buf, 16, table_offset)      # Table Offset → BAT
    struct.pack_into(">I", buf, 24, 0x00010000)        # Header Version
    struct.pack_into(">I", buf, 28, max_entries)       # Max Table Entries
    struct.pack_into(">I", buf, 32, BLOCK_SIZE)        # Block Size (2MB)

    # Checksum (offset 36) を計算してセット
    struct.pack_into(">I", buf, 36, 0)
    struct.pack_into(">I", buf, 36, checksum(bytes(buf)))

    return bytes(buf)


def read_source_sectors(path: str):
    """ソースファイルから (total_sectors, read_sector_fn) を返す"""
    file_size = os.path.getsize(path)
    fp = open(path, "rb")

    # VHD フッター判定
    fp.seek(file_size - SECTOR)
    footer_bytes = fp.read(SECTOR)
    if len(footer_bytes) == SECTOR and footer_bytes[:8] == b"conectix":
        disk_type = struct.unpack_from(">I", footer_bytes, 60)[0]
        if disk_type == 2:  # Fixed VHD
            data_size   = file_size - SECTOR
            total       = data_size // SECTOR
            def read_sec(lba):
                fp.seek(lba * SECTOR)
                return fp.read(SECTOR)
            return total, read_sec, fp
        else:
            fp.close()
            raise ValueError(f"未サポートの VHD タイプ: {disk_type} (Dynamic/Differencing は変換不要)")

    # フラット .img
    total = file_size // SECTOR
    fp.seek(0)
    def read_sec(lba):
        fp.seek(lba * SECTOR)
        return fp.read(SECTOR)
    return total, read_sec, fp


def convert(src_path: str, dst_path: str) -> None:
    print(f"Reading '{src_path}'...")
    total_sectors, read_sector, src_fp = read_source_sectors(src_path)
    disk_size = total_sectors * SECTOR
    max_blocks = (total_sectors + BLOCK_SECS - 1) // BLOCK_SECS

    print(f"  Disk size   : {disk_size >> 20} MB ({total_sectors} sectors)")
    print(f"  Block count : {max_blocks}  ({BLOCK_SIZE >> 20} MB/block)")

    # ブロックごとに非ゼロセクタを調査
    ZERO_SEC = bytes(SECTOR)
    alloc_blocks = []  # (block_num, list[sector_data])

    for blk in range(max_blocks):
        if blk % 100 == 0:
            pct = blk * 100 // max_blocks
            print(f"  Scanning {pct:3d}%\r", end="", flush=True)

        first_lba = blk * BLOCK_SECS
        sectors = []
        has_data = False
        for s in range(BLOCK_SECS):
            lba = first_lba + s
            if lba < total_sectors:
                data = read_sector(lba)
                if len(data) < SECTOR:
                    data = data + bytes(SECTOR - len(data))
            else:
                data = ZERO_SEC
            sectors.append(data)
            if data != ZERO_SEC:
                has_data = True

        if has_data:
            alloc_blocks.append((blk, sectors))

    src_fp.close()
    print(f"\n  Allocated blocks: {len(alloc_blocks)} / {max_blocks}")

    # --- Dynamic VHD レイアウト計算 ---
    # [Footer copy 512B][Dynamic Header 1024B][BAT (padded to 512B)][Data blocks][Footer 512B]
    bat_raw_size  = max_blocks * 4
    bat_padded    = (bat_raw_size + SECTOR - 1) // SECTOR * SECTOR  # 512B にパディング
    footer_offset = 0
    dynhdr_offset = SECTOR              # 512
    bat_offset    = dynhdr_offset + 1024  # 1536
    first_block_offset = bat_offset + bat_padded  # 1536 + bat_padded

    # BAT を構築
    bat = [0xFFFFFFFF] * max_blocks
    cur_offset = first_block_offset
    for blk_num, _ in alloc_blocks:
        bat[blk_num] = cur_offset // SECTOR
        cur_offset += BLOCK_ON_DISK
    footer_at_end = cur_offset

    # --- ファイル書き込み ---
    footer_bytes = make_footer(disk_size, 3, dynhdr_offset)  # type=3 Dynamic
    dynhdr_bytes = make_dynamic_header(bat_offset, max_blocks)

    bitmap_full = bytes([0xFF] * BITMAP_SECS * SECTOR)

    print(f"Writing '{dst_path}'...")
    with open(dst_path, "wb") as out:
        out.write(footer_bytes)        # Footer copy
        out.write(dynhdr_bytes)        # Dynamic Header
        # BAT (big-endian uint32 × max_blocks、512B パディング)
        for entry in bat:
            out.write(struct.pack(">I", entry))
        pad = bat_padded - bat_raw_size
        if pad > 0:
            out.write(bytes(pad))
        # Data blocks
        for blk_num, sectors in alloc_blocks:
            out.write(bitmap_full)
            for sec_data in sectors:
                out.write(sec_data)
        # Footer
        out.write(footer_bytes)

    result_size = os.path.getsize(dst_path)
    ratio = disk_size // result_size if result_size else 0
    print(f"Done. {disk_size >> 20} MB → {result_size >> 20} MB  ({ratio}x compression)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    convert(sys.argv[1], sys.argv[2])
