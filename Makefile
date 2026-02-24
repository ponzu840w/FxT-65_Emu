# Makefile

# ----------
#    設定
# ----------

PLATFORM ?= native

# ディレクトリ
SRC_DIR   := src
LIB_DIR   := $(SRC_DIR)/lib

ifeq ($(PLATFORM),web)
  CXX      := em++
  CC       := emcc
  TARGET   := docs/fxt65.html
  OBJ_DIR  := obj_web
  CXXFLAGS := -std=c++11 -Wall -DSOKOL_GLES3 -O2
  CFLAGS   := -O2
  LDFLAGS  := -sUSE_WEBGL2=1 -sWASM=1 -sALLOW_MEMORY_GROWTH=1 \
               --preload-file assets/rom.bin \
               --preload-file sdcard.img
  SRCS_CPP := $(wildcard $(SRC_DIR)/*.cpp)
  SRCS_MM  :=
  CLEAN_EXTRA := docs/fxt65.js docs/fxt65.wasm docs/fxt65.data
else
  CXX      := clang++
  CC       := cc
  OBJCXX   := clang++
  TARGET   := fxt65
  OBJ_DIR  := obj
  CXXFLAGS := -std=c++11 -Wall -DSOKOL_METAL
  CFLAGS   := -O2
  LDFLAGS  := -framework Cocoa -framework Metal -framework MetalKit \
               -framework QuartzCore -framework AudioToolbox
  SRCS_CPP := $(filter-out $(SRC_DIR)/sokol_impl_web.cpp, $(wildcard $(SRC_DIR)/*.cpp))
  SRCS_MM  := $(wildcard $(SRC_DIR)/*.mm)
  CLEAN_EXTRA :=
endif

# ----------
#  自動探索
# ----------

# ソースファイルをリストアップ
SRCS_C   := $(wildcard $(LIB_DIR)/*.c)

# オブジェクトファイルのパスを生成
OBJS_CPP := $(SRCS_CPP:%.cpp=$(OBJ_DIR)/%.o)
OBJS_MM  := $(SRCS_MM:%.mm=$(OBJ_DIR)/%.o)
OBJS_C   := $(SRCS_C:%.c=$(OBJ_DIR)/%.o)

# オブジェクトファイルのリスト
OBJS     := $(OBJS_CPP) $(OBJS_MM) $(OBJS_C)

# ----------
#   ビルド
# ----------

# リンク
$(TARGET): $(OBJS)
	@echo "Linking $@"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build Complete."

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

# クリーンアップ
clean:
	@echo "Cleaning up."
	@rm -rf $(OBJ_DIR) $(TARGET) $(CLEAN_EXTRA)

# sdcard.img を SPRS スパース形式に変換（Web ビルド前に実行）
# 元のフラット形式は sdcard_flat.img として保存
sparse: tools/to_sparse.py
	@[ -f sdcard_flat.img ] || cp sdcard.img sdcard_flat.img
	python3 tools/to_sparse.py sdcard_flat.img sdcard.img
	@echo "sdcard.img を SPRS スパース形式に変換しました（元: sdcard_flat.img）"

.PHONY: clean sparse
