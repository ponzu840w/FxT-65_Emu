/* src/FxtSystem.hpp */
#pragma once

#include <vector>  // メモリ配列用
#include <cstdint> // 固定長整数
#include <string>  // 文字列

#include "Via.hpp"
#include "Sd.hpp"

extern "C"
{
  #include "lib/vrEmu6502.h"
}

namespace Fxt
{

  struct System
  {
    // インスタンスのポインタ
    static System* s_instance;

    // Cライブラリに渡すためのブリッジ関数
    static uint8_t BridgeRead(uint16_t addr, bool isDbg);
    static void BridgeWrite(uint16_t addr, uint8_t val);

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

    // VIA
    Via::State via;

    // SDカード
    Sd::State sd;

    // コンストラクタ デストラクタ
    System();
    ~System();
  };

  // 操作関数

  // ROMロード
  bool LoadRom(System& sys, const std::string& filename);
  // システム初期化
  void Init(System& sys);
  // 1サイクル実行
  void Tick(System& sys);
  // バス読み書き
  uint8_t BusRead(System& sys, uint16_t addr);
  void BusWrite(System& sys, uint16_t addr, uint8_t val);
  // 割り込み操作
  void UpdateIrq(System& sys);
  void RequestNmi(System& sys);
  void ClearNmi(System& sys);

}

