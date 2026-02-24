/* src/Sd.cpp */
#include "Sd.hpp"
#include "FxtSystem.hpp"
#include <cstring>
#include <algorithm>

//#define DEBUG_SD // 定義するとデバッグ情報が出る

namespace Fxt
{
namespace Sd
{

  // ---------- 内部ヘルパー----------
  static void LoadSector(System& sys)
  {
    State& sd = sys.sd;
    if (sd.is_sparse)
    {
      // スパース形式: LBA で二分探索
      auto it = std::lower_bound(
        sd.sparse_sectors.begin(), sd.sparse_sectors.end(),
        sd.current_lba,
        [](const SparseSector& s, uint32_t lba){ return s.lba < lba; });
      if (it != sd.sparse_sectors.end() && it->lba == sd.current_lba)
        memcpy(sd.sector_buffer, it->data, 512);
      else
        memset(sd.sector_buffer, 0, 512); // 未記録セクタはゼロ
    }
    else
    {
      // フラット形式
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
  }

  static void FlushSector(System& sys)
  {
    State& sd = sys.sd;
    if (sd.is_sparse)
    {
      // SPRS: 既存エントリ更新 or 新規挿入（ソート順を維持）
      auto it = std::lower_bound(
        sd.sparse_sectors.begin(), sd.sparse_sectors.end(),
        sd.current_lba,
        [](const SparseSector& s, uint32_t lba){ return s.lba < lba; });
      if (it != sd.sparse_sectors.end() && it->lba == sd.current_lba)
      {
        memcpy(it->data, sd.sector_buffer, 512);
      }
      else
      {
        SparseSector ns;
        ns.lba = sd.current_lba;
        memcpy(ns.data, sd.sector_buffer, 512);
        sd.sparse_sectors.insert(it, ns);
      }
      sd.sparse_dirty = true;
      return;
    }
    if (!sd.image_fp) return;
    long offset = (long)sd.current_lba * 512;
    fseek(sd.image_fp, offset, SEEK_SET);
    fwrite(sd.sector_buffer, 1, 512, sd.image_fp);
    fflush(sd.image_fp);
  }
  // ---------- 内部ヘルパー----------

  // イメージファイルを開く
  bool MountImg(System& sys, const std::string& filename)
  {
    UnmountImg(sys);
    sys.sd.image_fp = fopen(filename.c_str(), "r+b");
    if (!sys.sd.image_fp)
    {
      printf("[SD] SD card image not found.\n");
      exit(1);
    }

    // SPRS スパース形式チェック
    uint8_t magic[4] = {};
    if (fread(magic, 1, 4, sys.sd.image_fp) == 4 && memcmp(magic, "SPRS", 4) == 0)
    {
      uint32_t count = 0;
      fread(&sys.sd.total_sectors, 4, 1, sys.sd.image_fp);
      fread(&count,                4, 1, sys.sd.image_fp);
      sys.sd.sparse_sectors.resize(count);
      for (uint32_t i = 0; i < count; i++)
      {
        fread(&sys.sd.sparse_sectors[i].lba,  4,   1, sys.sd.image_fp);
        fread( sys.sd.sparse_sectors[i].data, 512, 1, sys.sd.image_fp);
      }
      sys.sd.is_sparse = true;
      printf("[SD] Mounted '%s' as SPRS sparse (Sectors: %u, Entries: %u)\n",
             filename.c_str(), sys.sd.total_sectors, count);
      return true;
    }

    // フラット形式
    rewind(sys.sd.image_fp);
    fseek(sys.sd.image_fp, 0, SEEK_END);
    sys.sd.total_sectors = ftell(sys.sd.image_fp) / 512;
    rewind(sys.sd.image_fp);
    printf("[SD] Mounted '%s' (Sectors: %d)\n", filename.c_str(), sys.sd.total_sectors);
    return true;
  }

  // イメージファイルを放棄
  void UnmountImg(System& sys)
  {
    State& sd = sys.sd;
    if (sd.image_fp)
    {
      // SPRS: 変更があればファイルに書き戻す
      if (sd.is_sparse && sd.sparse_dirty)
      {
        rewind(sd.image_fp);
        uint32_t count = (uint32_t)sd.sparse_sectors.size();
        fwrite("SPRS",           1, 4, sd.image_fp);
        fwrite(&sd.total_sectors, 4, 1, sd.image_fp);
        fwrite(&count,            4, 1, sd.image_fp);
        for (auto& s : sd.sparse_sectors)
        {
          fwrite(&s.lba,  4,   1, sd.image_fp);
          fwrite( s.data, 512, 1, sd.image_fp);
        }
        fflush(sd.image_fp);
      }
      fclose(sd.image_fp);
      sd.image_fp    = nullptr;
      sd.is_sparse   = false;
      sd.sparse_dirty = false;
      sd.sparse_sectors.clear();
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
