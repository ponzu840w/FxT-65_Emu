/* src/sokol_impl_linux.cpp - Sokol ライブラリ実装定義 (Linux / X11 + GL backend) */

#define SOKOL_IMPL
#ifndef SOKOL_GLCORE
#define SOKOL_GLCORE
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
//  Linux プラットフォーム関数
// ---------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include "FxtSystem.hpp"
#include "Sd.hpp"

extern Fxt::System g_sys; // main.cpp で定義

static bool file_exists(const char* path)
{
  struct stat st;
  return path && path[0] && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

// fc-match で日本語フォントのファイルパスを取得する
static bool try_fc_match(const char* pattern, char* out, size_t out_size)
{
  std::string cmd = "fc-match -f '%{file}' ";
  cmd += "'";
  cmd += pattern;
  cmd += "' 2>/dev/null";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp) return false;
  size_t n = fread(out, 1, out_size - 1, fp);
  int status = pclose(fp);
  if (status != 0) { out[0] = '\0'; return false; }
  out[n] = '\0';
  // 末尾の改行を除去
  while (n > 0 && (out[n-1] == '\n' || out[n-1] == '\r' || out[n-1] == ' '))
    out[--n] = '\0';
  return file_exists(out);
}

// システムフォントのファイルパスを返す (見つからなければ "")
extern "C" const char* platform_get_ui_font_path(void)
{
  static char s_buf[2048] = "";
  if (s_buf[0]) return s_buf; // キャッシュ済み

  // 1) プロジェクト同梱の日本語フォントを優先
  const char* bundled[] = {
    "assets/ui_font.ttf",
    "assets/ipaexg.ttf",
    nullptr
  };
  for (int i = 0; bundled[i]; ++i)
  {
    if (file_exists(bundled[i]))
    {
      std::strncpy(s_buf, bundled[i], sizeof(s_buf) - 1);
      return s_buf;
    }
  }

  // 2) fontconfig から日本語対応フォントを取得
  const char* patterns[] = {
    "Noto Sans CJK JP",
    "Noto Sans CJK JP:style=Regular",
    "IPAexGothic",
    "Takao Gothic",
    "sans-serif:lang=ja",
    nullptr
  };
  for (int i = 0; patterns[i]; ++i)
  {
    if (try_fc_match(patterns[i], s_buf, sizeof(s_buf)))
      return s_buf;
  }

  s_buf[0] = '\0';
  return s_buf;
}

// ---------------------------------------------------------------
//  Linux VHD ファイルピッカー
//  zenity で .vhd / .img を選択し、SD カードをマウントする
// ---------------------------------------------------------------
void platform_open_vhd(void)
{
  const char* cmd =
    "zenity --file-selection "
    "--title='SDカードイメージを開く' "
    "--file-filter='SD card image | *.vhd *.img' "
    "--file-filter='All | *' 2>/dev/null";
  FILE* fp = popen(cmd, "r");
  if (!fp) return;

  char buf[4096] = {0};
  size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
  int status = pclose(fp);
  if (status != 0) return; // キャンセル等
  if (n == 0) return;
  buf[n] = '\0';
  // 末尾の改行/空白を除去
  while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' '))
    buf[--n] = '\0';
  if (buf[0] == '\0') return;

  std::string path = buf;
  Fxt::Sd::UnmountImg(g_sys);
  if (!Fxt::Sd::MountImg(g_sys, path))
  {
    // エラーはダイアログで通知 (zenity が無い場合は stderr)
    std::string err = "zenity --error --title='SDカード' "
                      "--text='SDカードイメージを開けませんでした:\n";
    err += path;
    err += "' 2>/dev/null";
    if (system(err.c_str()) != 0)
      std::fprintf(stderr, "Failed to mount SD image: %s\n", path.c_str());
  }
}
