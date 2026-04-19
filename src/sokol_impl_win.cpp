/* src/sokol_impl_win.cpp - Sokol ライブラリ実装定義 (Windows / D3D11 backend) */

#define SOKOL_IMPL
#ifndef SOKOL_D3D11
#define SOKOL_D3D11
#endif
#include "lib/sokol/sokol_log.h"
#include "lib/sokol/sokol_app.h"
#include "lib/sokol/sokol_gfx.h"
#include "lib/sokol/sokol_glue.h"
#include "lib/sokol/sokol_args.h"
#include "lib/sokol/sokol_audio.h"

// sokol_imgui 実装 (imgui.h は sokol_gfx.h / sokol_app.h の後に必須)
#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#include "lib/sokol/sokol_imgui.h"

// ---------------------------------------------------------------
//  Windows プラットフォーム関数
// ---------------------------------------------------------------
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include "FxtSystem.hpp"
#include "Sd.hpp"

extern Fxt::System g_sys; // main.cpp で定義

static bool file_exists_w(const char* path)
{
  if (!path || !path[0]) return false;
  struct _stat64 st;
  return _stat64(path, &st) == 0;
}

// システムフォントのファイルパスを返す (見つからなければ "")
// 優先順: プロジェクト同梱 → Windows システムフォント
extern "C" const char* platform_get_ui_font_path(void)
{
  static char s_buf[MAX_PATH * 2] = "";
  if (s_buf[0]) return s_buf;

  const char* bundled[] = {
    "assets/ui_font.ttf",
    "assets/ipaexg.ttf",
    nullptr
  };
  for (int i = 0; bundled[i]; ++i)
  {
    if (file_exists_w(bundled[i]))
    {
      std::strncpy(s_buf, bundled[i], sizeof(s_buf) - 1);
      return s_buf;
    }
  }

  // Windows の %WINDIR%\Fonts から日本語フォントを探索
  char win_dir[MAX_PATH] = "";
  UINT n = GetWindowsDirectoryA(win_dir, MAX_PATH);
  if (n > 0 && n < MAX_PATH)
  {
    const char* candidates[] = {
      "\\Fonts\\YuGothR.ttc",
      "\\Fonts\\YuGothM.ttc",
      "\\Fonts\\meiryo.ttc",
      "\\Fonts\\msgothic.ttc",
      "\\Fonts\\YuGothic.ttf",
      nullptr
    };
    for (int i = 0; candidates[i]; ++i)
    {
      std::string p = win_dir;
      p += candidates[i];
      if (file_exists_w(p.c_str()))
      {
        std::strncpy(s_buf, p.c_str(), sizeof(s_buf) - 1);
        return s_buf;
      }
    }
  }

  s_buf[0] = '\0';
  return s_buf;
}

// ---------------------------------------------------------------
//  Windows VHD ファイルピッカー
//  GetOpenFileName で .vhd / .img を選択し、SD カードをマウントする
// ---------------------------------------------------------------
void platform_open_vhd(void)
{
  char file_buf[MAX_PATH] = "";
  OPENFILENAMEA ofn;
  std::memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner   = GetActiveWindow();
  ofn.lpstrFilter =
    "SDカードイメージ (*.vhd;*.img)\0*.vhd;*.img\0"
    "すべてのファイル (*.*)\0*.*\0";
  ofn.lpstrFile   = file_buf;
  ofn.nMaxFile    = sizeof(file_buf);
  ofn.lpstrTitle  = "SDカードイメージを開く";
  ofn.Flags       = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

  if (!GetOpenFileNameA(&ofn)) return; // キャンセルまたはエラー
  if (file_buf[0] == '\0')   return;

  std::string path = file_buf;
  Fxt::Sd::UnmountImg(g_sys);
  if (!Fxt::Sd::MountImg(g_sys, path))
  {
    std::string msg = "SDカードイメージを開けませんでした:\n";
    msg += path;
    MessageBoxA(ofn.hwndOwner, msg.c_str(), "SDカード", MB_ICONERROR | MB_OK);
  }
}
