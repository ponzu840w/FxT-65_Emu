# Makefile

# ----------
#    設定
# ----------

PLATFORM ?= native

# ディレクトリ
SRC_DIR    := src
LIB_DIR    := $(SRC_DIR)/lib
IMGUI_DIR  := $(SRC_DIR)/lib/imgui

ifeq ($(PLATFORM),web)
  CXX      := em++
  CC       := emcc
  TARGET   := web_build/fxt65.html
  OBJ_DIR  := obj/web
  CXXFLAGS := -std=c++11 -Wall -DSOKOL_GLES3 -O2 -I$(IMGUI_DIR)
  CFLAGS   := -O2
  LDFLAGS  := -sUSE_WEBGL2=1 -sWASM=1 -sALLOW_MEMORY_GROWTH=1 \
               -sEXPORTED_RUNTIME_METHODS=ccall \
               --preload-file assets/rom.bin \
               --preload-file assets/ipaexg.ttf \
               --preload-file sdcard.vhd \
               --shell-file src/shell.html
  SRCS_CPP := $(wildcard $(SRC_DIR)/*.cpp)
  SRCS_MM  :=
  CLEAN_EXTRA := web_build/fxt65.js web_build/fxt65.wasm web_build/fxt65.data
else
  CXX      := clang++
  CC       := cc
  OBJCXX   := clang++
  TARGET   := fxt65
  OBJ_DIR  := obj/mac
  CXXFLAGS := -std=c++11 -Wall -DSOKOL_METAL -I$(IMGUI_DIR)
  CFLAGS   := -O2
  LDFLAGS  := -framework Cocoa -framework Metal -framework MetalKit \
               -framework QuartzCore -framework AudioToolbox
  SRCS_CPP := $(filter-out $(SRC_DIR)/sokol_impl_web.cpp, $(wildcard $(SRC_DIR)/*.cpp))
  SRCS_MM  := $(wildcard $(SRC_DIR)/*.mm)
  CLEAN_EXTRA :=
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

# リンク
$(TARGET): $(OBJS) $(ROM)
	@echo "Linking $@"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Build Complete."

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
	cd $(OS_SRC) && ./makeos.sh

# ROM を強制再ビルド
rom:
	$(MAKE) -C $(ROM_SRC)
	cp $(ROM_SRC)/rom.bin $(ROM)

# クリーンアップ
clean:
	@echo "Cleaning up."
	@rm -rf obj/ web_build/ fxt65

# ----------
#  SDカード イメージ変換
# ----------
#
# ワークフロー:
#   1. ./mksd.sh          : sdcard.img を新規作成（FAT32クラスタサイズ32セクタ等の制約を満たす）
#   2. make vhd           : sdcard.img → sdcard.vhd (Dynamic VHD, ネイティブ・Web 共用)
#   3. make img           : sdcard.vhd → sdcard.img (macOS で編集する際に展開)
#                           ↓ hdiutil attach -imagekey diskimage-class=CRawDiskImage sdcard.img
#                           ↓ ファイル編集
#                           ↓ hdiutil detach /Volumes/MCOS
#   4. make vhd           : 編集後の sdcard.img を再度 VHD に変換

# sdcard.img → sdcard.vhd (Dynamic VHD) に変換
# Dynamic VHD は未使用ブロック（2MB 単位）を省略し 2GB → 数 MB に圧縮できる
vhd: tools/to_dynamic_vhd.py sdcard.img
	python3 tools/to_dynamic_vhd.py sdcard.img sdcard.vhd
	@echo "作成完了: sdcard.vhd (Dynamic VHD)"

# sdcard.vhd → sdcard.img (フラット) に展開（macOS 編集用）
img: tools/vhd_to_img.py sdcard.vhd
	python3 tools/vhd_to_img.py sdcard.vhd sdcard.img
	@echo "展開完了: sdcard.img"
	@echo "マウント:   hdiutil attach -imagekey diskimage-class=CRawDiskImage sdcard.img"

.PHONY: clean rom vhd img os
