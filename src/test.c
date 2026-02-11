#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "lib/vrEmu6502.h"

uint8_t ram[0x8000]; // 32KB RAM
uint8_t rom[0x1000]; //  4KB ROM

// メモリ読み取り関数
uint8_t ReadMem(uint16_t addr, bool isDbg)
{
  if (addr < 0x8000) return ram[addr];
  if (addr >= 0xF000) return rom[addr & 0x0FFF];
  return 0;
}

// メモリ書き込み関数
void WriteMem(uint16_t addr, uint8_t val)
{
  if (addr < 0x8000) ram[addr] = val;
}

int main() {
  // リセットベクタ $F000
  rom[0x0FFC] = 0x00;
  rom[0x0FFD] = 0xF0;

  // テストプログラム 無限ループ
  rom[0x0000] = 0xEA; // $F000 NOP
  rom[0x0001] = 0x4C; // $F001 JMP $F000
  rom[0x0002] = 0x00;
  rom[0x0003] = 0xF0;

  // CPU作成
  VrEmu6502 *cpu = vrEmu6502New(CPU_W65C02, ReadMem, WriteMem);
  // CPUリセット
  vrEmu6502Reset(cpu);

  printf("CPUエミュレーション開始 PC: %04X\n", vrEmu6502GetPC(cpu));

  // 10サイクル実行
  for (int i = 0; i < 10; i++)
  {
    uint16_t pc = vrEmu6502GetPC(cpu);
    uint8_t op = vrEmu6502GetNextOpcode(cpu);

    printf("サイクル: %d | PC: %04X | オペコード: %02X\n", i, pc, op);

    // 1サイクル進める
    vrEmu6502Tick(cpu);
  }

  return 0;
}

