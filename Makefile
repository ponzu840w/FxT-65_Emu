# Makefile

# ----------
#    設定
# ----------

# コンパイラ
CXX       := g++
CC        := cc

# コンパイルオプション
CXXFLAGS  := -std=c++11 -Wall
CFLAGS    := -O2

# ディレクトリ
SRC_DIR   := src
OBJ_DIR   := obj
LIB_DIR   := $(SRC_DIR)/lib

# ターゲット
TARGET = fxt65

# ----------
#  自動探索
# ----------

# ソースファイルをリストアップ
SRCS_CPP := $(wildcard $(SRC_DIR)/*.cpp)
SRCS_C   := $(wildcard $(LIB_DIR)/*.c)

# オブジェクトファイルのパスを生成
OBJS_CPP := $(SRCS_CPP:%.cpp=$(OBJ_DIR)/%.o)
OBJS_C   := $(SRCS_C:%.c=$(OBJ_DIR)/%.o)

# オブジェクトファイルのリスト
OBJS     := $(OBJS_CPP) $(OBJS_C)

# ----------
#   ビルド
# ----------

# リンク
$(TARGET): $(OBJS)
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "Build Complete."

# C++
$(OBJ_DIR)/%.o: %.cpp
	@echo "Compiling C++: $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

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

