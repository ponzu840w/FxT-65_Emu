/* src/main.cpp - FxT-65 エミュレータ Sokol アプリ本体 */

#ifndef SOKOL_METAL
#define SOKOL_METAL
#endif
#include "lib/sokol/sokol_app.h"  // アプリラッパ
#include "lib/sokol/sokol_gfx.h"  // グラフィック
#include "lib/sokol/sokol_glue.h" // appとgfxのグルー
#include "lib/sokol/sokol_log.h"  // ロギング

#include "FxtSystem.hpp"
#include "Chdz.hpp"

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <cstdio>

// ---------------------------------------------------------------
//  定数
// ---------------------------------------------------------------
static constexpr int DISPLAY_W    = 256;    // 論理サイズ
static constexpr int DISPLAY_H    = 768;
static constexpr int WINDOW_W     = 1024;   // 表示サイズ
static constexpr int WINDOW_H     = 768;

// 1フレームあたりのCPUサイクル数
static constexpr int TICKS_PER_FRAME = 200000;

// ---------------------------------------------------------------
//  グローバル変数
// ---------------------------------------------------------------
// FxT-65システム
static Fxt::System  g_sys;
// sokol
static sg_image     g_image;    // GPUテクスチャ
static sg_pipeline  g_pip;      // レンダリングパイプライン
static sg_view      g_view;     // テクスチャビュー
static sg_sampler   g_sampler;  // テクスチャサンプリング方法
static sg_bindings  g_bind;     // シェーダ用リソースバインド情報
static sg_pass_action g_pass_action;  // レンダーパス開始時の動作
// 描画用バッファ CRTCエミュレータに書き込まれ、sokolが参照
static uint8_t      g_pixels[DISPLAY_W * DISPLAY_H * 4];

// 端末ノンブロッキング入力管理
struct TerminalSession
{
  struct termios oldt;
  int oldf;
  bool active = false;

