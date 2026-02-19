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
    };

    // 操作関数
    void Write(System& sys, uint16_t addr, uint8_t val);
    uint8_t Read(System& sys, uint16_t addr);

  }

}
