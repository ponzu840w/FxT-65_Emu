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
#include "Ps2.hpp"

#include <cstdio>
#include <algorithm> // std::min

// ---------------------------------------------------------------
//  定数
// ---------------------------------------------------------------
static constexpr int   DISPLAY_W    = 256;    // 論理サイズ
static constexpr int   DISPLAY_H    = 768;
static constexpr int   WINDOW_W     = 1024;   // 表示サイズ
static constexpr int   WINDOW_H     = 768;
static constexpr float PADDING_PX   = 20.0f; // ウィンドウ内の余白 [px]

// 1フレームあたりのCPUサイクル数
static constexpr int TICKS_PER_FRAME = 2000000;

// ---------------------------------------------------------------
//  グローバル状態
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

// アスペクト比維持用ユニフォーム
struct Uniforms { float scale_x; float scale_y; };
static Uniforms g_uniforms;

// ---------------------------------------------------------------
//  アスペクト比計算
//  ウィンドウサイズから PADDING を引いた領域に収まるスケールを求め、
//  NDC スケール値を更新する (中央配置)
// ---------------------------------------------------------------
static void update_uniforms(float win_w, float win_h)
{
  float avail_w = win_w - 2.0f * PADDING_PX;
  float avail_h = win_h - 2.0f * PADDING_PX;
  if (avail_w < 1.0f) avail_w = 1.0f;
  if (avail_h < 1.0f) avail_h = 1.0f;

  // スクリーン上の意図サイズ (論理ピクセルは水平4倍に引き伸ばして表示)
  // = WINDOW_W × WINDOW_H で定義された 4:3 比率を維持する
  const float asp_w = (float)WINDOW_W;
  const float asp_h = (float)WINDOW_H;
  float scale = std::min(avail_w / asp_w, avail_h / asp_h);
  g_uniforms.scale_x = (scale * asp_w) / win_w;
  g_uniforms.scale_y = (scale * asp_h) / win_h;
}

// ---------------------------------------------------------------
//  シェーダーソース (Metal)
//  vertex_id から NDC を生成し、scale_x/y でアスペクト比を維持して中央配置
// ---------------------------------------------------------------
static const char* vs_src =
  "#include <metal_stdlib>\n"
  "using namespace metal;\n"
  "struct vs_out { float4 pos [[position]]; float2 uv; };\n"
  "struct Uniforms { float scale_x; float scale_y; };\n"
  "vertex vs_out _main(uint vid [[vertex_id]],\n"
  "    constant Uniforms& u [[buffer(0)]]) {\n"
  "  vs_out o;\n"
  "  float2 ndc = float2((vid & 1) ? 1.0 : -1.0,\n"
  "                      (vid & 2) ? -1.0 : 1.0);\n"
  "  o.pos = float4(ndc * float2(u.scale_x, u.scale_y), 0.0, 1.0);\n"
  "  o.uv  = float2((vid & 1) ? 1.0 : 0.0,\n"
  "                 (vid & 2) ? 1.0 : 0.0);\n"
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
//  init_cb
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

  // エミュレータ初期化
  Fxt::Init(g_sys);

  // 初期ウィンドウサイズでアスペクト比を計算
  update_uniforms((float)sapp_width(), (float)sapp_height());

  // テクスチャ作成 (256×768 RGBA8888, stream_update)
  {
    sg_image_desc img_desc = {}; // 記述構造体
    img_desc.width        = DISPLAY_W;
    img_desc.height       = DISPLAY_H;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.usage.stream_update = true;
    img_desc.label        = "chdz-framebuffer";
    g_image = sg_make_image(&img_desc);
  }

  // テクスチャビュー作成
  {
    sg_view_desc vd = {};
    vd.texture.image = g_image;
    vd.label         = "chdz-view";
    g_view = sg_make_view(&vd);
  }

  // サンプラー作成 (Nearest)
  {
    sg_sampler_desc smp_desc = {};
    smp_desc.min_filter = SG_FILTER_NEAREST;
    smp_desc.mag_filter = SG_FILTER_NEAREST;
    smp_desc.label      = "chdz-sampler";
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
    // 頂点シェーダーのユニフォームブロック slot 0 (アスペクト比スケール)
    shd_desc.uniform_blocks[0].stage        = SG_SHADERSTAGE_VERTEX;
    shd_desc.uniform_blocks[0].size         = sizeof(Uniforms);
    shd_desc.uniform_blocks[0].layout       = SG_UNIFORMLAYOUT_NATIVE;
    shd_desc.uniform_blocks[0].msl_buffer_n = 0; // [[buffer(0)]]
    // フラグメントシェーダーのテクスチャビュー宣言 (slot 0)
    shd_desc.views[0].texture.stage       = SG_SHADERSTAGE_FRAGMENT;
    shd_desc.views[0].texture.image_type  = SG_IMAGETYPE_2D;
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
    pip_desc.shader         = shd;
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
    pip_desc.label          = "chdz-pipeline";
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
static void frame_cb(void)
{
  // エミュレーション実行 (PS/2 Tick は FxtSystem::Tick 内で呼ばれる)
  for (int i = 0; i < TICKS_PER_FRAME; i++)
    Fxt::Tick(g_sys);

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
  pass.action    = g_pass_action;
  pass.swapchain = sglue_swapchain();
  sg_begin_pass(&pass);
  sg_apply_pipeline(g_pip);
  sg_apply_bindings(&g_bind);
  sg_range uni_range = SG_RANGE(g_uniforms);
  sg_apply_uniforms(0, &uni_range);
  sg_draw(0, 4, 1);
  sg_end_pass();
  sg_commit();
}

// ---------------------------------------------------------------
//  event_cb  イベントハンドラ
// ---------------------------------------------------------------
static void event_cb(const sapp_event* ev)
{
  switch (ev->type)
  {
    case SAPP_EVENTTYPE_KEY_DOWN:
      Ps2::KeyDown(g_sys.ps2, (int)ev->key_code);
      break;

    case SAPP_EVENTTYPE_KEY_UP:
      Ps2::KeyUp(g_sys.ps2, (int)ev->key_code);
      break;

    case SAPP_EVENTTYPE_RESIZED:
      update_uniforms((float)ev->framebuffer_width,
                      (float)ev->framebuffer_height);
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------
//  cleanup_cb 最後に一回呼ばれる
// ---------------------------------------------------------------
static void cleanup_cb(void)
{
  sg_shutdown();
  Fxt::Sd::UnmountImg(g_sys);
}

// ---------------------------------------------------------------
//  sokol_main  エントリポイント
// ---------------------------------------------------------------
sapp_desc sokol_main(int argc, char* argv[])
{
  (void)argc; (void)argv;
  sapp_desc desc = {};
  desc.init_cb      = init_cb;
  desc.frame_cb     = frame_cb;
  desc.event_cb     = event_cb;
  desc.cleanup_cb   = cleanup_cb;
  desc.width        = WINDOW_W;
  desc.height       = WINDOW_H;
  desc.window_title = "FxT-65 Emulator";
  desc.logger.func  = slog_func;
  return desc;  // sokol appの記述子を返す
}
