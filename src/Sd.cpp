/* src/Sd.cpp */
#include "Sd.hpp"
#include "FxtSystem.hpp"
#include <cstring>

//#define DEBUG_SD // 定義するとデバッグ情報が出る

namespace Fxt
{
namespace Sd
{

  // ---------- VHD ビッグエンディアン ヘルパー ----------
  static uint32_t read_be32(const uint8_t* p)
  {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
  }

  static uint64_t read_be64(const uint8_t* p)
  {
    return ((uint64_t)read_be32(p) << 32) | (uint64_t)read_be32(p + 4);
  }

  static void write_be32(uint8_t* p, uint32_t v)
  {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v);
  }

  // ---------- 内部ヘルパー ----------
  static void LoadSector(System& sys)
  {
    State& sd = sys.sd;

    if (sd.file_type == State::DYNAMIC_VHD)
    {
      uint32_t block_num      = sd.current_lba / sd.sectors_per_block;
      uint32_t sector_in_block = sd.current_lba % sd.sectors_per_block;

      // 未割り当てブロックはゼロを返す
      if (block_num >= (uint32_t)sd.bat.size() || sd.bat[block_num] == 0xFFFFFFFF)
      {
        memset(sd.sector_buffer, 0, 512);
        return;
      }

      uint64_t block_byte  = (uint64_t)sd.bat[block_num] * 512;
      uint64_t sector_byte = block_byte
                           + (uint64_t)sd.bitmap_sectors * 512
                           + (uint64_t)sector_in_block   * 512;
      fseek(sd.image_fp, (long)sector_byte, SEEK_SET);
      size_t r = fread(sd.sector_buffer, 1, 512, sd.image_fp);
      if (r < 512) memset(sd.sector_buffer + r, 0, 512 - r);
      return;
    }

    // FLAT / FIXED_VHD: どちらも先頭から LBA×512 の位置に生データ
    if (!sd.image_fp)
    {
      memset(sd.sector_buffer, 0xFF, 512);
      return;
    }
    long offset = (long)sd.current_lba * 512;
    fseek(sd.image_fp, offset, SEEK_SET);
    size_t r = fread(sd.sector_buffer, 1, 512, sd.image_fp);
    if (r < 512) memset(sd.sector_buffer + r, 0, 512 - r);
  }

  static void FlushSector(System& sys)
  {
    State& sd = sys.sd;

    if (sd.file_type == State::DYNAMIC_VHD)
    {
      uint32_t block_num       = sd.current_lba / sd.sectors_per_block;
      uint32_t sector_in_block = sd.current_lba % sd.sectors_per_block;

      if (block_num >= (uint32_t)sd.bat.size()) return;

      if (sd.bat[block_num] == 0xFFFFFFFF)
      {
        // 新規ブロックをファイル末尾（フッター直前）に確保
        fseek(sd.image_fp, 0, SEEK_END);
        long file_end    = ftell(sd.image_fp);
        long block_start = file_end - 512; // フッター512バイト分を除く

        // ビットマップ (全ビット1 = 書き込み済み) とゼロデータを書き込む
        uint8_t bitmap[512];
        memset(bitmap, 0xFF, sizeof(bitmap));
        uint8_t zero[512];
        memset(zero,   0x00, sizeof(zero));

        fseek(sd.image_fp, block_start, SEEK_SET);
        for (uint32_t i = 0; i < sd.bitmap_sectors;    i++) fwrite(bitmap, 1, 512, sd.image_fp);
        for (uint32_t i = 0; i < sd.sectors_per_block; i++) fwrite(zero,   1, 512, sd.image_fp);

        // フッターを末尾に再書き込み
        fwrite(sd.vhd_footer, 1, 512, sd.image_fp);

        // BAT エントリをメモリとファイル両方に即時反映
        uint32_t new_sector = (uint32_t)((uint64_t)block_start / 512);
        sd.bat[block_num] = new_sector;

        uint8_t be_val[4];
        write_be32(be_val, new_sector);
        fseek(sd.image_fp,
              (long)(sd.bat_file_offset + (uint64_t)block_num * 4),
              SEEK_SET);
        fwrite(be_val, 1, 4, sd.image_fp);
      }

      uint64_t block_byte  = (uint64_t)sd.bat[block_num] * 512;
      uint64_t sector_byte = block_byte
                           + (uint64_t)sd.bitmap_sectors * 512
                           + (uint64_t)sector_in_block   * 512;
      fseek(sd.image_fp, (long)sector_byte, SEEK_SET);
      fwrite(sd.sector_buffer, 1, 512, sd.image_fp);
      fflush(sd.image_fp);
      return;
    }

    // FLAT / FIXED_VHD
    if (!sd.image_fp) return;
    long offset = (long)sd.current_lba * 512;
    fseek(sd.image_fp, offset, SEEK_SET);
    fwrite(sd.sector_buffer, 1, 512, sd.image_fp);
    fflush(sd.image_fp);
  }
  // ---------- 内部ヘルパー ----------

