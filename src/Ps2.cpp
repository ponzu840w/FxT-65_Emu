/* src/Ps2.cpp - PS/2キーボードエミュレータ実装 */

#ifndef SOKOL_METAL
#define SOKOL_METAL
#endif
#include "lib/sokol/sokol_app.h"

#include "Ps2.hpp"
#include "FxtSystem.hpp"
#include "Via.hpp"

namespace Ps2
{

// ---------------------------------------------------------------
//  Sokolキーコード → PS/2 Set 2 スキャンコード テーブル
// ---------------------------------------------------------------
struct Ps2Key
{
  bool    extended; // true=E0プレフィックスが必要
  uint8_t code;     // 0=未対応
};

static Ps2Key keycode_to_ps2(int kc)
{
  switch (kc)
  {
    // --- 記号 ---
    case SAPP_KEYCODE_SPACE:          return {false, 0x29};
    case SAPP_KEYCODE_APOSTROPHE:     return {false, 0x52};
    case SAPP_KEYCODE_COMMA:          return {false, 0x41};
    case SAPP_KEYCODE_MINUS:          return {false, 0x4E};
    case SAPP_KEYCODE_PERIOD:         return {false, 0x49};
    case SAPP_KEYCODE_SLASH:          return {false, 0x4A};
    case SAPP_KEYCODE_SEMICOLON:      return {false, 0x4C};
    case SAPP_KEYCODE_EQUAL:          return {false, 0x55};
    case SAPP_KEYCODE_LEFT_BRACKET:   return {false, 0x54};
    case SAPP_KEYCODE_BACKSLASH:      return {false, 0x5D};
    case SAPP_KEYCODE_RIGHT_BRACKET:  return {false, 0x5B};
    case SAPP_KEYCODE_GRAVE_ACCENT:   return {false, 0x0E};
    // --- 数字 ---
    case SAPP_KEYCODE_0:              return {false, 0x45};
    case SAPP_KEYCODE_1:              return {false, 0x16};
    case SAPP_KEYCODE_2:              return {false, 0x1E};
    case SAPP_KEYCODE_3:              return {false, 0x26};
    case SAPP_KEYCODE_4:              return {false, 0x25};
    case SAPP_KEYCODE_5:              return {false, 0x2E};
    case SAPP_KEYCODE_6:              return {false, 0x36};
    case SAPP_KEYCODE_7:             return {false, 0x3D};
    case SAPP_KEYCODE_8:             return {false, 0x3E};
    case SAPP_KEYCODE_9:             return {false, 0x46};
    // --- アルファベット ---
    case SAPP_KEYCODE_A:             return {false, 0x1C};
    case SAPP_KEYCODE_B:             return {false, 0x32};
    case SAPP_KEYCODE_C:             return {false, 0x21};
    case SAPP_KEYCODE_D:             return {false, 0x23};
    case SAPP_KEYCODE_E:             return {false, 0x24};
    case SAPP_KEYCODE_F:             return {false, 0x2B};
    case SAPP_KEYCODE_G:             return {false, 0x34};
    case SAPP_KEYCODE_H:             return {false, 0x33};
    case SAPP_KEYCODE_I:             return {false, 0x43};
    case SAPP_KEYCODE_J:             return {false, 0x3B};
    case SAPP_KEYCODE_K:             return {false, 0x42};
    case SAPP_KEYCODE_L:             return {false, 0x4B};
    case SAPP_KEYCODE_M:             return {false, 0x3A};
    case SAPP_KEYCODE_N:             return {false, 0x31};
    case SAPP_KEYCODE_O:             return {false, 0x44};
    case SAPP_KEYCODE_P:             return {false, 0x4D};
    case SAPP_KEYCODE_Q:             return {false, 0x15};
    case SAPP_KEYCODE_R:             return {false, 0x2D};
    case SAPP_KEYCODE_S:             return {false, 0x1B};
    case SAPP_KEYCODE_T:             return {false, 0x2C};
    case SAPP_KEYCODE_U:             return {false, 0x3C};
    case SAPP_KEYCODE_V:             return {false, 0x2A};
    case SAPP_KEYCODE_W:             return {false, 0x1D};
    case SAPP_KEYCODE_X:             return {false, 0x22};
    case SAPP_KEYCODE_Y:             return {false, 0x35};
    case SAPP_KEYCODE_Z:             return {false, 0x1A};
    // --- 制御キー ---
    case SAPP_KEYCODE_ESCAPE:        return {false, 0x76};
    case SAPP_KEYCODE_ENTER:         return {false, 0x5A};
    case SAPP_KEYCODE_TAB:           return {false, 0x0D};
    case SAPP_KEYCODE_BACKSPACE:     return {false, 0x66};
    case SAPP_KEYCODE_CAPS_LOCK:     return {false, 0x58};
    case SAPP_KEYCODE_SCROLL_LOCK:   return {false, 0x7E};
    case SAPP_KEYCODE_NUM_LOCK:      return {false, 0x77};
    // --- ファンクションキー ---
    case SAPP_KEYCODE_F1:            return {false, 0x05};
    case SAPP_KEYCODE_F2:            return {false, 0x06};
    case SAPP_KEYCODE_F3:            return {false, 0x04};
    case SAPP_KEYCODE_F4:            return {false, 0x0C};
    case SAPP_KEYCODE_F5:            return {false, 0x03};
    case SAPP_KEYCODE_F6:            return {false, 0x0B};
    case SAPP_KEYCODE_F7:            return {false, 0x83};
    case SAPP_KEYCODE_F8:            return {false, 0x0A};
    case SAPP_KEYCODE_F9:            return {false, 0x01};
    case SAPP_KEYCODE_F10:           return {false, 0x09};
    case SAPP_KEYCODE_F11:           return {false, 0x78};
    case SAPP_KEYCODE_F12:           return {false, 0x07};
    // --- モディファイアキー ---
    case SAPP_KEYCODE_LEFT_SHIFT:    return {false, 0x12};
    case SAPP_KEYCODE_RIGHT_SHIFT:   return {false, 0x59};
    case SAPP_KEYCODE_LEFT_CONTROL:  return {false, 0x14};
    case SAPP_KEYCODE_RIGHT_CONTROL: return {true,  0x14};
    case SAPP_KEYCODE_LEFT_ALT:      return {false, 0x11};
    case SAPP_KEYCODE_RIGHT_ALT:     return {true,  0x11};
    case SAPP_KEYCODE_LEFT_SUPER:    return {true,  0x1F};
    case SAPP_KEYCODE_RIGHT_SUPER:   return {true,  0x27};
    case SAPP_KEYCODE_MENU:          return {true,  0x2F};
    // --- ナビゲーションキー (拡張) ---
    case SAPP_KEYCODE_INSERT:        return {true,  0x70};
    case SAPP_KEYCODE_DELETE:        return {true,  0x71};
    case SAPP_KEYCODE_HOME:          return {true,  0x6C};
    case SAPP_KEYCODE_END:           return {true,  0x69};
    case SAPP_KEYCODE_PAGE_UP:       return {true,  0x7D};
    case SAPP_KEYCODE_PAGE_DOWN:     return {true,  0x7A};
    case SAPP_KEYCODE_RIGHT:         return {true,  0x74};
    case SAPP_KEYCODE_LEFT:          return {true,  0x6B};
    case SAPP_KEYCODE_DOWN:          return {true,  0x72};
    case SAPP_KEYCODE_UP:            return {true,  0x75};
    // --- テンキー ---
    case SAPP_KEYCODE_KP_0:          return {false, 0x70};
    case SAPP_KEYCODE_KP_1:          return {false, 0x69};
    case SAPP_KEYCODE_KP_2:          return {false, 0x72};
    case SAPP_KEYCODE_KP_3:          return {false, 0x7A};
    case SAPP_KEYCODE_KP_4:          return {false, 0x6B};
    case SAPP_KEYCODE_KP_5:          return {false, 0x73};
    case SAPP_KEYCODE_KP_6:          return {false, 0x74};
    case SAPP_KEYCODE_KP_7:          return {false, 0x6C};
    case SAPP_KEYCODE_KP_8:          return {false, 0x75};
    case SAPP_KEYCODE_KP_9:          return {false, 0x7D};
    case SAPP_KEYCODE_KP_DECIMAL:    return {false, 0x71};
    case SAPP_KEYCODE_KP_DIVIDE:     return {true,  0x4A};
    case SAPP_KEYCODE_KP_MULTIPLY:   return {false, 0x7C};
    case SAPP_KEYCODE_KP_SUBTRACT:   return {false, 0x7B};
    case SAPP_KEYCODE_KP_ADD:        return {false, 0x79};
    case SAPP_KEYCODE_KP_ENTER:      return {true,  0x5A};
    default:                         return {false, 0x00}; // 未対応
  }
}

// ---------------------------------------------------------------
//  内部ユーティリティ
// ---------------------------------------------------------------
static void queue_byte(State& ps2, uint8_t b)
{
  int next = (ps2.q_tail + 1) % QUEUE_SIZE;
  if (next != ps2.q_head) // キューが満杯でなければ積む
  {
    ps2.queue[ps2.q_tail] = b;
    ps2.q_tail = next;
  }
}

// ---------------------------------------------------------------
//  KeyDown / KeyUp
// ---------------------------------------------------------------
void KeyDown(State& ps2, int sapp_keycode)
{
  Ps2Key k = keycode_to_ps2(sapp_keycode);
  if (k.code == 0) return;
  if (k.extended) queue_byte(ps2, 0xE0);
  queue_byte(ps2, k.code);
}

void KeyUp(State& ps2, int sapp_keycode)
{
  Ps2Key k = keycode_to_ps2(sapp_keycode);
  if (k.code == 0) return;
  if (k.extended) queue_byte(ps2, 0xE0);
  queue_byte(ps2, 0xF0);
  queue_byte(ps2, k.code);
}

// ---------------------------------------------------------------
//  GetPortBBits
// ---------------------------------------------------------------
uint8_t GetPortBBits(const State& ps2)
{
  uint8_t val = 0;
  if (ps2.clk) val |= CLK_BIT;
  if (ps2.dat) val |= DAT_BIT;
  return val;
}

// ---------------------------------------------------------------
//  Tick - 1CPUサイクル分のPS/2ステートマシン実行
// ---------------------------------------------------------------
void Tick(Fxt::System& sys)
{
  State&            ps2 = sys.ps2;
  const Fxt::Via::State& via = sys.via;

  // ホストがCLKを出力LOWにしていたらデバイス送信禁止 (inhibit)
  bool inhibited = (via.reg_ddrb & CLK_BIT) && !(via.reg_orb & CLK_BIT);

  if (ps2.phase == TxPhase::IDLE)
  {
    if (inhibited || ps2.q_head == ps2.q_tail)
    {
      ps2.clk = true;
      ps2.dat = true;
      return;
    }
    // 新フレーム開始: バイトをキューから取り出す
    ps2.current_byte = ps2.queue[ps2.q_head];
    ps2.q_head = (ps2.q_head + 1) % QUEUE_SIZE;
    ps2.bit_idx  = 0;
    // 奇数パリティ: 1ビット数が偶数なら parity_bit=1
    int ones = 0;
    for (int i = 0; i < 8; i++) ones += (ps2.current_byte >> i) & 1;
    ps2.parity_bit = (ones & 1) ? 0 : 1;

    // スタートビット: CLK=LOW, DAT=LOW
    ps2.clk = false;
    ps2.dat = false;
    ps2.phase           = TxPhase::CLK_LOW;
    ps2.half_period_cnt = HALF_PERIOD;
    return;
  }

  // タイマー進行
  ps2.half_period_cnt--;
  if (ps2.half_period_cnt > 0) return;
  ps2.half_period_cnt = HALF_PERIOD;

  if (ps2.phase == TxPhase::CLK_LOW)
  {
    // CLK LOW 期間終了 → CLK HIGH へ
    ps2.clk   = true;
    ps2.phase = TxPhase::CLK_HIGH;
  }
  else // CLK_HIGH 期間終了 → 次ビットへ
  {
    ps2.bit_idx++;

    if (ps2.bit_idx <= 8)
    {
      // データビット (LSBファースト)
      bool bit = (ps2.current_byte >> (ps2.bit_idx - 1)) & 1;
      ps2.dat  = bit;
    }
    else if (ps2.bit_idx == 9)
    {
      // パリティビット
      ps2.dat = (ps2.parity_bit != 0);
    }
    else if (ps2.bit_idx == 10)
    {
      // ストップビット (HIGH=1)
      ps2.dat = true;
    }
    else
    {
      // フレーム完了 → IDLE
      ps2.phase = TxPhase::IDLE;
      ps2.clk   = true;
      ps2.dat   = true;
      return;
    }

    ps2.clk   = false;
    ps2.phase = TxPhase::CLK_LOW;
  }
}

} // namespace Ps2
