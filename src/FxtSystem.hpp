/* FxtSystem.hpp */
#pragma once

#include <vector>  // メモリ配列用
#include <cstdint> // 固定長整数
#include <cstdio>  // ファイルIO
#include <string>  // 文字列

extern "C"
{
  #include "lib/vrEmu6502.h"
}

struct FxtSystem
{
  // CPU
  VrEmu6502* cpu = nullptr;
  vrEmu6502Interrupt* irqPin = nullptr;
  vrEmu6502Interrupt* nmiPin = nullptr;

  // メモリ
  std::vector<uint8_t> ram;
  std::vector<uint8_t> rom;

  // UART
  uint8_t uart_input_buffer = 0;
  uint8_t uart_status = 0;

  // コンストラクタ
  FxtSystem()
  {
    // メモリ初期化
    ram.resize(0x8000, 0);
    rom.resize(0x2000, 0);
  }

  ~FxtSystem() {}
};

namespace Fxt
{
  uint8_t BusRead(FxtSystem& sys, uint16_t addr);
  void    BusWrite(FxtSystem& sys, uint16_t addr, uint8_t val);

  // ROMロード
  bool LoadRom(FxtSystem& sys, const std::string& filename)
  {
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    if (size != 8192) return false;

    fseek(fp, 4096, SEEK_SET);
    fread(sys.rom.data(), 1, size, fp);
    fclose(fp);
    return true;
  }

  // システム初期化
  void Init(FxtSystem& sys, vrEmu6502MemRead readFn, vrEmu6502MemWrite writeFn)
  {
    sys.cpu = vrEmu6502New(CPU_W65C02, readFn, writeFn);
    vrEmu6502Reset(sys.cpu);
    sys.irqPin = vrEmu6502Int(sys.cpu);
    sys.nmiPin = vrEmu6502Nmi(sys.cpu);
  }

  // バス読み込み
  uint8_t BusRead(FxtSystem& sys, uint16_t addr)
  {
    // RAM
    if (addr < 0x8000) return sys.ram[addr];
    // IO and ROM
    if (addr >= 0xE000)
    {
      // I/O
      // --- VIA IFR
      if (addr == 0xE20D) return 0xFF;
      // --- UART RX
      if (addr == 0xE000)
      {
        if (sys.irqPin) *sys.irqPin = IntCleared;
        sys.uart_status &= 0b11110111;
        return sys.uart_input_buffer;
      }
      // --- UART STATUS
      if (addr == 0xE001) return sys.uart_status;
      // ROM
      if (addr >= 0xF000) return sys.rom[addr & 0x0FFF];
    }
    return 0;
  }

  // バス書き込み
  void BusWrite(FxtSystem& sys, uint16_t addr, uint8_t val)
  {
    if (addr < 0x8000) sys.ram[addr] = val;
    if (addr == 0xE000) putchar(val);
  }

  // 1サイクル実行
  void Tick(FxtSystem& sys) { if (sys.cpu) vrEmu6502Tick(sys.cpu); }

  // 割り込み操作
  void RequestIrq(FxtSystem& sys) { if (sys.irqPin) *sys.irqPin = IntRequested; }
  void ClearIrq(FxtSystem& sys) { if (sys.irqPin) *sys.irqPin = IntCleared; }
  void RequestNmi(FxtSystem& sys) { if (sys.nmiPin) *sys.nmiPin = IntRequested; }
  void ClearNmi(FxtSystem& sys) { if (sys.nmiPin) *sys.nmiPin = IntCleared; }
}

