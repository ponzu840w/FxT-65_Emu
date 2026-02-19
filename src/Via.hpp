/* src/Via.hpp */
#pragma once
#include <cstdint>

namespace Fxt
{
  // 前方宣言
  struct System;

  namespace Via
  {

    // レジスタのアドレスオフセット
    namespace Reg
    {
      constexpr uint8_t ORB = 0x00; // Output Register B
      constexpr uint8_t DDRB= 0x02; // Data Direction Register B
      constexpr uint8_t T1CL = 0x04;
      constexpr uint8_t T1CH = 0x05;
      constexpr uint8_t T1LL = 0x06;
      constexpr uint8_t T1LH = 0x07;
      constexpr uint8_t T2CL = 0x08;
      constexpr uint8_t T2CH = 0x09;
      constexpr uint8_t SR  = 0x0A; // Shift Register
      constexpr uint8_t ACR = 0x0B; // Aux Control Register
      constexpr uint8_t PCR = 0x0C; // Peripheral Control Register
      constexpr uint8_t IFR = 0x0D; // Interrupt Flag Register
      constexpr uint8_t IER = 0x0E; // Interrupt Enable Register
    }

    // 内部状態
    struct State
    {
      // レジスタ
      uint8_t reg_orb = 0; // Output Register B (CS制御用)
      uint8_t reg_sr  = 0; // Shift Register (データ送受信)
      uint8_t reg_acr = 0; // Aux Control Register (モード設定)
      uint8_t reg_ifr = 0; // Interrupt Flag Register
      uint8_t reg_ier = 0; // Interrupt Enable Register
      uint8_t reg_pcr = 0;
      uint8_t reg_ddrb = 0;

      uint16_t t1_cnt = 0;    // T1カウンタ
      uint8_t t1_latch_l = 0; // T1ラッチ下位
      uint8_t t1_latch_h = 0; // T1ラッチ上位
      bool    t1_running = false;   // カウント中か
      bool    t1_fired = false;     // 一度発火したか

      uint16_t t2_cnt = 0;    // T2カウンタ
      uint8_t t2_latch_l = 0; // T2ラッチ下位
      bool    t2_running = false;   // カウント中か
      bool    t2_fired = false;     // 一度発火したか
    };

    // 操作関数
    void Write(System& sys, uint16_t addr, uint8_t val);
    uint8_t Read(System& sys, uint16_t addr);
    void Tick(System& sys);

  }

}
