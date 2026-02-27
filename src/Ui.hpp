/* src/Ui.hpp - メニューバー / ステータスバー UI */
#pragma once

#include "FxtSystem.hpp"

namespace Fxt
{
namespace Ui
{

  struct State
  {
    bool request_reset      = false;
    bool request_hard_reset = false;
    bool request_vhd_load   = false;
    bool request_vhd_dl     = false;  // Web 専用
    float menu_h   = 20.0f;  // メニューバー実高さ（次フレームでレイアウトに反映）
    float status_h = 20.0f;  // ステータスバー実高さ
  };

  void Init(int w, int h, float dpi);
  void NewFrame(int w, int h, double delta_time, float dpi);
  void Render(State& ui, const System& sys, float win_w, float win_h);
  void Shutdown();

} // namespace Ui
} // namespace Fxt
