#include "FxtSystem.hpp"
#include <unistd.h>   // 標準入出力 STDIN_FILENO
#include <termios.h>  // ターミナル設定
#include <fcntl.h>    // ファイル記述子制御 ノンブロッキング
#include <signal.h>   // signal
#include <cstdio>

// 前方宣言
void FormatFlags(uint8_t p, char* buf);
void PrintCpusys(Fxt::System& sys);

// 実行継続フラグ
volatile sig_atomic_t g_running = 1;
// Ctrl+C ハンドラ
void handle_sigint(int sig) { g_running = 0; }

// 生成で端末をノンブロッキング入力可能にして、
// スコープを外れると元に戻す
struct TerminalSession
{
  struct termios oldt;
  int oldf;
  bool active = false;

  TerminalSession()
  {
    tcgetattr(STDIN_FILENO, &oldt);
    struct termios newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    active = true;
  }

  ~TerminalSession()
  {
    if (active)
    {
      tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
      fcntl(STDIN_FILENO, F_SETFL, oldf);
    }
  }
};

int main()
{
  // システムを生成
  Fxt::System sys;

  // ROMロード
  if (!Fxt::LoadRom(sys, "assets/rom.bin"))
  {
    fprintf(stderr, "Error: ROM読み込みに失敗\n");
    return 1;
  }

  // SDカードイメージをマウント
  if (!Fxt::Sd::MountImg(sys, "sdcard.img")) {
    fprintf(stderr, "Error: SDカードイメージ読み込みに失敗\n");
    return 1;
  }

  // 端末ノンブロッキング入力設定
  TerminalSession term;
  signal(SIGINT, handle_sigint);

  // 初期化
  Fxt::Init(sys);

  // 実行ループ
  int cnt = 0;
  while(g_running)
  {
    // 4096回に1回の標準入力処理
    if ((cnt++ & 0xFFF) == 0xFFF)
    {
      // 標準入力があればUARTから送信する
      int ch = getchar();
      if (ch != EOF)
      {
        sys.uart_input_buffer = (uint8_t)ch;
        sys.uart_status |= 0b00001000;
        Fxt::UpdateIrq(sys);

        if (ch == 'N'-0x40) // CTRL+N
        {
          // NMI割り込みでsdmon起動
          Fxt::RequestNmi(sys);
          for(int i=0; i<10; i++) Fxt::Tick(sys);
          Fxt::ClearNmi(sys);
        }
      }
    }

    //PrintCpusys(sys);
    Fxt::Tick(sys);
  }
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
void PrintCpusys(Fxt::System& sys)
{
  VrEmu6502* cpu = sys.cpu;
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
    uint8_t lo = sys.ram[i * 2];
    uint8_t hi = sys.ram[i * 2 + 1];
    zr[i] = (hi << 8) | lo;
  }

  // 一括表示
  // PC | 逆アセンブル | レジスタ | フラグ | ZR
  printf("$%04X: %-14s | A:%02X X:%02X Y:%02X S:%02X P:%s | ZR0:%04X ZR1:%04X ZR2:%04X ZR3:%04X ZR4:%04X ZR5:%04X\n",
         pc, disasm,
         a, x, y, s, flags_str,
         zr[0], zr[1], zr[2], zr[3], zr[4], zr[5]);
  //fflush(stdout);
  //usleep(10000);
}
