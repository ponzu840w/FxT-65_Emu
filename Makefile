# Makefile

# ----------
#    設定
# ----------

# PLATFORM 自動判定:
#   未指定時は uname -s から native を mac / linux に振り分ける。
#   Web / Windows (MinGW クロスビルド) は明示的に指定する:
#     make PLATFORM=web
#     make PLATFORM=win
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  NATIVE_PLATFORM := mac
else
  NATIVE_PLATFORM := linux
endif
PLATFORM ?= $(NATIVE_PLATFORM)

# ディレクトリ
SRC_DIR    := src
LIB_DIR    := $(SRC_DIR)/lib
IMGUI_DIR  := $(SRC_DIR)/lib/imgui

ifeq ($(PLATFORM),web)
  # ---- Web (Emscripten) ----
  CXX      := em++
  CC       := emcc
  TARGET   := web_build/index.html
  OBJ_DIR  := obj/web
  CXXFLAGS := -std=c++11 -Wall -DSOKOL_GLES3 -O2 -I$(IMGUI_DIR)
  CFLAGS   := -O2
  LDFLAGS  := -sUSE_WEBGL2=1 -sWASM=1 -sALLOW_MEMORY_GROWTH=1 \
               -sEXPORTED_RUNTIME_METHODS=ccall \
               --preload-file assets/rom.bin \
               --preload-file assets/ui_font.ttf \
               --preload-file sdcard.vhd \
               --shell-file src/shell.html
  # Web: mm / linux / win 実装は除外
  SRCS_CPP := $(filter-out $(SRC_DIR)/sokol_impl_linux.cpp $(SRC_DIR)/sokol_impl_win.cpp, \
                $(wildcard $(SRC_DIR)/*.cpp))
  SRCS_MM  :=
  CLEAN_EXTRA := web_build/fxt65.js web_build/fxt65.wasm web_build/fxt65.data
else ifeq ($(PLATFORM),mac)
  # ---- macOS (Metal) ----
  CXX      := clang++
  CC       := cc
  OBJCXX   := clang++
  TARGET   := fxt65
  OBJ_DIR  := obj/mac
  CXXFLAGS := -std=c++11 -Wall -DSOKOL_METAL -I$(IMGUI_DIR)
  CFLAGS   := -O2
  LDFLAGS  := -framework Cocoa -framework Metal -framework MetalKit \
               -framework QuartzCore -framework AudioToolbox -framework CoreText
  # mac: web / linux / win 実装は除外
  SRCS_CPP := $(filter-out $(SRC_DIR)/sokol_impl_web.cpp \
                           $(SRC_DIR)/sokol_impl_linux.cpp \
                           $(SRC_DIR)/sokol_impl_win.cpp, \
                $(wildcard $(SRC_DIR)/*.cpp))
  SRCS_MM  := $(wildcard $(SRC_DIR)/*.mm)
  CLEAN_EXTRA :=
else ifeq ($(PLATFORM),linux)
  # ---- Linux (X11 + GL) ----
  CXX      := g++
  CC       := cc
  TARGET   := fxt65
  OBJ_DIR  := obj/linux
  CXXFLAGS := -std=c++11 -Wall -O2 -DSOKOL_GLCORE -I$(IMGUI_DIR) -pthread
  CFLAGS   := -O2 -pthread
  LDFLAGS  := -pthread -lX11 -lXi -lXcursor -lGL -ldl -lm -lasound
  # linux: web / mac(mm) / win 実装は除外
  SRCS_CPP := $(filter-out $(SRC_DIR)/sokol_impl_web.cpp \
                           $(SRC_DIR)/sokol_impl_win.cpp, \
                $(wildcard $(SRC_DIR)/*.cpp))
  SRCS_MM  :=
  CLEAN_EXTRA :=
else ifeq ($(PLATFORM),win)
  # ---- Windows (MinGW-w64 クロスビルド) ----
  MINGW_PREFIX ?= x86_64-w64-mingw32
  CXX      := $(MINGW_PREFIX)-g++
  CC       := $(MINGW_PREFIX)-gcc
  TARGET   := fxt65.exe
  OBJ_DIR  := obj/win
  # VR_EMU_6502_STATIC: vrEmu6502.h の __declspec(dllimport) を無効化し静的リンクする
  CXXFLAGS := -std=c++11 -Wall -O2 -DSOKOL_D3D11 -DVR_EMU_6502_STATIC \
              -I$(IMGUI_DIR) -D_WIN32_WINNT=0x0601
  CFLAGS   := -O2 -DVR_EMU_6502_STATIC
  # スタンドアロン配布 & Windows サブシステム
  LDFLAGS  := -static -static-libgcc -static-libstdc++ -mwindows \
              -lkernel32 -luser32 -lshell32 -lgdi32 -lole32 -lcomdlg32 \
              -ld3d11 -ldxgi -ld3dcompiler -lwinmm
  # win: web / mac(mm) / linux 実装は除外
  SRCS_CPP := $(filter-out $(SRC_DIR)/sokol_impl_web.cpp \
                           $(SRC_DIR)/sokol_impl_linux.cpp, \
                $(wildcard $(SRC_DIR)/*.cpp))
  SRCS_MM  :=
  CLEAN_EXTRA :=
else
  $(error Unknown PLATFORM: $(PLATFORM). Use one of: mac, linux, win, web)
endif

# ROM
ROM_SRC := sd-monitor
ROM     := assets/rom.bin

# ----------
#  自動探索
# ----------

# ソースファイルをリストアップ
SRCS_C      := $(wildcard $(LIB_DIR)/*.c)
SRCS_IMGUI  := $(IMGUI_DIR)/imgui.cpp \
               $(IMGUI_DIR)/imgui_draw.cpp \
               $(IMGUI_DIR)/imgui_tables.cpp \
               $(IMGUI_DIR)/imgui_widgets.cpp

# オブジェクトファイルのパスを生成
OBJS_CPP   := $(SRCS_CPP:%.cpp=$(OBJ_DIR)/%.o)
OBJS_MM    := $(SRCS_MM:%.mm=$(OBJ_DIR)/%.o)
OBJS_C     := $(SRCS_C:%.c=$(OBJ_DIR)/%.o)
OBJS_IMGUI := $(SRCS_IMGUI:%.cpp=$(OBJ_DIR)/%.o)

# オブジェクトファイルのリスト
OBJS     := $(OBJS_CPP) $(OBJS_MM) $(OBJS_C) $(OBJS_IMGUI)

# ----------
#   ビルド
# ----------

UI_FONT := assets/ui_font.ttf

# Web ビルドは UI フォントサブセットにも依存する
ifeq ($(PLATFORM),web)
EXTRA_DEPS := $(UI_FONT)
else
EXTRA_DEPS :=
endif

# リンク
$(TARGET): $(OBJS) $(ROM) $(EXTRA_DEPS)
	@echo "Linking $@"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Build Complete."

# UI フォントサブセット生成（Ui.cpp が変更されると自動再生成）
$(UI_FONT): assets/ipaexg.ttf tools/make_ui_font_subset.py src/Ui.cpp
	@echo "Generating UI font subset..."
	@python3 tools/make_ui_font_subset.py

# ROM ビルド: サブモジュールから assets/rom.bin を生成
$(ROM): $(ROM_SRC)/rom.bin
	cp $< $@

$(ROM_SRC)/rom.bin:
	$(MAKE) -C $(ROM_SRC)

# C++
$(OBJ_DIR)/%.o: %.cpp
	@echo "Compiling C++: $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Objective-C++
$(OBJ_DIR)/%.o: %.mm
	@echo "Compiling ObjC++: $<"
	@mkdir -p $(dir $@)
	@$(OBJCXX) $(CXXFLAGS) -ObjC++ -c $< -o $@

# C
$(OBJ_DIR)/%.o: %.c
	@echo "Compiling C  : $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# OS ビルド: miracos サブモジュールから bin/ を生成
OS_SRC := miracos

os:
	cd $(OS_SRC) && bash ./makeos.sh

# ROM を強制再ビルド
rom:
	$(MAKE) -C $(ROM_SRC)
	cp $(ROM_SRC)/rom.bin $(ROM)

# クリーンアップ
clean:
	@echo "Cleaning up."
	@rm -rf obj/ web_build/ fxt65 fxt65.exe

# ----------
#  SDカード イメージ
# ----------
#
# クリーンビルド (miracos/bin/ から新規作成):
#   make vhd       → sdcard.vhd を作成（中間 .img は /tmp に sparse、自動削除）
#   make img       → sdcard.img を作成（4 GiB sparse、編集可能状態）
#
# 変換:
#   make img2vhd   → 既存 sdcard.img を sdcard.vhd に変換
#   make vhd2img   → 既存 sdcard.vhd を sdcard.img に展開（編集用）
#
# 編集ワークフロー例:
#   make vhd2img
#   hdiutil attach -imagekey diskimage-class=CRawDiskImage sdcard.img
#     ↓ ファイル編集 ↓
#   hdiutil detach /Volumes/MCOS
#   make img2vhd

# クリーンビルド: miracos/bin/ から sdcard.vhd を新規作成
# 中間 .img は /tmp に sparse で作り、変換後に自動削除
vhd: tools/to_dynamic_vhd.py mksd.sh
	@TMP=$$(mktemp -t sdcard.XXXXXX.img); \
	  trap "rm -f $$TMP" EXIT INT TERM; \
	  ./mksd.sh $$TMP && \
	  python3 tools/to_dynamic_vhd.py $$TMP sdcard.vhd
	@echo "作成完了: sdcard.vhd (Dynamic VHD)"

# クリーンビルド: miracos/bin/ から sdcard.img を新規作成 (4 GiB sparse)
img: mksd.sh
	./mksd.sh sdcard.img
	@echo "作成完了: sdcard.img (4 GiB sparse)"

# sdcard.img → sdcard.vhd (Dynamic VHD) に変換
# Dynamic VHD は未使用ブロック（2MB 単位）を省略し 4GB → 数 MB に圧縮できる
img2vhd: tools/to_dynamic_vhd.py sdcard.img
	python3 tools/to_dynamic_vhd.py sdcard.img sdcard.vhd
	@echo "変換完了: sdcard.vhd (Dynamic VHD)"

# sdcard.vhd → sdcard.img (フラット sparse) に展開（macOS 編集用）
vhd2img: tools/vhd_to_img.py sdcard.vhd
	python3 tools/vhd_to_img.py sdcard.vhd sdcard.img
	@echo "展開完了: sdcard.img"
	@echo "マウント:   hdiutil attach -imagekey diskimage-class=CRawDiskImage sdcard.img"

.PHONY: clean rom vhd img img2vhd vhd2img os
