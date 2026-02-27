/* src/sokol_impl_web.cpp - Sokol 実装定義 (GLES3/Emscripten backend) */

#define SOKOL_IMPL
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

// Web 環境では assets/ui_font.ttf (サブセット) を使用する
extern "C" const char* platform_get_ui_font_path(void) { return "assets/ui_font.ttf"; }
