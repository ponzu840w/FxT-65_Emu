/* src/FxtSystem.hpp */
#pragma once

#include <vector>  // メモリ配列用
#include <cstdint> // 固定長整数
#include <string>  // 文字列

#include "Via.hpp"
#include "Sd.hpp"
#include "Chdz.hpp"
#include "Ps2.hpp"
#include "Psg.hpp"

extern "C"
{
  #include "lib/vrEmu6502.h"
}

namespace Fxt
{

  struct EmulatorConfig
  {
    // エミュレータ速度設定
    // 可変速度
    int   cpu_hz    = 8000000;  // CPU周波数 8MHz
    float sim_speed = 1.0f;     // シミュレーション速度倍率（1.0 = 等倍）

    // 固定速度
    static constexpr int VBLANK_HZ  = 60;    // CRTCが生成するVBLANK周波数
    static constexpr int PS2_CLK_HZ = 12000; // PS/2バスクロック (~12kHz)
    static constexpr int HOST_FPS   = 60;     // Sokolフレームレート

    // VBLANK周期 [CPUサイクル]
    int vblank_period()   const { return cpu_hz / VBLANK_HZ; }
    // PS/2クロック半周期 [CPUサイクル]
    int ps2_half_period() const { return cpu_hz / PS2_CLK_HZ / 2; }
    // 1フレームあたりのCPUサイクル数
    int ticks_per_frame() const
    {
      return (int)((double)cpu_hz * sim_speed / HOST_FPS);
    }
  };

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

    // PSG (YMZ294)
    Psg::State psg;

    // エミュレータ設定
    EmulatorConfig cfg;

    // VBLANK (VIA CA2) 生成用カウンタ
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

