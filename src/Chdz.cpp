/* src/Chdz.cpp - Chiina-Dazzler CRTC エミュレーション
 *
 * レジスタマップ ($E600-$E607):
 *   $E600 CONF  コンフィグ [7:4]=cmd, [3:0]=data
 *               cmd=0x0 WF: write_frame = data[1:0]
 *               cmd=0x1 TT: frame_ttmode[write_frame] = data[0]
 *               cmd=0x2 T0: tt_color_0 = data[3:0]
 *               cmd=0x3 T1: tt_color_1 = data[3:0]
 *   $E601 REPT  最後のWDATを再書き込み (カーソル+1)
 *   $E602 PTRX  書き込みX座標 [6:0] (charboxカウンタリセット)
 *   $E603 PTRY  書き込みY座標 [7:0]
 *   $E604 WDAT  VRAM書き込み (カーソル自動進め)
 *   $E605 DISP  表示フレーム: [7:6]=sub0, [5:4]=sub1, [3:2]=sub2, [1:0]=sub3
 *   $E606 CHRW  charbox: [7]=disable, [6:0]=width
 *   $E607 CHRH  charbox: [7:0]=height
 *
 * カラーパレット:
 *   インデックス = {R, B, G1, G0}
 *   bit3=R, bit2=B, bit[1:0]=G (0..3 → 0,85,170,255)
 *
 * VRAMアドレス (15bit):
 *   cursor[14:7]=row, cursor[6:0]=col_byte
 *   16色: 128 bytes/行 (2 px/byte)
 *   2色:  128 bytes/行 (8 px/byte) 行後部96バイトは不使用
 */
#include "Chdz.hpp"
#include <cstring>

