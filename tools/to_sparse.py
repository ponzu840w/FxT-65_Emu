#!/usr/bin/env python3
"""tools/to_sparse.py - Flat イメージを SPRS スパース形式に変換

Usage:
  python3 tools/to_sparse.py <input.img> <output.img>

SPRS ファイル形式 (little-endian):
  [4 bytes] magic         "SPRS"
  [4 bytes] total_sectors  ディスク論理セクタ数 (uint32)
  [4 bytes] entry_count    非ゼロセクタ数 (uint32)
  entry_count × 516 bytes  エントリ (LBA 昇順):
    [4 bytes]   LBA        セクタ番号 (uint32)
    [512 bytes] data       セクタデータ
"""

import sys
import struct
import os


def convert(src: str, dst: str) -> None:
    SECTOR = 512
    ZERO   = bytes(SECTOR)

    src_size      = os.path.getsize(src)
    total_sectors = src_size // SECTOR

    print(f"Reading '{src}' ({src_size >> 20} MB, {total_sectors} sectors)...")

    non_empty: list[tuple[int, bytes]] = []
    with open(src, "rb") as f:
        for lba in range(total_sectors):
            if lba % 100_000 == 0:
                pct = lba * 100 // total_sectors
                print(f"  {pct:3d}%\r", end="", flush=True)
            data = f.read(SECTOR)
            if data != ZERO:
                non_empty.append((lba, data))

    sparse_bytes = 12 + len(non_empty) * (4 + SECTOR)
    ratio        = src_size // sparse_bytes if sparse_bytes else 0
    print(f"\nNon-empty sectors : {len(non_empty)}")
    print(f"Sparse size       : {sparse_bytes >> 10} KB  ({ratio}x compression)")

    with open(dst, "wb") as f:
        f.write(b"SPRS")
        f.write(struct.pack("<II", total_sectors, len(non_empty)))
        for lba, data in non_empty:
            f.write(struct.pack("<I", lba))
            f.write(data)

    print(f"Written '{dst}'")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    convert(sys.argv[1], sys.argv[2])