  TerminalSession()
  {
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) return;
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
static TerminalSession* g_term = nullptr;

// ---------------------------------------------------------------
//  シェーダーソース (Metal)
//  フルスクリーンクワッドを描くシンプルなシェーダー
// ---------------------------------------------------------------
static const char* vs_src =
  "#include <metal_stdlib>\n"
  "using namespace metal;\n"
  "struct vs_out { float4 pos [[position]]; float2 uv; };\n"
  "vertex vs_out _main(uint vid [[vertex_id]]) {\n"
  "  vs_out o;\n"
  "  float2 pos = float2((vid & 1) ? 1.0 : -1.0, (vid & 2) ? -1.0 : 1.0);\n"
  "  o.pos = float4(pos, 0.0, 1.0);\n"
  "  o.uv  = float2((vid & 1) ? 1.0 : 0.0, (vid & 2) ? 1.0 : 0.0);\n"
  "  return o;\n"
  "}\n";

static const char* fs_src =
  "#include <metal_stdlib>\n"
  "using namespace metal;\n"
  "struct vs_out { float4 pos [[position]]; float2 uv; };\n"
  "fragment float4 _main(vs_out in [[stage_in]],\n"
  "    texture2d<float> tex [[texture(0)]],\n"
  "    sampler smp [[sampler(0)]]) {\n"
  "  return tex.sample(smp, in.uv);\n"
  "}\n";

// ---------------------------------------------------------------
//  init_cb 最初に一回呼ばれる
// ---------------------------------------------------------------
static void init_cb(void)
{
  // sokol_gfx 初期化
  sg_desc gfx_desc = {};
  gfx_desc.environment = sglue_environment();
  gfx_desc.logger.func = slog_func;
  sg_setup(&gfx_desc);

  // ROM ロード
  if (!Fxt::LoadRom(g_sys, "assets/rom.bin"))
  {
    fprintf(stderr, "Error: ROM読み込みに失敗\n");
    sapp_quit();
    return;
  }

  // SD カードイメージをマウント
  if (!Fxt::Sd::MountImg(g_sys, "sdcard.img"))
  {
    fprintf(stderr, "Error: SDカードイメージ読み込みに失敗\n");
    sapp_quit();
    return;
  }

  // 端末ノンブロッキング入力設定
  g_term = new TerminalSession();

  // エミュレータ初期化
  Fxt::Init(g_sys);

  // テクスチャ作成 (256×768 RGBA8888, stream_update)
  {
    sg_image_desc img_desc = {}; // 記述構造体
    img_desc.width  = DISPLAY_W;
    img_desc.height = DISPLAY_H;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.usage.stream_update = true;
    img_desc.label = "chdz-framebuffer";
    g_image = sg_make_image(&img_desc);
  }

  // テクスチャビュー作成
  {
    sg_view_desc vd = {};
    vd.texture.image = g_image;
    vd.label = "chdz-view";
    g_view = sg_make_view(&vd);
  }

  // サンプラー作成 (Nearest)
  {
    sg_sampler_desc smp_desc = {};
    smp_desc.min_filter = SG_FILTER_NEAREST;
    smp_desc.mag_filter = SG_FILTER_NEAREST;
    smp_desc.label = "chdz-sampler";
    g_sampler = sg_make_sampler(&smp_desc);
  }

  // シェーダー作成
  sg_shader shd;
  {
    sg_shader_desc shd_desc = {};
    shd_desc.vertex_func.source   = vs_src;
    shd_desc.vertex_func.entry    = "_main";
    shd_desc.fragment_func.source = fs_src;
    shd_desc.fragment_func.entry  = "_main";
    // フラグメントシェーダーのテクスチャビュー宣言 (slot 0)
    shd_desc.views[0].texture.stage      = SG_SHADERSTAGE_FRAGMENT;
    shd_desc.views[0].texture.image_type = SG_IMAGETYPE_2D;
    shd_desc.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
    // フラグメントシェーダーのサンプラー宣言 (slot 0)
    shd_desc.samplers[0].stage        = SG_SHADERSTAGE_FRAGMENT;
    shd_desc.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
    // テクスチャ-サンプラーペア
    shd_desc.texture_sampler_pairs[0].stage        = SG_SHADERSTAGE_FRAGMENT;
    shd_desc.texture_sampler_pairs[0].view_slot    = 0;
    shd_desc.texture_sampler_pairs[0].sampler_slot = 0;
    shd_desc.label = "chdz-shader";
    shd = sg_make_shader(&shd_desc);
  }

  // パイプライン作成 (頂点バッファなし、シェーダー内で位置生成)
  {
    sg_pipeline_desc pip_desc = {};
    pip_desc.shader = shd;
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
    pip_desc.label = "chdz-pipeline";
    g_pip = sg_make_pipeline(&pip_desc);
  }

  // バインディング設定
  g_bind.views[0]    = g_view;
  g_bind.samplers[0] = g_sampler;

  // パスアクション (黒クリア)
  g_pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
  g_pass_action.colors[0].clear_value = {0.0f, 0.0f, 0.0f, 1.0f};
}

// ---------------------------------------------------------------
//  frame_cb  フレームごとに呼ばれる
// ---------------------------------------------------------------
static int g_input_cnt = 0;

static void frame_cb(void)
{
  // 入力処理 (4096サイクルに1回)
  g_input_cnt += TICKS_PER_FRAME;
  if (g_input_cnt >= 4096)
  {
    g_input_cnt -= 4096;
    int ch = getchar();
    if (ch != EOF)
    {
      g_sys.uart_input_buffer = (uint8_t)ch;
      g_sys.uart_status |= 0b00001000;
      Fxt::UpdateIrq(g_sys);

      if (ch == 'N' - 0x40) // Ctrl+N: NMI
      {
        Fxt::RequestNmi(g_sys);
        for (int i = 0; i < 10; i++) Fxt::Tick(g_sys);
        Fxt::ClearNmi(g_sys);
      }
    }
  }

  // エミュレーション実行
  for (int i = 0; i < TICKS_PER_FRAME; i++)
  {
    Fxt::Tick(g_sys);
  }

  // フレームバッファレンダリング
  Chdz::RenderFrame(g_sys.chdz, g_pixels);

  // テクスチャ更新
  {
    sg_image_data img_data = {};
    img_data.mip_levels[0].ptr  = g_pixels;
    img_data.mip_levels[0].size = sizeof(g_pixels);
    sg_update_image(g_image, &img_data);
  }

  // フルスクリーンクワッド描画
  sg_pass pass = {};
  pass.action = g_pass_action;
  pass.swapchain = sglue_swapchain();
  sg_begin_pass(&pass);
  sg_apply_pipeline(g_pip);
  sg_apply_bindings(&g_bind);
  sg_draw(0, 4, 1);
  sg_end_pass();
  sg_commit();
}

// ---------------------------------------------------------------
//  event_cb  イベントハンドラ
// ---------------------------------------------------------------
static void event_cb(const sapp_event* ev)
{
  // キー押下処理
  if (ev->type == SAPP_EVENTTYPE_KEY_DOWN)
  {
    // 印字可能文字の変換
    int ch = -1;
    if (ev->key_code == SAPP_KEYCODE_ENTER)          ch = 0x0A;
    else if (ev->key_code == SAPP_KEYCODE_BACKSPACE) ch = 0x08;
    else if (ev->key_code == SAPP_KEYCODE_ESCAPE)    ch = 0x1B;
    else if (ev->key_code == SAPP_KEYCODE_SPACE)     ch = ' ';
    else if (ev->key_code >= SAPP_KEYCODE_SPACE && ev->key_code <= SAPP_KEYCODE_Z)
    {
      ch = (int)ev->key_code;
      if (!(ev->modifiers & SAPP_MODIFIER_SHIFT))
        ch = ch + ('a' - 'A');
    }

    if (ch >= 0)
    {
      if (ev->modifiers & SAPP_MODIFIER_CTRL)
        ch &= 0x1F;

      g_sys.uart_input_buffer = (uint8_t)ch;
      g_sys.uart_status |= 0b00001000;
      Fxt::UpdateIrq(g_sys);

      if (ch == 'N' - 0x40) // Ctrl+N: NMI
      {
        Fxt::RequestNmi(g_sys);
        for (int i = 0; i < 10; i++) Fxt::Tick(g_sys);
        Fxt::ClearNmi(g_sys);
      }
    }
  }
}

// ---------------------------------------------------------------
//  cleanup_cb 最後に一回呼ばれる
// ---------------------------------------------------------------
static void cleanup_cb(void)
{
  sg_shutdown();
  Fxt::Sd::UnmountImg(g_sys);
  delete g_term;
  g_term = nullptr;
}

// ---------------------------------------------------------------
//  sokol_main  エントリポイント
// ---------------------------------------------------------------
sapp_desc sokol_main(int argc, char* argv[])
{
  (void)argc; (void)argv;
  sapp_desc desc = {};
  desc.init_cb    = init_cb;
  desc.frame_cb   = frame_cb;
  desc.event_cb   = event_cb;
  desc.cleanup_cb = cleanup_cb;
  desc.width      = WINDOW_W;
  desc.height     = WINDOW_H;
  desc.window_title = "FxT-65 Emulator";
  desc.logger.func  = slog_func;
  return desc;  // sokol appの記述子を返す
}
