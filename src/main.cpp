/* src/main.cpp - FxT-65 エミュレータ Sokol アプリ本体 */

#include "lib/sokol/sokol_app.h"   // アプリラッパ
#include "lib/sokol/sokol_gfx.h"   // グラフィック
#include "lib/sokol/sokol_glue.h"  // appとgfxのグルー
#include "lib/sokol/sokol_log.h"   // ロギング
#include "lib/sokol/sokol_args.h"  // コマンドライン引数
#include "lib/sokol/sokol_audio.h" // オーディオ出力
#include "lib/sokol/sokol_imgui.h" // ImGui 統合

#include "FxtSystem.hpp"
#include "Chdz.hpp"
#include "Ps2.hpp"
#include "Psg.hpp"
#include "Ui.hpp"

#include <cstdio>
#include <cstdlib>
#include <algorithm> // std::min
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#endif

// ---------------------------------------------------------------
//  定数
// ---------------------------------------------------------------
static constexpr int   DISPLAY_W        = 256;    // 論理サイズ
static constexpr int   DISPLAY_H        = 768;
static constexpr int   WINDOW_W         = 1024;   // 表示サイズ
static constexpr int   WINDOW_H         = 768;
static constexpr float PADDING_PX       = 20.0f;  // ウィンドウ内の余白 [px]
static constexpr int   AUDIO_SAMPLE_RATE = 44100; // 音声サンプルレート [Hz]
static constexpr int   AUDIO_BUF_SIZE   = 2048;   // 音声バッファサイズ [サンプル]
static constexpr float INT16_FULL_SCALE = 32768.0f; // int16_t→float 正規化係数

// ---------------------------------------------------------------
//  グローバル状態
// ---------------------------------------------------------------
// FxT-65システム (sokol_impl.mm からアクセスするため外部リンケージ)
Fxt::System  g_sys;
// UI 状態
static Fxt::Ui::State g_ui;
// sokol
static sg_image     g_image;    // GPUテクスチャ
static sg_pipeline  g_pip;      // レンダリングパイプライン
static sg_view      g_view;     // テクスチャビュー
static sg_sampler   g_sampler;  // テクスチャサンプリング方法
static sg_bindings  g_bind;     // シェーダ用リソースバインド情報
static sg_pass_action g_pass_action;  // レンダーパス開始時の動作
// 描画用バッファ CRTCエミュレータに書き込まれ、sokolが参照
static uint32_t     g_pixels[DISPLAY_W * DISPLAY_H];

// アスペクト比維持用ユニフォーム
// offset_y: メニュー/ステータスバーを考慮した Y 方向オフセット (NDC)
struct Uniforms { float scale_x; float scale_y; float offset_y; };
static Uniforms g_uniforms;

// ---------------------------------------------------------------
//  platform_open_vhd 宣言 (macOS 実装は sokol_impl.mm)
// ---------------------------------------------------------------
#ifndef __EMSCRIPTEN__
void platform_open_vhd(void);
#endif

// ---------------------------------------------------------------
//  Web プラットフォーム: VHD ロード / ダウンロード (Emscripten)
// ---------------------------------------------------------------
#ifdef __EMSCRIPTEN__

// 現在マウント中のパス（JS コールバック後に更新）
static std::string g_mounted_vhd_path = "sdcard.vhd";

// JavaScript から呼ばれる:
//   ユーザが選択したファイルを MEMFS に書き込んだ後、このパスでマウントする
extern "C" {
EMSCRIPTEN_KEEPALIVE
void web_mount_vhd_from_path(const char* path)
{
  Fxt::Sd::UnmountImg(g_sys);
  if (Fxt::Sd::MountImg(g_sys, std::string(path)))
    g_mounted_vhd_path = path;
}
} // extern "C"

// ファイルピッカーを開く（ユーザジェスチャが有効な rAF 内から呼ぶ）
static void platform_open_vhd_web()
{
  EM_ASM({
    var input = document.createElement('input');
    input.type   = 'file';
    input.accept = '.vhd,.img';
    input.onchange = function(e) {
      var file = e.target.files[0];
      if (!file) return;
      var reader = new FileReader();
      reader.onload = function(ev) {
        var data = new Uint8Array(ev.target.result);
        var path = '/tmp/' + file.name;
        FS.writeFile(path, data);
        Module.ccall('web_mount_vhd_from_path', null, ['string'], [path]);
      };
      reader.readAsArrayBuffer(file);
    };
    input.click();
  });
}

