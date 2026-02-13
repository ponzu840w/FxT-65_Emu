/* FxtSystem.hpp */
#pragma once

#include <vector>  // メモリ配列用
#include <cstdint> // 固定長整数
#include <string>  // 文字列

extern "C"
{
  #include "lib/vrEmu6502.h"
}

struct FxtSystem
{
  // インスタンスのポインタ
  static FxtSystem* s_instance;

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

  // コンストラクタ デストラクタ
  FxtSystem();
  ~FxtSystem();
};

namespace Fxt
{
  // ROMロード
  bool LoadRom(FxtSystem& sys, const std::string& filename);
  // システム初期化
  void Init(FxtSystem& sys);
  // バス読み書き
  uint8_t BusRead(FxtSystem& sys, uint16_t addr);
  void BusWrite(FxtSystem& sys, uint16_t addr, uint8_t val);
  // 1サイクル実行
  void Tick(FxtSystem& sys);
  // 割り込み操作
  void RequestIrq(FxtSystem& sys);
  void ClearIrq(FxtSystem& sys);
  void RequestNmi(FxtSystem& sys);
  void ClearNmi(FxtSystem& sys);
}

