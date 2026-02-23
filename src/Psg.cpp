/* src/Psg.cpp - YMZ294 PSGエミュレーション実装 */
#include "Psg.hpp"

namespace Psg
{

void Init(State& psg, uint32_t sample_rate)
{
  psg.psg = PSG_new(CLOCK_HZ, sample_rate);
  PSG_reset(psg.psg);
  psg.addr_reg = 0;
}

void Shutdown(State& psg)
{
  if (psg.psg)
  {
    PSG_delete(psg.psg);
    psg.psg = nullptr;
  }
}

void WriteAddr(State& psg, uint8_t val)
{
  psg.addr_reg = val;
}

void WriteData(State& psg, uint8_t val)
{
  if (psg.psg)
    PSG_writeReg(psg.psg, psg.addr_reg, val);
}

uint8_t ReadData(const State& psg)
{
  if (!psg.psg) return 0;
  return PSG_readReg(psg.psg, psg.addr_reg);
}

int16_t Calc(State& psg)
{
  if (!psg.psg) return 0;
  return PSG_calc(psg.psg);
}

} // namespace Psg