// 現在マウント中の VHD をブラウザにダウンロードさせる
static void platform_download_vhd_web()
{
  // FILE* を MEMFS に同期してから JS 側で読む
  if (g_sys.sd.image_fp) fflush(g_sys.sd.image_fp);

  EM_ASM({
    var path = UTF8ToString($0);
    try {
      var data = FS.readFile(path);
      var blob = new Blob([data.buffer], {type: 'application/octet-stream'});
      var url  = URL.createObjectURL(blob);
      var a    = document.createElement('a');
      a.href     = url;
      a.download = path.split('/').pop() || 'sdcard.vhd';
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      setTimeout(function() { URL.revokeObjectURL(url); }, 100);
    } catch(e) {
      console.error('VHD download failed: ' + e);
    }
  }, g_mounted_vhd_path.c_str());
}

#endif // __EMSCRIPTEN__

// ---------------------------------------------------------------
//  アスペクト比計算
//  ウィンドウサイズからメニュー/ステータスバーと PADDING を引いた領域に
//  収まるスケールを求め、NDC スケール・オフセット値を更新する（中央配置）
// ---------------------------------------------------------------
static void update_uniforms(float win_w, float win_h, float menu_h, float status_h)
{
  float avail_w = win_w - 2.0f * PADDING_PX;
  float avail_h = win_h - menu_h - status_h - 2.0f * PADDING_PX;
  if (avail_w < 1.0f) avail_w = 1.0f;
  if (avail_h < 1.0f) avail_h = 1.0f;

  // スクリーン上の意図サイズ (論理ピクセルは水平4倍に引き伸ばして表示)
  // = WINDOW_W × WINDOW_H で定義された 4:3 比率を維持する
  const float asp_w = (float)WINDOW_W;
  const float asp_h = (float)WINDOW_H;
  float scale = std::min(avail_w / asp_w, avail_h / asp_h);
  g_uniforms.scale_x = (scale * asp_w) / win_w;
  g_uniforms.scale_y = (scale * asp_h) / win_h;
  // NDC Y は上が+1、下が-1 なので、メニューが上でステータスが下にある分だけ
  // 中心を下にシフト: (status_h - menu_h) / win_h
  g_uniforms.offset_y = (status_h - menu_h) / win_h;
}

#ifndef __EMSCRIPTEN__
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
#endif

// ---------------------------------------------------------------
//  シェーダーソース
//  vertex_id から NDC を生成し、scale_x/y でアスペクト比を維持して中央配置
//  offset_y: バーによる Y オフセット (NDC)
// ---------------------------------------------------------------
#ifdef SOKOL_GLES3
static const char* vs_src =
  "#version 300 es\n"
  "uniform float scale_x;\n"
  "uniform float scale_y;\n"
  "uniform float offset_y;\n"
  "out vec2 uv;\n"
  "void main() {\n"
  "  vec2 ndc = vec2(\n"
  "    (gl_VertexID & 1) != 0 ? 1.0 : -1.0,\n"
  "    (gl_VertexID & 2) != 0 ? -1.0 : 1.0);\n"
  "  gl_Position = vec4(ndc.x * scale_x, ndc.y * scale_y + offset_y, 0.0, 1.0);\n"
  "  uv = vec2(\n"
  "    (gl_VertexID & 1) != 0 ? 1.0 : 0.0,\n"
  "    (gl_VertexID & 2) != 0 ? 1.0 : 0.0);\n"
  "}\n";
static const char* fs_src =
  "#version 300 es\n"
  "precision mediump float;\n"
  "uniform sampler2D tex;\n"
  "in vec2 uv;\n"
  "out vec4 frag_color;\n"
  "void main() { frag_color = texture(tex, uv); }\n";
