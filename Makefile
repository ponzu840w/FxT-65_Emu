# Makefile

# ----------
#    設定
# ----------

# コンパイラ
CXX       := clang++
CC        := cc
OBJCXX    := clang++

# コンパイルオプション
CXXFLAGS  := -std=c++11 -Wall -DSOKOL_METAL
CFLAGS    := -O2

# ディレクトリ
SRC_DIR   := src
OBJ_DIR   := obj
LIB_DIR   := $(SRC_DIR)/lib

# ターゲット
TARGET = fxt65

# macOS フレームワーク
LDFLAGS   := -framework Cocoa -framework Metal -framework MetalKit -framework QuartzCore -framework AudioToolbox

# ----------
#  自動探索
# ----------

# ソースファイルをリストアップ
SRCS_CPP := $(wildcard $(SRC_DIR)/*.cpp)
SRCS_MM  := $(wildcard $(SRC_DIR)/*.mm)
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
	@rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: clean
