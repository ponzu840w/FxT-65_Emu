/* src/Psg.hpp - YMZ294 PSGエミュレーション */
#pragma once
#include <cstdint>

#include "lib/emu2149.h"

namespace Psg
{
  static constexpr uint32_t CLOCK_HZ = 4000000; // YMZ294マスタークロック 4MHz

  struct State
  {
    PSG*    psg      = nullptr;
    uint8_t addr_reg = 0; // 選択中のレジスタ番号
  };

  void    Init(State& psg, uint32_t sample_rate);
  void    Shutdown(State& psg);
  void    WriteAddr(State& psg, uint8_t val);
  void    WriteData(State& psg, uint8_t val);
  uint8_t ReadData(const State& psg);
  int16_t Calc(State& psg);
}
