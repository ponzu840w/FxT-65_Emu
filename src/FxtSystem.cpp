/* src/FxtSystem.cpp */
#include "FxtSystem.hpp"
#include "Ps2.hpp"
#include <cstdio>

namespace Fxt
{
  // インスタンスのポインタ
  System* System::s_instance = nullptr;

  // Cライブラリに渡すためのブリッジ関数
  uint8_t System::BridgeRead(uint16_t addr, bool isDbg)
  {
    return BusRead(*s_instance, addr);
  }

  void System::BridgeWrite(uint16_t addr, uint8_t val)
  {
    BusWrite(*s_instance, addr, val);
  }

  // コンストラクタ
  System::System() {}

  // デストラクタ
  System::~System() {}

  // 周辺機器の状態に応じて割り込み線を更新
  void UpdateIrq(System& sys)
  {
    if (!sys.irqPin) return;

    // UART・VIAからの割り込み要求
    bool uart_irq = (sys.uart_status & 0b00001000); // RxReady
    bool via_irq  = (sys.via.reg_ifr & sys.via.reg_ier & 0x7F);

    if (uart_irq || via_irq) *sys.irqPin = IntRequested;
    else                     *sys.irqPin = IntCleared;
  }

  // ノンマスカブル割り込み操作
  void RequestNmi(System& sys) { if (sys.nmiPin) *sys.nmiPin = IntRequested; }
  void ClearNmi(System& sys) { if (sys.nmiPin) *sys.nmiPin = IntCleared; }

  // ROMロード
  bool LoadRom(System& sys, const std::string& filename)
  {
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    if (size != 8192) return false;

    fseek(fp, 4096, SEEK_SET);
    fread(sys.rom, 1, size, fp);
    fclose(fp);
    return true;
  }

  // システム初期化
  void Init(System& sys)
  {
    System::s_instance = &sys;
    sys.cpu = vrEmu6502New(CPU_W65C02, System::BridgeRead, System::BridgeWrite);
    vrEmu6502Reset(sys.cpu);
    sys.irqPin = vrEmu6502Int(sys.cpu);
    sys.nmiPin = vrEmu6502Nmi(sys.cpu);
  }

  // バス読み込み
  uint8_t BusRead(System& sys, uint16_t addr)
  {
    // RAM
    if (addr < 0x8000) return sys.ram[addr];
    // IO and ROM
    if (addr >= 0xE000)
    {
      // I/O
      // --- UART RX
      if (addr == 0xE000)
      {
        if (sys.irqPin) *sys.irqPin = IntCleared;
        sys.uart_status &= 0b11110111;
        UpdateIrq(sys);
        return sys.uart_input_buffer;
      }
      // --- UART STATUS
      if (addr == 0xE001) return sys.uart_status;
      // --- VIA
      if (addr >= 0xE200 && addr <= 0xE20F) return Via::Read(sys, addr);
      // --- PSG (YMZ294) データ読み出し
      if (addr == 0xE401) return Psg::ReadData(sys.psg);
      // ROM
      if (addr >= 0xF000) return sys.rom[addr & 0x0FFF];
    }
    return 0;
  }

  // バス書き込み
  void BusWrite(System& sys, uint16_t addr, uint8_t val)
  {
    // RAM
    if (addr < 0x8000) sys.ram[addr] = val;
    // UART
    if (addr == 0xE000)
    {
      putchar(val);
      fflush(stdout);
    }
    // VIA
    if (addr >= 0xE200 && addr <= 0xE20F) Via::Write(sys, addr, val);
    // PSG (YMZ294)
    if (addr == 0xE400) Psg::WriteAddr(sys.psg, val);
    if (addr == 0xE401) Psg::WriteData(sys.psg, val);
    // Chiina-Dazzler CRTC
    if (addr >= 0xE600 && addr <= 0xE607) Chdz::Write(sys.chdz, addr, val);
  }

  // 1サイクル実行
  void Tick(System& sys)
  {
    vrEmu6502Tick(sys.cpu);
    Via::Tick(sys);
    Ps2::Tick(sys);

    // VBLANK (VIA CA2 立ち下がりエッジ) 生成
    if (++sys.vblank_cnt >= sys.cfg.vblank_period())
    {
      sys.vblank_cnt = 0;
      // PCR bits[3:1] = 001 の場合のみ CA2 を立ち下がりエッジ割り込みとして扱う
      // IFR bit0 = CA2 フラグをセット
      sys.via.reg_ifr |= 0x01;
      UpdateIrq(sys);
    }
  }

}
