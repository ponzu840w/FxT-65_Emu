#!/usr/bin/env python3
"""
tools/make_ui_font_subset.py

src/Ui.cpp 内の文字列リテラルをスキャンして非 ASCII 文字を自動収集し、
最小限のフォントサブセットを生成する。

UI に日本語文字列を追加した場合は make で自動再生成される（Makefile 依存に記述）。

Usage:
    python3 tools/make_ui_font_subset.py
"""

import re
import sys
import os

# スキャン対象ソースファイル（ここに列挙したファイルが変更されると make が再生成する）
SOURCE_FILES = ["src/Ui.cpp"]

INPUT_FONT  = "assets/ipaexg.ttf"
OUTPUT_FONT = "assets/ui_font.ttf"


def collect_nonascii(files):
    """ソースファイル中の C/C++ 文字列リテラルから非 ASCII 文字を収集する"""
    chars = set()
    # "..." 形式の文字列リテラルを抽出（エスケープシーケンスを考慮）
    pattern = re.compile(r'"((?:[^"\\]|\\.)*)"')
    for path in files:
        with open(path, encoding="utf-8") as f:
            content = f.read()
        for m in pattern.finditer(content):
            for ch in m.group(1):
                if ord(ch) > 0x7F:
                    chars.add(ch)
    return chars


def main():
    # fontTools をインポート（未インストール時はエラーメッセージを表示）
    try:
        from fontTools import subset as ft_subset
        from fontTools.ttLib import TTFont
    except ImportError:
        print("Error: fonttools が見つかりません。")
        print("  pip install fonttools")
        sys.exit(1)

    if not os.path.exists(INPUT_FONT):
        print(f"Error: {INPUT_FONT} が見つかりません。")
        sys.exit(1)

    chars = collect_nonascii(SOURCE_FILES)
    if not chars:
        print("非 ASCII 文字が見つかりませんでした。サブセット生成をスキップします。")
        # 空ファイルでも OUTPUT_FONT を作成して make のタイムスタンプを更新
        open(OUTPUT_FONT, "wb").close()
        return

    print(f"収集した非 ASCII 文字 ({len(chars)} 文字): {''.join(sorted(chars))}")

    # ASCII 印字可能文字 (U+0020-U+007E) を常に含める
    # → 日本語フォント 1 本で ASCII も日本語もカバーし、metrics を統一する
    all_unicodes = set(ord(c) for c in chars) | set(range(0x0020, 0x007F))

    # fontTools.subset でサブセット生成
    options = ft_subset.Options()
    options.no_hinting       = True
    options.layout_features  = []   # GSUB/GPOS を省略してさらに圧縮

    font      = TTFont(INPUT_FONT)
    subsetter = ft_subset.Subsetter(options=options)
    subsetter.populate(unicodes=list(all_unicodes))
    subsetter.subset(font)

    os.makedirs(os.path.dirname(OUTPUT_FONT) or ".", exist_ok=True)
    font.save(OUTPUT_FONT)

    size = os.path.getsize(OUTPUT_FONT)
    print(f"サブセット保存: {OUTPUT_FONT}  ({size:,} bytes / {size / 1024:.1f} KB)")


if __name__ == "__main__":
    main()
