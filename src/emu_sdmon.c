#include <stdio.h>
#include <stdlib.h>   // atexit, exit
#include <stdint.h>   // uint8_t型
#include <unistd.h>   // 標準入出力 STDIN_FILENO
#include <termios.h>  // ターミナル設定
#include <fcntl.h>    // ファイル記述子制御 ノンブロッキング
#include <signal.h>   // signal

#include "lib/vrEmu6502.h"

// 標準入力制御用
struct termios oldt;
int oldf;
int is_term_configured = 0; // 設定済みフラグ

// FxT-65 内部データ
uint8_t ram[0x8000]; // 32KB RAM
uint8_t rom[0x1000]; //  4KB ROM
uint8_t uart_input_buffer;  // UART入力バッファ
uint8_t uart_status;        // UARTステータスレジスタ内容
vrEmu6502Interrupt* irqPin; // 割り込み制御用
vrEmu6502Interrupt* nmiPin; // ノンマスカブル割り込み制御用

// 終了時に呼ばれる復帰関数
void restore_terminal(void)
{
  if (is_term_configured)
  {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    is_term_configured = 0;
    printf("\nTerminal restored.\n");
  }
}

// Ctrl+C ハンドラ
void handle_sigint(int sig) { exit(0); }

// メモリ読み取り関数
uint8_t ReadMem(uint16_t addr, bool isDbg)
{
  if (addr < 0x8000)  return ram[addr];           // RAM
  if (addr >= 0xF000) return rom[addr & 0x0FFF];  // ROM
  if (addr == 0xE20D) return 0xFF;                // VIA IFR
  if (addr == 0xE000)                             // UART RX
  {
    // 割り込みを解除して受信データフラグも折る
    *irqPin = IntCleared;
    uart_status = uart_status & 0b11110111;
    return uart_input_buffer;
  }
  if (addr == 0xE001) return uart_status;         // UART STATUS
  return 0;
}

// メモリ書き込み関数
void WriteMem(uint16_t addr, uint8_t val)
{
  if (addr < 0x8000) ram[addr] = val; // RAM
  if (addr == 0xE000) putchar(val);   // UART TX
}

// ROMロード
int LoadRom(const char *filename)
{
  printf("ROMデータ\"%s\"をロード開始\n", filename);
  FILE *fp = fopen(filename, "rb");
  if (!fp)
  {
    perror("ROMファイルのオープンに失敗しました。");
    return 0;
  }

  // ファイルサイズ取得
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  rewind(fp);

  printf("ファイルサイズ: %ld バイト\n", size);

  if (size != 8192)
  {
    printf("ファイルサイズが正しくありません。ROMは8KB（8192バイト）です。\n");
    return 0;
  }

  // ROM配列に読み込み
  fseek(fp, 4096, SEEK_SET);
  size_t read_size = fread(rom, 1, size, fp);
  fclose(fp);

  printf("ロード完了（%zu バイト）\n", read_size);
  return 1;
}

// フラグを文字列化するヘルパ ("NV-BDIZC")
void FormatFlags(uint8_t p, char* buf)
{
  const char* names = "NV-BDIZC";
  for (int i = 0; i < 8; i++)
  {
    // ビット7(N)からビット0(C)の順にチェック
    if (p & (0x80 >> i)) buf[i] = names[i];
    else                 buf[i] = '.';
  }
  buf[8] = '\0';
}

// デバッグ表示
void PrintCpuState(VrEmu6502 *cpu)
{
  uint16_t pc = vrEmu6502GetPC(cpu);

  // 逆アセンブル
  char disasm[32];
  vrEmu6502DisassembleInstruction(cpu, pc, sizeof(disasm), disasm, NULL, NULL);

  // レジスタ取得
  uint8_t a = vrEmu6502GetAcc(cpu);
  uint8_t x = vrEmu6502GetX(cpu);
  uint8_t y = vrEmu6502GetY(cpu);
  uint8_t s = vrEmu6502GetStackPointer(cpu);
  uint8_t p = vrEmu6502GetStatus(cpu);
  char flags_str[9];
  FormatFlags(p, flags_str);

  // ZR取得
  uint16_t zr[6];
  for (int i = 0; i < 6; i++)
  {
    uint8_t lo = ReadMem(i * 2, true);
    uint8_t hi = ReadMem(i * 2 + 1, true);
    zr[i] = (hi << 8) | lo;
  }

  // 一括表示
  // PC | 逆アセンブル | レジスタ | フラグ | ZR
  printf("$%04X: %-14s | A:%02X X:%02X Y:%02X S:%02X P:%s | ZR0:%04X ZR1:%04X ZR2:%04X ZR3:%04X ZR4:%04X ZR5:%04X\n",
         pc, disasm,
         a, x, y, s, flags_str,
         zr[0], zr[1], zr[2], zr[3], zr[4], zr[5]);
}

int main()
{
  // ノンブロッキング標準入力制御用変数
  struct termios newt;
  int ch;

  // ROMファイルのロード
  if (!LoadRom("assets/rom.bin"))
  {
    printf("エラー: assets/rom.bin のロードに失敗しました。\n");
    return 1;
  }

  // 終了時に端末設定をリストアするよう設定
  atexit(restore_terminal);       // 終了時に設定を戻す関数を登録
  signal(SIGINT, handle_sigint);  // Ctrl+C ハンドラ設定

  // 端末設定を変更
  tcgetattr(STDIN_FILENO, &oldt); // 現在の端末設定を保存
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);         // カノニカルモードとエコーをオフ
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);  // 標準入力 ノンブロッキングモード
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
  is_term_configured = 1;

  // CPU作成
  VrEmu6502 *cpu = vrEmu6502New(CPU_W65C02, ReadMem, WriteMem);
  // CPUリセット
  vrEmu6502Reset(cpu);
  // 割り込みピン取得
  irqPin = vrEmu6502Int(cpu);
  nmiPin = vrEmu6502Nmi(cpu);

  printf("FxT-65起動 PC: %04X\n", vrEmu6502GetPC(cpu));

  // IPL初期化処理をさせる
  for (int i = 0; i < 500000; i++)
  {
    //PrintCpuState(cpu);

    vrEmu6502Tick(cpu); // 1サイクル進める
  }

  // ノンマスカブル割り込みをかけてsd-monitorを起動させる
  *nmiPin = IntRequested;
  for (int i = 0; i < 10; i++) vrEmu6502Tick(cpu);
  *nmiPin = IntCleared;

  while(true)
  {
    //PrintCpuState(cpu);

    // 標準入力があればUARTから送信する
    ch = getchar();
    if (ch != EOF)
    {
      uart_input_buffer = ch;
      uart_status = uart_status | 0b00001000;
      *irqPin = IntRequested;
    }

    vrEmu6502Tick(cpu); // 1サイクル進める
  }

  return 0;
}

