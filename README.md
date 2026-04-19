# FxT-65 Emulator

## ビルド

```
git submodule update --init --recursive
make                  # ネイティブ (mac/linux 自動判定)
./fxt65
```

`PLATFORM=` で切替: `mac` / `linux` / `win` (MinGW クロス) / `web` (Emscripten)。

### 依存

- 共通: `make`, `python3`, `cl65` (cc65)
- Linux: `build-essential libx11-dev libxi-dev libxcursor-dev libgl1-mesa-dev libasound2-dev`
- Windows クロス: `g++-mingw-w64-x86-64`
- Web: Emscripten, `python3-fonttools`

## SDカード生成

```
make os        # MIRACOS を miracos/bin/ にビルド
./mksd.sh      # sdcard.img (2GB FAT32) を作成
make vhd       # sdcard.img → sdcard.vhd
```

mksd.sh の追加依存:

- mac: `brew install dosfstools`
- Linux: `sudo apt install mtools dosfstools`

### 編集サイクル

```
make img       # sdcard.vhd → sdcard.img
# 編集 (mac: hdiutil attach / linux: mcopy -i sdcard.img@@1048576 ...)
make vhd
```

起動中は UI メニューから別 VHD に差し替え可。
