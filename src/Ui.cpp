/* src/Ui.cpp - メニューバー / ステータスバー UI 実装 */

// sokol_imgui.h は sokol_gfx.h → sokol_app.h → imgui.h の順でインクルード必須
#include "lib/sokol/sokol_gfx.h"
#include "lib/sokol/sokol_app.h"
#include "lib/sokol/sokol_log.h"
#include "imgui.h"              // lib/imgui/ (Makefile -I で追加)
#include "lib/sokol/sokol_imgui.h"

#include "Ui.hpp"
#include "Sd.hpp"
#include "lib/vrEmu6502.h"

extern "C" const char* platform_get_ui_font_path(void);

namespace Fxt
{
namespace Ui
{

static float   s_dpi              = 1.0f;
static ImFont* s_mono_font        = nullptr; // ステータスバー用等幅フォント (ProggyClean)
static bool    s_has_japanese_font = false;  // 日本語フォント読み込み成否
static bool    s_use_japanese      = false;  // 実効言語（フォントあり && ユーザー設定）

// 実効言語に応じて ja / en を返す
static const char* L(const char* ja, const char* en)
{
  return s_use_japanese ? ja : en;
}

// ------------------------------------------------------------------
//  Init  sokol_gfx 初期化後に一度だけ呼ぶ
// ------------------------------------------------------------------
void Init(int w, int h, float dpi)
{
  s_dpi = dpi;

  simgui_desc_t desc = {};
  desc.no_default_font = true;  // カスタムフォントで置き換えるので省略
  desc.logger.func     = slog_func;
  simgui_setup(&desc);

  ImGuiIO& io = ImGui::GetIO();

  // システムフォント (Hiragino Sans 等): メニュー等の日本語テキスト用
  {
    const char* font_path = platform_get_ui_font_path();
    if (font_path && font_path[0] != '\0')
    {
      ImFontConfig cfg = {};
      cfg.OversampleH = 1;
      cfg.OversampleV = 1;
      ImFont* f = io.Fonts->AddFontFromFileTTF(
        font_path,
        14.0f * dpi,
        &cfg,
        io.Fonts->GetGlyphRangesJapanese());
      s_has_japanese_font = (f != nullptr);
      // ASCII はサブセット/システムフォントに含まれるため追加マージ不要
    }
    // フォントが見つからない場合は後続の AddFontDefault() がデフォルトフォントになる
  }

  // ProggyClean: ステータスバーのレジスタ値表示用（等幅 ASCII）
  s_mono_font = io.Fonts->AddFontDefault();

  (void)w; (void)h;
}

// ------------------------------------------------------------------
//  NewFrame  毎フレーム描画前に呼ぶ（simgui_new_frame ラッパ）
// ------------------------------------------------------------------
void NewFrame(int w, int h, double delta_time, float dpi)
{
  s_dpi = dpi;
  simgui_frame_desc_t fd = {};
  fd.width      = w;
  fd.height     = h;
  fd.delta_time = delta_time;
  fd.dpi_scale  = dpi;
  simgui_new_frame(&fd);
}

// ------------------------------------------------------------------
//  Render  ImGui ウィジェット構築 + simgui_render
// ------------------------------------------------------------------
void Render(State& ui, const System& sys, float win_w, float win_h)
{
  // 実効言語: フォントが利用可能な場合のみ日本語を使う
  s_use_japanese = s_has_japanese_font && ui.lang_japanese;

  // ---- メニューバー ----
  if (ImGui::BeginMainMenuBar())
  {
    ui.menu_h = ImGui::GetWindowHeight();

    if (ImGui::BeginMenu(L("システム", "System")))
    {
      if (ImGui::MenuItem(L("リセット",      "Reset")))      ui.request_reset      = true;
      if (ImGui::MenuItem(L("ハードリセット", "Hard Reset"))) ui.request_hard_reset = true;
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(L("SDカード", "SD Card")))
    {
      if (ImGui::MenuItem(L("VHDファイルを選択...", "Load VHD..."))) ui.request_vhd_load = true;
#ifdef __EMSCRIPTEN__
      if (ImGui::MenuItem(L("VHDファイルをダウンロード", "Download VHD"))) ui.request_vhd_dl = true;
#endif
      ImGui::EndMenu();
    }

    // ---- 言語メニュー ----
    if (ImGui::BeginMenu(L("言語", "Language")))
    {
      // 日本語オプション: フォントがない場合はグレーアウト
      if (!s_has_japanese_font) ImGui::BeginDisabled();
      if (ImGui::MenuItem("日本語", nullptr, s_has_japanese_font && ui.lang_japanese))
        ui.lang_japanese = true;
      if (!s_has_japanese_font) ImGui::EndDisabled();

      if (ImGui::MenuItem("English", nullptr, !s_use_japanese))
        ui.lang_japanese = false;

      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }

  // ---- ステータスバー（下部固定ウィンドウ） ----
  {
    float sh = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
    ui.status_h = sh;

    ImGui::SetNextWindowPos(ImVec2(0, win_h - sh));
    ImGui::SetNextWindowSize(ImVec2(win_w, sh));

    static constexpr ImGuiWindowFlags kStatusFlags =
      ImGuiWindowFlags_NoDecoration   |
      ImGuiWindowFlags_NoMove         |
      ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoScrollbar    |
      ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin("##status", nullptr, kStatusFlags))
    {
      ImGui::PushFont(s_mono_font); // 等幅フォントに切り替え

      // ---- CPU レジスタ ----
      uint16_t pc   = vrEmu6502GetPC(sys.cpu);
      uint8_t  sp   = vrEmu6502GetStackPointer(sys.cpu);
      uint8_t  sr   = vrEmu6502GetStatus(sys.cpu);
      uint8_t  acc  = vrEmu6502GetAcc(sys.cpu);
      uint8_t  x    = vrEmu6502GetX(sys.cpu);
      uint8_t  y    = vrEmu6502GetY(sys.cpu);

      ImGui::Text("PC:%04X SP:%02X P:%02X A:%02X X:%02X Y:%02X",
                  pc, sp, sr, acc, x, y);

      ImGui::SameLine(0, 20);

      // ---- SD カード状態 ----
      if (sys.sd.image_fp != nullptr)
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "SD:OK");
      else
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "SD:--");

      ImGui::SameLine(0, 20);

      // ---- MHz / FPS（0.5秒ごとに更新） ----
      static double s_accum     = 0.0;
      static float  s_disp_mhz = 0.0f;
      static float  s_disp_fps = 0.0f;
      static int    s_frame_cnt = 0;
      static int    s_tick_acc  = 0;

      double dt = sapp_frame_duration();
      s_accum    += dt;
      s_frame_cnt++;
      s_tick_acc += sys.cfg.ticks_per_frame();

      if (s_accum >= 0.5)
      {
        s_disp_fps = (float)(s_frame_cnt / s_accum);
        s_disp_mhz = (float)(s_tick_acc  / s_accum) * 1e-6f;
        s_accum    = 0.0;
        s_frame_cnt = 0;
        s_tick_acc  = 0;
      }

      ImGui::Text("%.2f MHz  %.1f FPS", s_disp_mhz, s_disp_fps);

      ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

  // ---- GPU レンダリング ----
  simgui_render();
}

// ------------------------------------------------------------------
//  Shutdown
// ------------------------------------------------------------------
void Shutdown()
{
  simgui_shutdown();
}

} // namespace Ui
} // namespace Fxt