namespace Chdz
{

// 色インデックス -> rgb121{R, B, G1, G0} 変換
static uint32_t MakePaletteEntry(uint8_t idx)
{
  uint8_t r = (idx & 0x08) ? 255 : 0;   // bit3 = R
  uint8_t b = (idx & 0x04) ? 255 : 0;   // bit2 = B
  uint8_t g = (idx & 0x03) * 85;        // bit[1:0] = G (0,85,170,255)
  // RGBA8888
  return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | (0xFFu << 24);
}

// RGB121 パレット
static const uint32_t* GetPalette()
{
  static uint32_t palette[16];
  static bool initialized = false;
  if (!initialized)
  {
    for (int i = 0; i < 16; i++) palette[i] = MakePaletteEntry((uint8_t)i);
    initialized = true;
  }
  return palette;
}

// ---------------------------------------------------------------
//  DoWrite  実際のVRAM書き込み + カーソル進め
//  WDAT と REPT の両方から呼ばれる
// ---------------------------------------------------------------
static void DoWrite(State& chdz)
{
  // VRAM書き込み
  uint32_t addr = chdz.cursor & 0x7FFF;
  if (addr < VRAM_FRAME_SZ)
    chdz.vram[chdz.write_frame][addr] = chdz.last_wdat;

  // カーソルを進める
  if (!chdz.charbox_disable && chdz.charbox_width_counter == chdz.charbox_width)
  {
    // CharBoxの右端を超過
    chdz.charbox_width_counter = 0;

    if (chdz.charbox_height_counter == chdz.charbox_height)
    {
      // さらに下端も超過：右上に移動し次のキャラクタに移る
      // charbox_next_x = cursor[6:0] + 1
      uint8_t next_x = (uint8_t)((chdz.cursor & 0x7F) + 1);
      chdz.cursor = ((uint16_t)chdz.charbox_top_y << 7) | next_x;
      chdz.charbox_base_x = next_x;
      chdz.charbox_height_counter = 0;
    }
    else
    {
      // まだ下端ではない：次の行の左端に移動
      uint8_t row = (uint8_t)((chdz.cursor >> 7) + 1);
      chdz.cursor = ((uint16_t)row << 7) | chdz.charbox_base_x;
      chdz.charbox_height_counter++;
    }
  }
  else
  {
    // ただ単にVRAMの次のアドレスに移動
    chdz.cursor = (chdz.cursor + 1) & 0x7FFF;
    chdz.charbox_width_counter++;
  }
}

// ---------------------------------------------------------------
//  Write  $E600-$E607
// ---------------------------------------------------------------
void Write(State& chdz, uint16_t addr, uint8_t val)
{
  switch (addr & 0x000F)
  {
    case 0x00: // CONF ($E600) コンフィグ
      {
        uint8_t cmd = (val >> 4) & 0x0F;
        uint8_t dat = val & 0x0F;
        switch (cmd)
        {
          case 0x0: // WF: 書き込みフレーム選択
            chdz.write_frame = dat & 0x03;
            break;
          case 0x1: // TT: フレームの色モード選択 (0=16色, 1=2色)
            chdz.frame_ttmode[chdz.write_frame] = (dat & 0x01) != 0;
            break;
          case 0x2: // T0: 2色パレット0
            chdz.tt_color_0 = dat;
            break;
          case 0x3: // T1: 2色パレット1
            chdz.tt_color_1 = dat;
            break;
        }
      }
      break;

    case 0x01: // REPT ($E601) 最後のWDATを再書き込み
      DoWrite(chdz);
      break;

    case 0x02: // PTRX ($E602) X座標設定 + charboxカウンタリセット
      chdz.charbox_width_counter = 0;
      chdz.charbox_height_counter = 0;
      chdz.charbox_base_x = val & 0x7F;
      chdz.cursor = (chdz.cursor & 0x7F80) | (val & 0x7F);
      break;

    case 0x03: // PTRY ($E603) Y座標設定
      chdz.charbox_top_y = val;
      chdz.cursor = ((uint16_t)val << 7) | (chdz.cursor & 0x007F);
      break;

    case 0x04: // WDAT ($E604) VRAM書き込み
      chdz.last_wdat = val;
      DoWrite(chdz);
      break;

    case 0x05: // DISP ($E605) 表示フレーム選択
      // VHDL: disp_frame_by_lines_reg(0) <= disp_frame_bf_reg(7 downto 6);
      chdz.read_frame[0] = (val >> 6) & 0x03;  // sub0 = bits[7:6]
      chdz.read_frame[1] = (val >> 4) & 0x03;  // sub1 = bits[5:4]
      chdz.read_frame[2] = (val >> 2) & 0x03;  // sub2 = bits[3:2]
      chdz.read_frame[3] = (val >> 0) & 0x03;  // sub3 = bits[1:0]
      break;

    case 0x06: // CHRW ($E606) charbox幅設定
      chdz.charbox_disable = (val & 0x80) != 0;
      chdz.charbox_width   = val & 0x7F;
      break;

    case 0x07: // CHRH ($E607) charbox高さ設定
      chdz.charbox_height = val;
      break;
  }
}

// ---------------------------------------------------------------
//  RenderFrame  256×768 RGBA8888
//
//  VRAM layout:
//    16色: row * 128 + x/2  (上位ニブル=偶数px, 下位=奇数px)
//    2色:  row * 128 + x/8  (bit7=左端px)
// ---------------------------------------------------------------
void RenderFrame(const State& chdz, uint8_t* pixels)
{
  // カラーパレット
  const uint32_t* palette = GetPalette();

  // 画面Y軸方向 実ピクセル行 ループ
  for (int display_y = 0; display_y < DISPLAY_H; display_y++)
  {
    int sub_row  = display_y & 3;        // 0-3 論理行中のサブ行
    int vram_row = display_y >> 2;       // 0-191 論理行

    // 論理行 -> 表示フレームバッファ&色モード
    uint8_t frame = chdz.read_frame[sub_row];
    bool ttmode = chdz.frame_ttmode[frame]; // 色モード: false=16色 true=2色

    // フレームバッファ中の行データ
    const uint8_t* src = chdz.vram[frame] + (vram_row * 128);
    // 色データを書き込むべき場所
    uint32_t*  row_out = (uint32_t*)(pixels + (display_y * DISPLAY_W * 4));

    if (!ttmode)
    {
      // 16色モード: 1byte = 2px (上位ニブル=左, 下位ニブル=右)
      for (int x = 0; x < DISPLAY_W; x++)
      {
        uint8_t byte = src[x >> 1]; // 行中バイトオフセット=画面上X座標/2
        uint8_t cidx = (x & 1) ? (byte & 0x0F) : (byte >> 4); // 上位か下位のニブルを抽出
        row_out[x] = palette[cidx];
      }
    }
    else
    {
      // 2色モード: 1byte = 8 px (bit7=左端)
      for (int x = 0; x < DISPLAY_W; x++)
      {
        uint8_t byte = src[x >> 3]; // 行中バイトオフセット=画面上X座標/8
        uint8_t bit  = (byte >> (7 - (x & 7))) & 1; // 該当するビットを抽出
        row_out[x] = palette[bit ? chdz.tt_color_1 : chdz.tt_color_0];
      }
    }
  }
}

}