  // イメージファイルを開く
  bool MountImg(System& sys, const std::string& filename)
  {
    UnmountImg(sys);
    State& sd = sys.sd;

    sd.image_fp = fopen(filename.c_str(), "r+b");
    if (!sd.image_fp)
    {
      printf("[SD] SD card image not found: %s\n", filename.c_str());
      return false;
    }

    // ---- VHD 判定: ファイル末尾 512 バイトがフッター ----
    fseek(sd.image_fp, 0, SEEK_END);
    long file_size = ftell(sd.image_fp);

    if (file_size >= 512)
    {
      fseek(sd.image_fp, file_size - 512, SEEK_SET);
      if (fread(sd.vhd_footer, 1, 512, sd.image_fp) == 512 &&
          memcmp(sd.vhd_footer, "conectix", 8) == 0)
      {
        uint32_t disk_type = read_be32(sd.vhd_footer + 60);

        if (disk_type == 2) // Fixed VHD
        {
          sd.file_type     = State::FIXED_VHD;
          sd.total_sectors = (uint32_t)((file_size - 512) / 512);
          printf("[SD] Mounted '%s' as Fixed VHD (Sectors: %u)\n",
                 filename.c_str(), sd.total_sectors);
          return true;
        }

        if (disk_type == 3) // Dynamic VHD
        {
          sd.file_type = State::DYNAMIC_VHD;

          // Current Size (offset 48, 8 bytes BE) からセクタ数を算出
          uint64_t disk_size = read_be64(sd.vhd_footer + 48);
          sd.total_sectors   = (uint32_t)(disk_size / 512);

          // Dynamic Disk Header (Data Offset, offset 16, 8 bytes BE)
          uint64_t dyn_offset = read_be64(sd.vhd_footer + 16);
          uint8_t  dyn_hdr[1024] = {};
          fseek(sd.image_fp, (long)dyn_offset, SEEK_SET);
          fread(dyn_hdr, 1, 1024, sd.image_fp);

          // BAT 情報
          sd.bat_file_offset    = read_be64(dyn_hdr + 16); // Table Offset
          uint32_t max_entries  = read_be32(dyn_hdr + 28); // Max Table Entries
          uint32_t block_size   = read_be32(dyn_hdr + 32); // Block Size (bytes)

          sd.sectors_per_block = block_size / 512;
          uint32_t bitmap_bytes = (sd.sectors_per_block + 7) / 8;
          sd.bitmap_sectors     = (bitmap_bytes + 511) / 512;

          // BAT をメモリに読み込む
          sd.bat.resize(max_entries);
          fseek(sd.image_fp, (long)sd.bat_file_offset, SEEK_SET);
          for (uint32_t i = 0; i < max_entries; i++)
          {
            uint8_t be_val[4];
            if (fread(be_val, 1, 4, sd.image_fp) == 4)
              sd.bat[i] = read_be32(be_val);
            else
              sd.bat[i] = 0xFFFFFFFF;
          }

          printf("[SD] Mounted '%s' as Dynamic VHD "
                 "(Sectors: %u, Blocks: %u, BlockSectors: %u)\n",
                 filename.c_str(), sd.total_sectors,
                 max_entries, sd.sectors_per_block);
          return true;
        }

        // Type 4 (Differencing) などは未サポート
        printf("[SD] Unsupported VHD disk type: %u\n", disk_type);
        fclose(sd.image_fp);
        sd.image_fp = nullptr;
        return false;
      }
    }

    // ---- フラット形式 ----
    sd.file_type     = State::FLAT;
    sd.total_sectors = (uint32_t)(file_size / 512);
    rewind(sd.image_fp);
    printf("[SD] Mounted '%s' (Sectors: %u)\n", filename.c_str(), sd.total_sectors);
    return true;
  }

