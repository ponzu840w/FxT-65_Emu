/* src/sokol_impl.mm - Sokol ライブラリ実装定義 (Metal backend, Obj-C++) */

#define SOKOL_IMPL
#ifndef SOKOL_METAL
#define SOKOL_METAL
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
//  macOS プラットフォーム関数
// ---------------------------------------------------------------
#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#include <string.h>
#include "FxtSystem.hpp"
#include "Sd.hpp"

// システムフォントのファイルパスを返す (見つからなければ "")
// Hiragino Sans (HiraginoSans-W3 等) を優先して検索する
extern "C" const char* platform_get_ui_font_path(void)
{
  static char s_buf[2048] = "";
  if (s_buf[0]) return s_buf; // キャッシュ済み

  @autoreleasepool {
    NSArray* names = @[@"HiraginoSans-W3", @"HiraKakuProN-W3", @"HiraKakuPro-W3"];
    for (NSString* name in names) {
      CTFontDescriptorRef desc = CTFontDescriptorCreateWithNameAndSize(
        (__bridge CFStringRef)name, 14.0);
      if (!desc) continue;
      CFURLRef urlRef = (CFURLRef)CTFontDescriptorCopyAttribute(desc, kCTFontURLAttribute);
      CFRelease(desc);
      if (urlRef) {
        NSURL* url = (__bridge NSURL*)urlRef;
        if (url.fileSystemRepresentation) {
          strlcpy(s_buf, url.fileSystemRepresentation, sizeof(s_buf));
          CFRelease(urlRef);
          return s_buf;
        }
        CFRelease(urlRef);
      }
    }
  }
  return "";
}

// ---------------------------------------------------------------
//  macOS VHD ファイルピッカー
//  NSOpenPanel で .vhd / .img を選択し、SD カードをマウントする
// ---------------------------------------------------------------

extern Fxt::System g_sys; // main.cpp で定義

void platform_open_vhd(void)
{
  @autoreleasepool {
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.title = @"SDカードイメージを開く";
    panel.allowsMultipleSelection = NO;
    panel.canChooseDirectories    = NO;
    panel.canChooseFiles          = YES;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    panel.allowedFileTypes = @[@"vhd", @"img"];
#pragma clang diagnostic pop

    if ([panel runModal] == NSModalResponseOK)
    {
      NSURL* url = panel.URLs.firstObject;
      if (url)
      {
        std::string path = url.fileSystemRepresentation;
        Fxt::Sd::UnmountImg(g_sys);
        if (!Fxt::Sd::MountImg(g_sys, path))
        {
          NSAlert* alert = [[NSAlert alloc] init];
          alert.messageText = @"SDカードイメージを開けませんでした";
          alert.informativeText = [NSString stringWithUTF8String:path.c_str()];
          [alert runModal];
        }
      }
    }
  }
}