#else
static const char* vs_src =
  "#include <metal_stdlib>\n"
  "using namespace metal;\n"
  "struct vs_out { float4 pos [[position]]; float2 uv; };\n"
  "struct Uniforms { float scale_x; float scale_y; float offset_y; };\n"
  "vertex vs_out _main(uint vid [[vertex_id]],\n"
  "    constant Uniforms& u [[buffer(0)]]) {\n"
  "  vs_out o;\n"
  "  float2 ndc = float2((vid & 1) ? 1.0 : -1.0,\n"
  "                      (vid & 2) ? -1.0 : 1.0);\n"
  "  o.pos = float4(ndc.x * u.scale_x, ndc.y * u.scale_y + u.offset_y, 0.0, 1.0);\n"
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
#endif

// ---------------------------------------------------------------
//  init_cb
// ---------------------------------------------------------------
static void init_cb(void)
{
  // sokol_audio 初期化
  {
    saudio_desc audio_desc = {};
    audio_desc.num_channels = 1;
    audio_desc.sample_rate  = AUDIO_SAMPLE_RATE;
    audio_desc.logger.func  = slog_func;
    saudio_setup(&audio_desc);
    Psg::Init(g_sys.psg, saudio_sample_rate()); // 実際のレートで初期化
  }

  // sokol_gfx 初期化
  sg_desc gfx_desc = {};
  gfx_desc.environment = sglue_environment();
  gfx_desc.logger.func = slog_func;
  sg_setup(&gfx_desc);

  // sokol_imgui 初期化 (sokol_gfx の直後)
  Fxt::Ui::Init(sapp_width(), sapp_height(), sapp_dpi_scale());

  // ROM ロード
  if (!Fxt::LoadRom(g_sys, "assets/rom.bin"))
  {
    fprintf(stderr, "Error: ROM読み込みに失敗\n");
    sapp_quit();
    return;
  }

  // SD カードイメージをマウント (.vhd → .img の順で試行)
  if (!Fxt::Sd::MountImg(g_sys, "sdcard.vhd") &&
      !Fxt::Sd::MountImg(g_sys, "sdcard.img"))
  {
    fprintf(stderr, "Error: SDカードイメージ読み込みに失敗 (sdcard.vhd / sdcard.img)\n");
    sapp_quit();
    return;
  }

#ifndef __EMSCRIPTEN__
  // 端末ノンブロッキング入力設定
  g_term = new TerminalSession();
#endif

  // エミュレータ初期化
  Fxt::Init(g_sys);

  // 初期ウィンドウサイズでアスペクト比を計算
  update_uniforms((float)sapp_width(), (float)sapp_height(),
                  g_ui.menu_h, g_ui.status_h);

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
#ifndef SOKOL_GLES3
    shd_desc.vertex_func.entry    = "_main";
#endif
    shd_desc.fragment_func.source = fs_src;
#ifndef SOKOL_GLES3
    shd_desc.fragment_func.entry  = "_main";
#endif
    // 頂点シェーダーのユニフォームブロック slot 0 (アスペクト比スケール + オフセット)
    shd_desc.uniform_blocks[0].stage        = SG_SHADERSTAGE_VERTEX;
    shd_desc.uniform_blocks[0].size         = sizeof(Uniforms);
    shd_desc.uniform_blocks[0].layout       = SG_UNIFORMLAYOUT_NATIVE;
#ifndef SOKOL_GLES3
    shd_desc.uniform_blocks[0].msl_buffer_n = 0; // [[buffer(0)]]
#else
    shd_desc.uniform_blocks[0].glsl_uniforms[0].glsl_name = "scale_x";
    shd_desc.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT;
    shd_desc.uniform_blocks[0].glsl_uniforms[1].glsl_name = "scale_y";
    shd_desc.uniform_blocks[0].glsl_uniforms[1].type = SG_UNIFORMTYPE_FLOAT;
    shd_desc.uniform_blocks[0].glsl_uniforms[2].glsl_name = "offset_y";
    shd_desc.uniform_blocks[0].glsl_uniforms[2].type = SG_UNIFORMTYPE_FLOAT;
#endif
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
#ifdef SOKOL_GLES3
    shd_desc.texture_sampler_pairs[0].glsl_name = "tex";
#endif
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
#ifndef __EMSCRIPTEN__
static int   g_input_cnt  = 0;
#endif
static int   g_audio_acc  = 0;
static float g_audio_buf[AUDIO_BUF_SIZE];

// UART 入力を処理するヘルパー
static void process_uart_input(int ch)
{
  if (ch == 0x7F) ch = 0x08; // DEL → BS
  g_sys.uart_input_buffer = (uint8_t)ch;
  g_sys.uart_status |= 0b00001000;
  Fxt::UpdateIrq(g_sys);
  if (ch == ('N' - 0x40)) // Ctrl+N: NMI
  {
    Fxt::RequestNmi(g_sys);
    for (int i = 0; i < 10; i++) Fxt::Tick(g_sys);
    Fxt::ClearNmi(g_sys);
  }
}

static void frame_cb(void)
{
  float win_w = sapp_widthf();
  float win_h = sapp_heightf();

  // ImGui 新フレーム開始
  Fxt::Ui::NewFrame((int)win_w, (int)win_h,
                    sapp_frame_duration(), sapp_dpi_scale());

#ifdef __EMSCRIPTEN__
  // Web: JavaScript の uartInputQueue からフレームごとにポーリング
  {
    int ch = EM_ASM_INT({
      return (typeof uartInputQueue !== 'undefined' && uartInputQueue.length > 0)
        ? uartInputQueue.shift() : -1;
    });
    if (ch >= 0) process_uart_input(ch);
  }
#else
  // ネイティブ: 標準入力処理 (4096サイクルに1回)
  g_input_cnt += g_sys.cfg.ticks_per_frame();
  if (g_input_cnt >= 4096)
  {
    g_input_cnt = 0;
    int ch = getchar();
    if (ch != EOF) process_uart_input(ch);
  }
#endif

  // エミュレーション実行
  int tpf = g_sys.cfg.ticks_per_frame();  // 1フレームあたり何ティックか
  int audio_count = 0;                    // バッファのインデックス
  int sr  = saudio_sample_rate();         // 音声サンプリングレート
  for (int i = 0; i < tpf; i++)
  {
    // FxT-65のティック=CPUクロックを進める
    Fxt::Tick(g_sys);

    // 音声サンプリング（CPUクロックよりも低頻度）
    // cpu_hzに対して、sr/cpu_hz の頻度で実行
    g_audio_acc += sr;
    if (g_audio_acc >= g_sys.cfg.cpu_hz)
    {
      // カウンタをリセットするが端数を保存
      g_audio_acc -= g_sys.cfg.cpu_hz;
      // PSG出力信号レベル（16bit int）を正規化して音声出力
      if (audio_count < AUDIO_BUF_SIZE)
      {
        g_audio_buf[audio_count++] = Psg::Calc(g_sys.psg) / INT16_FULL_SCALE;
      }
    }
  }
  saudio_push(g_audio_buf, audio_count);

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

  // UI レンダリング (ImGui ウィジェット構築 + GPU 描画)
  Fxt::Ui::Render(g_ui, g_sys, win_w, win_h);

  sg_end_pass();
  sg_commit();

  // UI リクエスト処理
  if (g_ui.request_reset)
  {
    vrEmu6502Reset(g_sys.cpu);
    g_ui.request_reset = false;
  }
  if (g_ui.request_hard_reset)
  {
    Fxt::Init(g_sys);
    g_ui.request_hard_reset = false;
  }
#ifdef __EMSCRIPTEN__
  if (g_ui.request_vhd_load)
  {
    platform_open_vhd_web();
    g_ui.request_vhd_load = false;
  }
  if (g_ui.request_vhd_dl)
  {
    platform_download_vhd_web();
    g_ui.request_vhd_dl = false;
  }
#else
  if (g_ui.request_vhd_load)
  {
    platform_open_vhd();
    g_ui.request_vhd_load = false;
  }
#endif

  // バー高さに応じたユニフォーム更新
  update_uniforms(win_w, win_h, g_ui.menu_h, g_ui.status_h);
}

// ---------------------------------------------------------------
//  event_cb  イベントハンドラ
// ---------------------------------------------------------------
static void event_cb(const sapp_event* ev)
{
  // ImGui にイベントを渡す（マウス/キーボード入力）
  if (simgui_handle_event(ev)) return;

#ifdef __EMSCRIPTEN__
  // sokol は window の capture phase にキーイベントを登録するため、
  // terminal にフォーカスがあっても event_cb が呼ばれてしまう。
  // JS 側が uartInputQueue に積む処理と二重にならないよう、
  // terminal フォーカス中はキーイベントを無視する。
  if (ev->type == SAPP_EVENTTYPE_KEY_DOWN || ev->type == SAPP_EVENTTYPE_KEY_UP)
  {
    if (EM_ASM_INT({
      return document.activeElement === document.getElementById('terminal') ? 1 : 0;
    })) return;
  }
#else
  static bool keyin_to_uart = false;
#endif

  switch (ev->type)
  {
    // キーボード押下
    case SAPP_EVENTTYPE_KEY_DOWN:
#ifndef __EMSCRIPTEN__
      if (keyin_to_uart)
      {
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
          if (ev->modifiers & SAPP_MODIFIER_CTRL) ch &= 0x1F;
          process_uart_input(ch);
        }
        else
        {
          Ps2::KeyDown(g_sys.ps2, (int)ev->key_code);
        }
      }
      else
#endif
      {
        // PS/2キーボードとして入力
        Ps2::KeyDown(g_sys.ps2, (int)ev->key_code);
      }
      break;

    case SAPP_EVENTTYPE_KEY_UP:
#ifndef __EMSCRIPTEN__
      if (!keyin_to_uart)
#endif
        Ps2::KeyUp(g_sys.ps2, (int)ev->key_code);
      break;

    case SAPP_EVENTTYPE_RESIZED:
      update_uniforms((float)ev->framebuffer_width,
                      (float)ev->framebuffer_height,
                      g_ui.menu_h, g_ui.status_h);
      break;

    default:
      break;
  }
}

