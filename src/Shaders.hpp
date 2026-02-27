/* src/Shaders.hpp - フレームバッファ表示シェーダ
 *
 * 頂点バッファなし: gl_VertexID / vertex_id から NDC 座標を生成する。
 * ユニフォーム scale_x/y でアスペクト比を維持しつつ中央配置し、
 * offset_y でメニュー/ステータスバー分の上下オフセットを与える。
 */
#pragma once

// ---------------------------------------------------------------
//  GLES3 (Web / Emscripten)
// ---------------------------------------------------------------
#ifdef SOKOL_GLES3

static const char* vs_src = R"(
#version 300 es
uniform float scale_x;
uniform float scale_y;
uniform float offset_y;
out vec2 uv;
void main() {
    vec2 ndc = vec2(
        (gl_VertexID & 1) != 0 ? 1.0 : -1.0,
        (gl_VertexID & 2) != 0 ? -1.0 : 1.0);
    gl_Position = vec4(ndc.x * scale_x, ndc.y * scale_y + offset_y, 0.0, 1.0);
    uv = vec2(
        (gl_VertexID & 1) != 0 ? 1.0 : 0.0,
        (gl_VertexID & 2) != 0 ? 1.0 : 0.0);
}
)";

static const char* fs_src = R"(
#version 300 es
precision mediump float;
uniform sampler2D tex;
in vec2 uv;
out vec4 frag_color;
void main() { frag_color = texture(tex, uv); }
)";

// ---------------------------------------------------------------
//  Metal (macOS)
// ---------------------------------------------------------------
#else

static const char* vs_src = R"(
#include <metal_stdlib>
using namespace metal;
struct vs_out { float4 pos [[position]]; float2 uv; };
struct Uniforms { float scale_x; float scale_y; float offset_y; };
vertex vs_out _main(uint vid [[vertex_id]],
    constant Uniforms& u [[buffer(0)]]) {
    vs_out o;
    float2 ndc = float2((vid & 1) ? 1.0 : -1.0,
                        (vid & 2) ? -1.0 : 1.0);
    o.pos = float4(ndc.x * u.scale_x, ndc.y * u.scale_y + u.offset_y, 0.0, 1.0);
    o.uv  = float2((vid & 1) ? 1.0 : 0.0,
                   (vid & 2) ? 1.0 : 0.0);
    return o;
}
)";

static const char* fs_src = R"(
#include <metal_stdlib>
using namespace metal;
struct vs_out { float4 pos [[position]]; float2 uv; };
fragment float4 _main(vs_out in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler smp [[sampler(0)]]) {
    return tex.sample(smp, in.uv);
}
)";

#endif
