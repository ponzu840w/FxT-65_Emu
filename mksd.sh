#!/bin/bash

# mksd.sh
# MIRACOSのブートディスクのSDカードイメージを作成する
# mac用

# 空のイメージ作成
dd if=/dev/zero of=sdcard.img bs=1m count=2048

# アタッチ
DISK=$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount sdcard.img \
  | head -1 | awk '{print $1}')
echo "DISK: $DISK"

# MBRでパーティション作成
#  フォーマットまでされるが、クラスタサイズを指定できないのであとで破棄する
diskutil partitionDisk $DISK MBR "MS-DOS FAT32" MCOS 100%

# マウントまでされてしまうのでアンマウント
diskutil unmount ${DISK}s1

# クラスタサイズを -s 32 で指定して再フォーマット
sudo "$(brew --prefix)/sbin/mkfs.fat" -F 32 -s 32 -f 2 ${DISK}s1
#sudo newfs_msdos -F 32 -c 64 -k 2 ${DISK}s1 # こちらのコマンドでは機能しない可能性がある

# マウントしてMIRACOSシステムファイルをコピー
MNT=$(mktemp -d)
sudo mount -t msdos ${DISK}s1 $MNT
sudo cp -r assets/MIRACOS_SDC/* $MNT/
sudo umount $MNT

# デタッチ
hdiutil detach $DISK

