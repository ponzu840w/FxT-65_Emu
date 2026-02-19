/* src/Via.cpp */
#include "Via.hpp"
#include "FxtSystem.hpp"
#include "Sd.hpp"

namespace Fxt
{
namespace Via
{
  void Write(System& sys, uint16_t addr, uint8_t val)
  {
    uint8_t reg = addr & 0x0F;
    switch (reg)
    {
      case Reg::ORB:
        sys.via.reg_orb = val;
        // SDカードのCS制御へ委譲
        Sd::SetCs(sys, (val & 0b01000000) == 0);
        break;
      case Reg::DDRB: sys.via.reg_ddrb = val; break;
      case Reg::ACR:  sys.via.reg_acr = val;  break;
      case Reg::PCR:  sys.via.reg_pcr = val;  break;
      case Reg::IER:
        if (val & 0b10000000) sys.via.reg_ier |=  (val & 0x7F);
        else                  sys.via.reg_ier &= ~(val & 0x7F);
        UpdateIrq(sys);
        break;
      case Reg::IFR:
        sys.via.reg_ifr &= ~(val & 0x7F);
        UpdateIrq(sys);
        break;
      case Reg::SR:
        sys.via.reg_ifr &= ~0x04;
        UpdateIrq(sys);
        sys.via.reg_sr = val;
        // SDカード転送へ委譲
        {
          uint8_t rx = Sd::Transfer(sys, val);
          sys.via.reg_sr = rx;
          sys.via.reg_ifr |= 0x04;
          UpdateIrq(sys);
        }
        break;
    }
  }

  uint8_t Read(System& sys, uint16_t addr)
  {
    uint8_t reg = addr & 0x0F;
    switch (reg)
    {
      case Reg::ORB: return sys.via.reg_orb;
      case Reg::DDRB:return sys.via.reg_ddrb;
      case Reg::ACR: return sys.via.reg_acr;
      case Reg::PCR: return sys.via.reg_pcr;
      case Reg::IER: return sys.via.reg_ier | 0x80;
      case Reg::IFR:
        {
           uint8_t val = sys.via.reg_ifr & 0x7F;
           if ((sys.via.reg_acr & 0x1C) == 0x08) val |= 0x04;
           if (sys.via.reg_ifr & sys.via.reg_ier & 0x7F) val |= 0x80;
           return val;
        }
      case Reg::SR:
        if ((sys.via.reg_acr & 0x1C) == 0x08)
        {
          // ダミー0xFFを送ってデータを受信
          uint8_t rx = Sd::Transfer(sys, 0xFF);
          sys.via.reg_sr = rx;
        }
        sys.via.reg_ifr &= ~0x04;
        UpdateIrq(sys);
        return sys.via.reg_sr;
    }
    return 0;
  }

}
}
