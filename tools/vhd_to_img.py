#!/usr/bin/env python3
"""tools/vhd_to_img.py - VHD (Fixed / Dynamic) をフラット .img に展開

Dynamic VHD の未割当ブロックはゼロセクタとして出力する。
出力した .img は hdiutil attach でマウント可能。

Usage:
  python3 tools/vhd_to_img.py <input.vhd> <output.img>
"""

import sys
import struct
import os


SECTOR = 512


def read_be32(data: bytes, off: int) -> int:
    return struct.unpack_from(">I", data, off)[0]


def read_be64(data: bytes, off: int) -> int:
    return struct.unpack_from(">Q", data, off)[0]


def convert(src: str, dst: str) -> None:
    file_size = os.path.getsize(src)
    if file_size < SECTOR:
        raise ValueError("ファイルが小さすぎます")

    with open(src, "rb") as f:
        # フッターを読む (末尾 512B)
        f.seek(file_size - SECTOR)
        footer = f.read(SECTOR)

    if footer[:8] != b"conectix":
        raise ValueError("VHD フッターが見つかりません (conectix magic なし)")

    disk_type   = read_be32(footer, 60)
    disk_size   = read_be64(footer, 48)
    total_secs  = disk_size // SECTOR

    print(f"VHD Disk Type : {disk_type} ({'Fixed' if disk_type == 2 else 'Dynamic' if disk_type == 3 else '?'})")
    print(f"Disk Size     : {disk_size >> 20} MB ({total_secs} sectors)")

    ZERO = bytes(SECTOR)

    with open(src, "rb") as inp, open(dst, "wb") as out:

        if disk_type == 2:
            # Fixed VHD: 生データをそのまま
            inp.seek(0)
            remaining = disk_size
            while remaining > 0:
                chunk = min(remaining, 65536)
                out.write(inp.read(chunk))
                remaining -= chunk

        elif disk_type == 3:
            # Dynamic VHD
            data_offset = read_be64(footer, 16)   # Dynamic Header の位置

            inp.seek(data_offset)
            dyn_hdr = inp.read(1024)
            table_offset     = read_be64(dyn_hdr, 16)
            max_entries      = read_be32(dyn_hdr, 28)
            block_size       = read_be32(dyn_hdr, 32)
            sectors_per_block = block_size // SECTOR
            bitmap_bytes     = (sectors_per_block + 7) // 8
            bitmap_secs      = (bitmap_bytes + SECTOR - 1) // SECTOR

            # BAT 読み込み
            inp.seek(table_offset)
            bat = []
            for _ in range(max_entries):
                entry = struct.unpack(">I", inp.read(4))[0]
                bat.append(entry)

            # セクタを順に出力
            for blk in range(max_entries):
                if blk % 50 == 0:
                    pct = blk * 100 // max_entries
                    print(f"  Writing {pct:3d}%\r", end="", flush=True)

                first_lba = blk * sectors_per_block
                if first_lba >= total_secs:
                    break

                if bat[blk] == 0xFFFFFFFF:
                    # 未割当ブロック → ゼロ出力
                    count = min(sectors_per_block, total_secs - first_lba)
                    for _ in range(count):
                        out.write(ZERO)
                else:
                    block_byte = bat[blk] * SECTOR
                    data_start = block_byte + bitmap_secs * SECTOR
                    inp.seek(data_start)
                    count = min(sectors_per_block, total_secs - first_lba)
                    for _ in range(count):
                        data = inp.read(SECTOR)
                        if len(data) < SECTOR:
                            data += bytes(SECTOR - len(data))
                        out.write(data)
            print()

        else:
            raise ValueError(f"未サポートの VHD タイプ: {disk_type}")

    result_size = os.path.getsize(dst)
    print(f"Written '{dst}' ({result_size >> 20} MB)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    convert(sys.argv[1], sys.argv[2])
