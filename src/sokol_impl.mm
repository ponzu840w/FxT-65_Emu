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
//  macOS VHD ファイルピッカー
//  NSOpenPanel で .vhd / .img を選択し、SD カードをマウントする
// ---------------------------------------------------------------
#import <Cocoa/Cocoa.h>
#include "FxtSystem.hpp"
#include "Sd.hpp"

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
