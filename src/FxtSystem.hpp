/* src/FxtSystem.hpp */
#pragma once

#include <vector>  // メモリ配列用
#include <cstdint> // 固定長整数
#include <string>  // 文字列

#include "Via.hpp"
#include "Sd.hpp"
#include "Chdz.hpp"
#include "Ps2.hpp"

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
    uint8_t ram[0x8000];
    uint8_t rom[0x1000];

    // UART
    uint8_t uart_input_buffer = 0;
    uint8_t uart_status = 0;

    // VIA
    Via::State via;

    // SDカード
    Sd::State sd;

    // Chiina-Dazzler CRTC
    Chdz::State chdz;

    // PS/2キーボード
    Ps2::State ps2;

    // VBLANK (VIA CA2) 生成用カウンタ
    // 8MHz / 60Hz ≈ 133333 サイクルごとに CA2 パルスを発生
    static constexpr int VBLANK_PERIOD = 133333;
    int vblank_cnt = 0;

    // コンストラクタ
    System();
    // デストラクタ
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