  // イメージファイルを放棄
  void UnmountImg(System& sys)
  {
    State& sd = sys.sd;
    if (sd.image_fp)
    {
      fclose(sd.image_fp);
      sd.image_fp      = nullptr;
      sd.file_type     = State::FLAT;
      sd.total_sectors = 0;
      sd.bat.clear();
      sd.sectors_per_block = 0;
      sd.bitmap_sectors    = 0;
      sd.bat_file_offset   = 0;
    }
  }

  // CS（チップセレクト）信号入力状態を変更
  void SetCs(System& sys, bool active)
  {
    #ifdef DEBUG_SD
      fprintf(stderr, "[SD] SetCs=%d\n", active);
    #endif
    sys.sd.cs_active = active;
  }

  // 1バイトデータ送受
  uint8_t Transfer(System& sys, uint8_t mosi)
  {
    if (!sys.sd.cs_active) return 0xFF;

    #ifdef DEBUG_SD
      fprintf(stderr, "[SD] Transfer:0x%02X\n", mosi);
    #endif

    State& sd = sys.sd;
    switch (sd.phase)
    {
      case State::IDLE:
        #ifdef DEBUG_SD
          fprintf(stderr, "[SD] IDLE\n");
        #endif
        if ((mosi & 0xC0) == 0x40)
        {
          sd.phase = State::CMD_RECEIVE;
          sd.cmd_buffer[0] = mosi;
          sd.cmd_idx = 1;
        }
        return 0xFF;

      case State::CMD_RECEIVE:
        #ifdef DEBUG_SD
          fprintf(stderr, "[SD] CMD_RECEIVE\n");
        #endif
        sd.cmd_buffer[sd.cmd_idx++] = mosi;
        if (sd.cmd_idx >= 6)
        {
          uint8_t cmd = sd.cmd_buffer[0] & 0x3F;
          uint32_t arg = (sd.cmd_buffer[1] << 24) | (sd.cmd_buffer[2] << 16) |
                         (sd.cmd_buffer[3] << 8)  | sd.cmd_buffer[4];

          sd.resp_len = 0;
          sd.resp_idx = 0;
          sd.wait_cycles = 2;

          #ifdef DEBUG_SD
            if (sd.is_acmd) fprintf(stderr, "[SD] ACMD%d Arg:0x%08X -> ", cmd, arg);
            else            fprintf(stderr, "[SD] CMD%d Arg:0x%08X -> ", cmd, arg);
          #endif

          if (sd.is_acmd)
          {
            sd.is_acmd = false;
            if (cmd == 41) sd.response_buffer[0] = 0x00;
            else           sd.response_buffer[0] = 0x04;
            sd.resp_len = 1;
          }
          else
          {
            switch (cmd)
            {
              case 0:
                sd.response_buffer[0] = 0x01;
                sd.resp_len = 1; break;
              case 8:
                sd.response_buffer[0] = 0x01;
                sd.response_buffer[1] = 0x00;
                sd.response_buffer[2] = 0x00;
                sd.response_buffer[3] = 0x01;
                sd.response_buffer[4] = 0xAA;
                sd.resp_len = 5;
                break;
              case 55:
                sd.is_acmd = true;
                sd.response_buffer[0] = 0x01;
                sd.resp_len = 1;
                break;
              case 58:
                sd.response_buffer[0] = 0x00;
                sd.response_buffer[1] = 0xC0;
                sd.response_buffer[2] = 0xFF;
                sd.response_buffer[3] = 0x80;
                sd.response_buffer[4] = 0x00;
                sd.resp_len = 5;
                break;
              case 17:
                sd.current_lba = arg;
                LoadSector(sys);
                sd.response_buffer[0] = 0x00;
                sd.resp_len = 1;
                sd.phase = State::WAIT_RESPONSE;
                goto PREPARE_READ;
              case 24:
                sd.current_lba = arg;
                sd.response_buffer[0] = 0x00; sd.resp_len = 1;
                sd.phase = State::WAIT_RESPONSE;
                goto PREPARE_WRITE;
              default:
                sd.response_buffer[0] = 0x00;
                sd.resp_len = 1;
                break;
            }
          }

          #ifdef DEBUG_SD
            fprintf(stderr, "Resp:");
            for(int i=0; i<sd.resp_len; i++) fprintf(stderr, " %02X", sd.response_buffer[i]);
            fprintf(stderr, "\n");
          #endif

          sd.phase = State::WAIT_RESPONSE;
          return 0xFF;

          PREPARE_READ:
            sd.wait_cycles = 2;
            #ifdef DEBUG_SD
              fprintf(stderr, " (Read Sector Start)\n");
            #endif
            return 0xFF;
          PREPARE_WRITE:
            sd.wait_cycles = 2;
            #ifdef DEBUG_SD
              fprintf(stderr, " (Write Sector Start)\n");
            #endif
            return 0xFF;
        }
        return 0xFF;

      // コマンド受信からSDカードがレスポンスを返すまでの遅延時間
      case State::WAIT_RESPONSE:
        #ifdef DEBUG_SD
          fprintf(stderr, "[SD] WAIT_RESPONSE wait_cycles=%d\n", sd.wait_cycles);
        #endif
        if (sd.wait_cycles > 0)
        {
          sd.wait_cycles--;
          return 0xFF;
        }
        sd.phase = State::SEND_RESPONSE;
        // fallthrough
      // 受信したコマンドに対し、SDカードからレスポンスを返す
      case State::SEND_RESPONSE:
        {
          uint8_t res = sd.response_buffer[sd.resp_idx++];
          #ifdef DEBUG_SD
            fprintf(stderr, "[SD] SEND_RESPONSE res=%02X\n", res);
          #endif
          if (sd.resp_idx >= sd.resp_len)
          {
            uint8_t last_cmd = sd.cmd_buffer[0] & 0x3F;
            #ifdef DEBUG_SD
              fprintf(stderr, "[SD] last_cmd=%d\n", last_cmd);
            #endif
            if (last_cmd == 17)
            {
              sd.phase = State::READ_WAIT_TOKEN;
              sd.wait_cycles = 4;
            }
            else if (last_cmd == 24)
            {
              sd.phase = State::WRITE_WAIT_TOKEN;
            }
            else
            {
              sd.phase = State::IDLE;
            }
          }
          return res;
        }
      case State::READ_WAIT_TOKEN:
        #ifdef DEBUG_SD
          fprintf(stderr, "[SD] READ_WAIT_TOKEN\n");
        #endif
        if (sd.wait_cycles > 0)
        {
          sd.wait_cycles--;
          return 0xFF;
        }
        sd.phase = State::READ_SEND_DATA;
        sd.data_idx = 0;
        return 0xFE;
      case State::READ_SEND_DATA:
        {
          uint8_t d = sd.sector_buffer[sd.data_idx++];
          #ifdef DEBUG_SD
            fprintf(stderr, "[SD] READ_SEND_DATA d:0x%02X\n", d);
          #endif
          if (sd.data_idx >= 512)
          {
            // CMD17のデータ転送直後にIDLE状態に戻る（CRC出力はさぼる）
            sd.phase = State::IDLE;
            sd.data_idx = 0;
          }
          return d;
        }
      case State::WRITE_WAIT_TOKEN:
        #ifdef DEBUG_SD
          fprintf(stderr, "[SD] WRITE_WAIT_TOKEN\n");
        #endif
        if (mosi == 0xFE)
        {
          sd.phase = State::WRITE_RECEIVE;
          sd.data_idx = 0;
        }
        return 0xFF;
      case State::WRITE_RECEIVE:
        #ifdef DEBUG_SD
          fprintf(stderr, "[SD] WRITE_RECEIVE\n");
        #endif
        sd.sector_buffer[sd.data_idx++] = mosi;
        if (sd.data_idx >= 512)
        {
          sd.phase = State::WRITE_BUSY;
          sd.wait_cycles = 2;
          FlushSector(sys);
        }
        return 0xFF;
      case State::WRITE_BUSY:
        #ifdef DEBUG_SD
          fprintf(stderr, "[SD] WRITE_BUSY\n");
        #endif
        if (sd.wait_cycles > 0)
        {
          sd.wait_cycles--; return 0xFF;
        }
        sd.phase = State::IDLE; return 0x05;
    }
    return 0xFF;
  }

}
}