#ifndef __EMSCRIPTEN__
// ---------------------------------------------------------------
//  端末復元 (cleanup_cb・シグナルハンドラ共通)
// ---------------------------------------------------------------
static void restore_terminal(void)
{
  if (g_term) { delete g_term; g_term = nullptr; }
}

// SIGINT/SIGTERM ハンドラ: 端末を復元してデフォルト動作に戻す
static void signal_cleanup(int sig)
{
  restore_terminal();
  signal(sig, SIG_DFL);
  raise(sig);
}
#endif

// ---------------------------------------------------------------
//  cleanup_cb 最後に一回呼ばれる
// ---------------------------------------------------------------
static void cleanup_cb(void)
{
#ifndef __EMSCRIPTEN__
  restore_terminal();
#endif
  Fxt::Ui::Shutdown();
  sg_shutdown();
  saudio_shutdown();
  sargs_shutdown();
  Psg::Shutdown(g_sys.psg);
  Fxt::Sd::UnmountImg(g_sys);
}

// ---------------------------------------------------------------
//  sokol_main  エントリポイント
// ---------------------------------------------------------------
sapp_desc sokol_main(int argc, char* argv[])
{
#ifndef __EMSCRIPTEN__
  // SIGINT/SIGTERM で端末状態を復元する
  signal(SIGINT,  signal_cleanup);
  signal(SIGTERM, signal_cleanup);
#endif

  // コマンドライン引数パース
  sargs_desc sargs_d = {};
  sargs_d.argc = argc;
  sargs_d.argv = argv;
  sargs_setup(&sargs_d);

  // CPU周波数 cpu_hz=8000000
  if (sargs_exists("cpu_hz"))
    g_sys.cfg.cpu_hz = atoi(sargs_value("cpu_hz"));

  // シミュレーション速度倍率 speed=1.0
  if (sargs_exists("speed"))
    g_sys.cfg.sim_speed = (float)atof(sargs_value("speed"));

  sapp_desc desc = {};
  desc.init_cb      = init_cb;
  desc.frame_cb     = frame_cb;
  desc.event_cb     = event_cb;
  desc.cleanup_cb   = cleanup_cb;
  desc.width        = WINDOW_W;
  desc.height       = WINDOW_H;
  desc.window_title = "FxT-65 Emulator";
  desc.logger.func  = slog_func;
#ifdef __EMSCRIPTEN__
  // canvas_resize=true: CanvasをCSSサイズではなくdesc.width/heightで固定する
  // (falseだとsokolがCSSを読んでフレームバッファを作るため初期表示が崩れる)
  desc.html5.canvas_resize = true;
#endif
  return desc;  // sokol appの記述子を返す
}
