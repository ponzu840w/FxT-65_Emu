/* src/Sd.hpp */
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace Fxt
{

  struct System; // 前方宣言

  namespace Sd
  {

    // 内部状態
    struct State
    {
      // 動作状態
      enum Phase
      {
        IDLE,             // コマンド待機
        CMD_RECEIVE,      // コマンド受信中
        WAIT_RESPONSE,    // レスポンス前のウェイト(NCR)
        SEND_RESPONSE,    // レスポンス送信中
        // Read (CMD17)
        READ_WAIT_TOKEN,  // データトークン待ち
        READ_SEND_DATA,   // データ送信中
        READ_SEND_CRC,    // CRC送信中
        // Write (CMD24)
        WRITE_WAIT_TOKEN, // スタートトークン待ち
        WRITE_RECEIVE,    // データ受信中
        WRITE_BUSY        // 書き込みBusy
      } phase = IDLE;

      bool cs_active = false; // チップセレクト
      bool is_acmd = false;   // 次がACMDか

      // ファイル操作
      FILE* image_fp = nullptr;
      uint32_t total_sectors = 0;
      uint32_t current_lba = 0;

      // バッファ
      uint8_t cmd_buffer[6];       // 受信コマンド
      uint8_t cmd_idx = 0;

      uint8_t response_buffer[6];  // 返送用レスポンス
      uint8_t resp_len = 0;
      uint8_t resp_idx = 0;
      uint8_t wait_cycles = 0;     // NCRウェイト用

      uint8_t sector_buffer[512];  // セクタデータ
      uint16_t data_idx = 0;
    };

    // 操作関数
    uint8_t Transfer(System& sys, uint8_t data);
    void SetCs(System& sys, bool active);
    bool MountImg(System& sys, const std::string& filename);
    void UnmountImg(System& sys);

  }

}
