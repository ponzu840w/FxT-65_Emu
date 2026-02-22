/* src/Ps2.hpp - PS/2キーボードエミュレータ
 *
 * VIA Port B ピン配置 (FXT65.inc 準拠):
 *   bit 5 (0x20): PS2_CLK
 *   bit 4 (0x10): PS2_DAT
 *
 * PS/2 Set 2 スキャンコード, デバイス→ホスト送信のみ実装。
 * クロック周波数: HALF_PERIOD サイクルごとにCLK反転 (~12kHz at 8MHz CPU)
 */
#pragma once
#include <cstdint>

namespace Fxt { struct System; }

namespace Ps2
{
  // PS/2クロック半周期 [CPUサイクル] (8MHz / 12kHz / 2 ≒ 333)
  static constexpr int HALF_PERIOD = 333;
  // 送信キューサイズ (複数バイトのシーケンス含む)
  static constexpr int QUEUE_SIZE  = 64;

  // VIA Port B上のピンビット
  static constexpr uint8_t CLK_BIT = 0x20; // bit 5
  static constexpr uint8_t DAT_BIT = 0x10; // bit 4
  static constexpr uint8_t PS2_MASK = CLK_BIT | DAT_BIT;

  enum class TxPhase { IDLE, CLK_LOW, CLK_HIGH };

  struct State
  {
    // 送信バイトキュー (PS/2生バイト列: E0/F0プレフィックス込み)
    uint8_t queue[QUEUE_SIZE];
    int     q_head = 0;
    int     q_tail = 0;

    // 送信ステートマシン
    TxPhase phase            = TxPhase::IDLE;
    int     half_period_cnt  = 0;
    uint8_t current_byte     = 0;
    int     bit_idx          = 0; // 0=スタートビット, 1-8=データ, 9=パリティ, 10=ストップ
    int     parity_bit       = 0; // 奇数パリティビット値

    // 現在のライン状態 (VIA Port B の入力として見える)
    bool clk = true;  // アイドル時はHIGH
    bool dat = true;  // アイドル時はHIGH
  };

  // CPUサイクルごとに呼ぶ
  void Tick(Fxt::System& sys);

  // キー押下→PS/2メイクコードをキューに積む
  void KeyDown(State& ps2, int sapp_keycode);
  // キー離し→PS/2ブレイクコード (F0 XX) をキューに積む
  void KeyUp(State& ps2, int sapp_keycode);

  // VIA Port Bリード用: CLK/DATビットを返す (その他のビットは0)
  uint8_t GetPortBBits(const State& ps2);

} // namespace Ps2
