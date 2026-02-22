/* src/Ps2.cpp - PS/2キーボードエミュレータ実装 */

#ifndef SOKOL_METAL
#define SOKOL_METAL
#endif
#include "lib/sokol/sokol_app.h"

#include "Ps2.hpp"
#include "FxtSystem.hpp"
#include "Via.hpp"

//#define PS2_DEBUG 1
#if PS2_DEBUG
#include <cstdio>
#define PS2_LOG(...) fprintf(stderr, "[PS2] " __VA_ARGS__)
#else
#define PS2_LOG(...)
#endif

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
    PS2_LOG("Queued TX byte: %02X\n", b);
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

  // ホストのライン状態判定
  bool host_clk_low = (via.reg_ddrb & CLK_BIT) && !(via.reg_orb & CLK_BIT);
  bool host_dat_low = (via.reg_ddrb & DAT_BIT) && !(via.reg_orb & DAT_BIT);

  if (ps2.phase == Phase::IDLE)
  {
    if (host_clk_low) // ホストがクロックをLow: Inhibit
    {
#if PS2_DEBUG
      if (ps2.q_head != ps2.q_tail) {
        static int inhibit_cnt = 0;
        if (++inhibit_cnt % 50000 == 0)
          PS2_LOG("STALL: Inhibit継続中 queue=%d tx_delay=%d DDRB=%02X ORB=%02X\n",
                  (ps2.q_tail - ps2.q_head + QUEUE_SIZE) % QUEUE_SIZE,
                  ps2.tx_delay_cnt, via.reg_ddrb, via.reg_orb);
      }
#endif
      ps2.clk = true;
      ps2.dat = true;
      ps2.tx_delay_cnt = 400;
      return;
    }
    else if (host_dat_low)
    {
      PS2_LOG("Host RTS detected (DAT Low, CLK High). Starting RX phase.\n");
      // 送信要求 (RTS): CLKが解放され、DATがLowになっている
      ps2.phase = Phase::RX_CLK_LOW;
      ps2.half_period_cnt = HALF_PERIOD;
      ps2.clk = false;
      ps2.dat = true;
      ps2.bit_idx = 0;
      ps2.current_rx_byte = 0;
      return;
    }
    else if (ps2.q_head != ps2.q_tail)
    {
      if (ps2.tx_delay_cnt-- > 0) return;

      // デバイス送信 (TX) 開始
      ps2.current_tx_byte = ps2.queue[ps2.q_head];
      ps2.q_head = (ps2.q_head + 1) % QUEUE_SIZE;
      PS2_LOG("Starting TX phase for byte: %02X\n", ps2.current_tx_byte);
      ps2.bit_idx  = 0;

      int ones = 0;
      for (int i = 0; i < 8; i++) ones += (ps2.current_tx_byte >> i) & 1;
      ps2.parity_bit = (ones & 1) ? 0 : 1;

      ps2.clk = false;
      ps2.dat = false;
      ps2.phase = Phase::TX_CLK_LOW;
      ps2.half_period_cnt = HALF_PERIOD;
      return;
    }
    return; // アイドル維持
  }

  // タイマー進行
  ps2.half_period_cnt--;
  if (ps2.half_period_cnt > 0) return;
  ps2.half_period_cnt = HALF_PERIOD;

  // -------------------------------------------------
  //  送信 (TX: デバイス -> ホスト)
  // -------------------------------------------------
  if (ps2.phase == Phase::TX_CLK_LOW)
  {
    ps2.clk = true;
    ps2.phase = Phase::TX_CLK_HIGH;
  }
  else if (ps2.phase == Phase::TX_CLK_HIGH)
  {
    ps2.bit_idx++;
    if (ps2.bit_idx <= 8) {
      ps2.dat = (ps2.current_tx_byte >> (ps2.bit_idx - 1)) & 1;
    } else if (ps2.bit_idx == 9) {
      ps2.dat = (ps2.parity_bit != 0);
    } else if (ps2.bit_idx == 10) {
      ps2.dat = true; // Stop bit
    } else {
      PS2_LOG("TX completed for byte: %02X\n", ps2.current_tx_byte);
      ps2.phase = Phase::IDLE;
      ps2.clk = true; ps2.dat = true;
      return;
    }
    ps2.clk = false;
    ps2.phase = Phase::TX_CLK_LOW;
  }
  // -------------------------------------------------
  //  受信 (RX: ホスト -> デバイス)
  // -------------------------------------------------
  else if (ps2.phase == Phase::RX_CLK_LOW)
  {
    ps2.clk = true;
    ps2.phase = Phase::RX_CLK_HIGH;
  }
  else if (ps2.phase == Phase::RX_CLK_HIGH)
  {
    // クロックがHighの期間の中央でホストの出力をサンプリング
    bool host_dat = true;
    if (via.reg_ddrb & DAT_BIT)
      host_dat = (via.reg_orb & DAT_BIT) != 0;

    if (ps2.bit_idx >= 1 && ps2.bit_idx <= 8)
    {
      if (host_dat) ps2.current_rx_byte |= (1 << (ps2.bit_idx - 1));
    }

    ps2.bit_idx++;

    if (ps2.bit_idx == 11)
    {
      // 11ビット目: ACK出力 (デバイスがDataをLowに引く)
      ps2.dat = false;
    }
    else if (ps2.bit_idx == 12)
    {
      PS2_LOG("RX completed. Received byte: %02X (expecting_led: %d)\n", ps2.current_rx_byte, ps2.expecting_led_arg);
      // 受信完了処理
      ps2.dat = true;
      ps2.clk = true;
      ps2.phase = Phase::IDLE;
      ps2.tx_delay_cnt = 400;

      // コマンドに対する応答をキューに積む
      if (ps2.expecting_led_arg)
      {
        ps2.expecting_led_arg = false;
        queue_byte(ps2, 0xFA); // ACK
      }
      else
      {
        switch (ps2.current_rx_byte)
        {
          case 0xFF: // Reset
            queue_byte(ps2, 0xFA); // ACK
            queue_byte(ps2, 0xAA); // BAT Completion
            break;
          case 0xED: // Set LED
            queue_byte(ps2, 0xFA); // ACK
            ps2.expecting_led_arg = true;
            break;
          case 0xF4: // Enable
          case 0xF5: // Disable
            queue_byte(ps2, 0xFA); // ACK
            break;
          default:
            queue_byte(ps2, 0xFA); // 未知のコマンドにも一応ACKを返す
            break;
        }
      }
      return;
    }

    ps2.clk = false;
    ps2.phase = Phase::RX_CLK_LOW;
  }
}

}
