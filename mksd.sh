#!/bin/bash
#
# mksd.sh
# MIRACOS のブートディスク (SDカードイメージ) を作成する
#
# 対応プラットフォーム:
#   - macOS: hdiutil + diskutil + mkfs.fat (brew) + mount (sudo あり)
#   - Linux / WSL: sfdisk + mkfs.fat + mtools (sudo 不要、ループバック不要)
#
# 出力 (4 GiB sparse, MBR 1 パーティション, FAT32, クラスタ=64セクタ/32KB, FAT数=2)
# クラスタ 32 KiB の場合 2 GiB だと FAT32 規格の最小クラスタ数 65525 を下回るため 4 GiB が必要
#
# Usage: ./mksd.sh [output.img]
#   出力先を省略した場合は ./sdcard.img を作成する

set -euo pipefail

IMG="${1:-sdcard.img}"
SIZE_MB=4096                 # SD カード全体サイズ (4 GiB)
PART_START_SECTORS=2048      # パーティション開始セクタ (512B 単位, 1MB 位置)
SECTORS_PER_CLUSTER=64       # 64 sec × 512B = 32 KiB クラスタ
NUM_FATS=2                   # FAT 数

OS=$(uname -s)
DISK=""
MNT=""

cleanup() {
  if [[ -n "$MNT" && -d "$MNT" ]]; then
    sudo umount "$MNT" 2>/dev/null || true
    rmdir "$MNT" 2>/dev/null || true
  fi
  if [[ -n "$DISK" ]]; then
    hdiutil detach "$DISK" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

echo "Creating $IMG (${SIZE_MB}MB sparse, FAT32 cluster=${SECTORS_PER_CLUSTER} sectors)..."
rm -f "$IMG"

if [ "$OS" = "Darwin" ]; then
  # ==================================================================
  #  macOS フロー
  # ==================================================================
  # sparse: 実体は書き込みが発生したぶんだけ確保される
  dd if=/dev/zero of="$IMG" bs=1m count=0 seek=$SIZE_MB

  DISK=$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount "$IMG" \
    | head -1 | awk '{print $1}')
  echo "DISK: $DISK"

  # MBR + FAT32 パーティション (フォーマットまでされるが後で再フォーマット)
  diskutil partitionDisk "$DISK" MBR "MS-DOS FAT32" MCOS 100%
  diskutil unmount "${DISK}s1"

  # クラスタサイズ指定で再フォーマット
  sudo "$(brew --prefix)/sbin/mkfs.fat" -F 32 -s "$SECTORS_PER_CLUSTER" -f "$NUM_FATS" "${DISK}s1"

  # MIRACOS システムファイルをコピー
  MNT=$(mktemp -d)
  sudo mount -t msdos "${DISK}s1" "$MNT"
  if [ -d miracos/bin ] && [ -n "$(ls -A miracos/bin 2>/dev/null)" ]; then
    sudo cp -r miracos/bin/* "$MNT/"
  else
    echo "WARN: miracos/bin/ が空または存在しません ('make os' で生成してください)" >&2
  fi
  sudo umount "$MNT"
  rmdir "$MNT"
  MNT=""

  hdiutil detach "$DISK"
  DISK=""

else
  # ==================================================================
  #  Linux / WSL フロー (sudo 不要)
  # ==================================================================
  missing=()
  for tool in sfdisk mkfs.fat mcopy truncate; do
    command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
  done
  if [ ${#missing[@]} -ne 0 ]; then
    echo "ERROR: 次のコマンドが見つかりません: ${missing[*]}" >&2
    echo "  Ubuntu/Debian:  sudo apt install dosfstools mtools util-linux coreutils" >&2
    exit 1
  fi

  # 1) ゼロ埋めイメージ作成 (疎ファイル)
  truncate -s "${SIZE_MB}M" "$IMG"

  # 2) MBR パーティションテーブル書き込み
  #    start=2048 セクタ, type=0c (FAT32 LBA)
  sfdisk --no-reread --no-tell-kernel "$IMG" >/dev/null <<EOF
label: dos
unit: sectors
start=${PART_START_SECTORS}, type=0c
EOF

  # 3) FAT32 フォーマット (--offset でパーティション先頭に書く)
  #    BLOCKS 引数 (1024B 単位) = パーティションサイズ
  total_sectors=$(( SIZE_MB * 1024 * 1024 / 512 ))
  part_sectors=$(( total_sectors - PART_START_SECTORS ))
  part_blocks_1k=$(( part_sectors / 2 ))
  mkfs.fat -F 32 -s "$SECTORS_PER_CLUSTER" -f "$NUM_FATS" -n MCOS \
           --offset="${PART_START_SECTORS}" \
           "$IMG" "${part_blocks_1k}" >/dev/null

  # 4) MIRACOS システムファイルを mtools でコピー
  #    mcopy は @@<byte offset> でイメージ内のパーティションを直接指定できる
  part_byte_offset=$(( PART_START_SECTORS * 512 ))
  if [ -d miracos/bin ] && [ -n "$(ls -A miracos/bin 2>/dev/null)" ]; then
    export MTOOLS_SKIP_CHECK=1
    for f in miracos/bin/*; do
      mcopy -i "${IMG}@@${part_byte_offset}" -s "$f" ::
    done
  else
    echo "WARN: miracos/bin/ が空または存在しません ('make os' で生成してください)" >&2
  fi
fi

echo "完了: $IMG"
echo "  ネイティブ版: make img2vhd  # sdcard.img → sdcard.vhd"
