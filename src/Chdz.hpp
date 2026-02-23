/* src/Chdz.hpp - Chiina-Dazzler CRTC エミュレーション */
#pragma once
#include <cstdint>

namespace Chdz
{

  // VRAM サイズ定数
  static constexpr int VRAM_FRAMES   = 4;
  static constexpr int VRAM_FRAME_SZ = 32768; // 2^15 32KB/フレーム
  static constexpr int DISPLAY_W     = 256;
  static constexpr int DISPLAY_H     = 768;   // 192論理行 × 4サブ行

  // CRTC 内部状態 (ChiinaDazzler.vhd)
  struct State
  {
    // VRAM: [frame][byte], アドレス = cursor[14:0]
    uint8_t vram[VRAM_FRAMES][VRAM_FRAME_SZ] = {};

    // --- CONF コマンドで設定されるレジスタ ---
    // WF コマンド: 書き込みフレーム選択
    uint8_t write_frame = 0;
    // TT コマンド: フレームごとのカラーモード (false=16色, true=2色)
    bool frame_ttmode[VRAM_FRAMES] = {};
    // T0/T1 コマンド: 2色モードパレット
    uint8_t tt_color_0 = 0;
    uint8_t tt_color_1 = 0;

    // --- DISP ($E605) ---
    // サブ行ごとの表示フレーム
    uint8_t read_frame[4] = {0, 0, 0, 0};

    // --- 書き込みカーソル (15bit: [14:7]=row, [6:0]=col) ---
    uint16_t cursor = 0;

    // --- Charbox ---
    bool    charbox_disable = true;  // CHRW bit7: true=シンプル+1, false=charboxモード
    int     charbox_width   = 0;     // CHRW bits[6:0]
    int     charbox_height  = 0;     // CHRH bits[7:0]
    uint8_t charbox_base_x  = 0;     // PTRX 設定時に保存 (右端折り返し基準X)
    uint8_t charbox_top_y   = 0;     // PTRY 設定時に保存 (下端折り返し基準Y)
    int     charbox_width_counter  = 0;
    int     charbox_height_counter = 0;

    // --- REPT ($E601) 用: 最後の書き込みデータ ---
    uint8_t last_wdat = 0;
  };

  // --- API ---

  // $E600-$E607 への書き込み
  void Write(State& chdz, uint16_t addr, uint8_t val);

  // 256×768 RGBA8888 バッファにレンダリング
  // pixels: 256*768*4 ピクセルの配列
  void RenderFrame(const State& chdz, uint32_t* pixels);

}
