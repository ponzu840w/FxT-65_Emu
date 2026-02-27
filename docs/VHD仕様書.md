# 仮想ハードディスクイメージフォーマット仕様書 勝手訳 (Virtual Hard Disk Image Format Specification)

底本 2006年10月11日 - バージョン 1.0

翻訳 令和8年2月27日 - バージョン 1.0

---

**底本についての情報**
このドキュメントは、Microsoftが提供している [仮想ハード ディスク イメージ形式の仕様](https://download.microsoft.com/download/f/f/e/ffef50a5-07dd-4cf8-aaa3-442c0673a029/Virtual%20Hard%20Disk%20Format%20Spec_10_18_06.doc)をもとにしています。

底本の著作権情報: © 2005 Microsoft Corporation. All rights reserved.

底本の示す仕様について: "This specification is provided under the Microsoft Open Specification Promise.  For further details on the Microsoft Open Specification Promise, please refer to: http://www.microsoft.com/interop/osp/default.mspx.  Microsoft may have patents, patent applications, trademarks, copyrights, or other intellectual property rights covering subject matter in these materials. Except as expressly provided in the Microsoft Open Specification Promise, the furnishing of these materials does not give you any license to these patents, trademarks, copyrights, or other intellectual property."

---

### 概要 (Abstract)

本ドキュメントでは、Microsoft Virtual PC および Virtual Server でサポートされているさまざまな仮想ハードディスクフォーマットについて説明し、データの保存方法に関する情報を提供します。


## はじめに (Introduction)

本ドキュメントでは、Microsoft Virtual PC および Virtual Server 製品でサポートされている各種ハードディスクフォーマットについて説明します。ハードディスクと仮想マシンのインターフェース方法や、ATA（AT Attachment）ハードディスク、SCSI（Small Computer System Interface）ハードディスクに関する情報は提供しません。ホストファイルシステム上のファイルにデータを保存する方法に焦点を当てています。

本ドキュメントでは以下の用語を使用します：

* **システム (System)**: Virtual PC、Virtual Server、またはその両方を指します。
* **絶対バイトオフセット (Absolute Byte Offset)**: ファイルの先頭からのバイトオフセットを指します。
* **予約済み (Reserved)**: 予約済みとマークされたフィールドは、非推奨であるか、将来の使用のために予約されています。
* **セクタ長 (Sector length)**: セクタ長は常に512バイトです。

ファイルフォーマット内のすべての値は、特に明記されていない限り、ネットワークバイトオーダー（ビッグエンディアン）で保存されます。また、特に指定がない限り、すべての予約済み値はゼロに設定する必要があります。

---

## 仮想ハードディスクイメージタイプの概要 (Overview of Virtual Hard Disk Image Types)

仮想マシンのハードディスクは、ネイティブのホストファイルシステム上に存在するファイルとして実装されます。Microsoft Virtual PC および Virtual Server では、以下のタイプの仮想ハードディスクフォーマットがサポートされています：

* 容量固定ハードディスクイメージ (Fixed hard disk image)
* 容量可変ハードディスクイメージ (Dynamic hard disk image)
* 差分ハードディスクイメージ (Differencing hard disk image)

### 容量固定ハードディスクイメージ (Fixed Hard Disk Image)

容量固定ハードディスクイメージは、仮想ディスクのサイズに合わせて割り当てられるファイルです。たとえば、2 GBの仮想ハードディスクを作成すると、システムは約2 GBのホストファイルを作成します。

データ用に割り当てられたスペースの後にフッタ構造が続きます。ファイル全体のサイズは、ゲストオペレーティングシステム内のハードディスクのサイズにフッタのサイズを加えたものになります。ホストファイルシステムのサイズ制限（FAT32での4 GB制限など）により、固定ハードディスクのサイズが制限される場合があります。

### 容量可変ハードディスクイメージ (Dynamic Hard Disk Image)

容量可変ハードディスクイメージは、実際に書き込まれたデータのサイズにヘッダとフッタのサイズを加えたファイルです。割り当てはブロック単位で行われます。データが書き込まれるにつれて、ブロックを追加で割り当てることで動的にファイルサイズが増加します。

動的ハードディスクには、ユーザーデータにアクセスするためのメタデータが保存されます。最大サイズは2040 GBですが、実際のサイズは基盤となるディスクハードウェアプロトコル（ATAの127 GB制限など）によって制限されます。

**動的ディスクの基本フォーマット**：

| 動的ディスクヘッダフィールド (Dynamic Disk header fields) |
| --- |
| ハードディスクフッタのコピー (512バイト) |
| 動的ディスクヘッダ (1024バイト) |
| BAT (ブロック割り当てテーブル) |
| データブロック 1 |
| データブロック 2 |
| ... |
| データブロック n |
| ハードディスクフッタ (512バイト) |

データブロックが追加されるたびに、ハードディスクのフッタはファイルの末尾に移動する必要があります。フッタは重要な部分であるため、冗長性の目的でファイルの先頭にヘッダとしてミラーリングされます。

### 差分ハードディスクイメージ (Differencing Hard Disk Image)

差分ハードディスクイメージは、親イメージと比較して変更されたブロックのセットとして、仮想ハードディスクの現在の状態を表します。このタイプは独立しておらず、完全に機能するためには別のハードディスクイメージに依存します。親イメージは、別の差分ディスクを含む任意のディスクイメージタイプにすることができます。

---

## ハードディスクフッタフォーマット (Hard Disk Footer Format)

すべてのハードディスクイメージは、基本的なフッタフォーマットを共有します。

| ハードディスクフッタフィールド | サイズ (バイト) |
| --- | --- |
| Cookie | 8 |
| Features | 4 |
| File Format Version | 4 |
| Data Offset | 8 |
| Time Stamp | 4 |
| Creator Application | 4 |
| Creator Version | 4 |
| Creator Host OS | 4 |
| Original Size | 8 |
| Current Size | 8 |
| Disk Geometry | 4 |
| Disk Type | 4 |
| Checksum | 4 |
| Unique Id | 16 |
| Saved State | 1 |
| Reserved | 427 |

> **注:** Microsoft Virtual PC 2004より前のバージョンでは、511バイトのディスクフッタを作成します。したがって、フッタはファイルの最後の511バイトまたは512バイトに存在する可能性があります。

### ハードディスクフッタフィールドの説明

* **Cookie**: オリジナルの作成者を一意に識別します。Microsoftは `"conectix"` (8文字ASCII) を使用します。
* **Features**: 機能サポートを示すビットフィールド。
* `0x00000000`: 機能無効
* `0x00000001`: 一時的 (Temporary)
* `0x00000002`: 予約済み (常に1に設定)


* **File Format Version**: メジャー/マイナーバージョン。現在は `0x00010000` で初期化する必要があります。
* **Data Offset**: 次の構造体への絶対バイトオフセット。固定ディスクの場合は `0xFFFFFFFF` に設定します。
* **Time Stamp**: 作成時刻（2000年1月1日 12:00:00 AM UTC/GMT からの秒数）。
* **Creator Application**: 作成したアプリケーション。"vpc " (Virtual PC) または "vs  " (Virtual Server) など。
* **Creator Version**: アプリケーションのバージョン。
* **Creator Host OS**: ホストOSのタイプ。`0x5769326B` (Wi2k - Windows)、`0x4D616320` (Mac  - Macintosh)。
* **Original Size**: 作成時の仮想マシンから見たハードディスクのサイズ。
* **Current Size**: 現在のサイズ。
* **Disk Geometry**: シリンダ (2バイト)、ヘッド (1バイト)、トラックあたりのセクタ数 (1バイト)。
* **Disk Type**:
* 0: None
* 2: 容量固定ハードディスク (Fixed hard disk)
* 3: 容量可変ハードディスク (Dynamic hard disk)
* 4: 差分ハードディスク (Differencing hard disk)


* **Checksum**: フッタのチェックサム（チェックサムフィールドを除外した全バイトの合計の1の補数）。
* **Unique ID**: ハードディスクを一意に識別するための128ビットUUID。親ディスクと差分ディスクの関連付けに使用されます。
* **Saved State**: 保存状態フラグ（保存状態の場合は1）。
* **Reserved**: 427バイトのゼロ。

---

## 動的ディスクヘッダフォーマット (Dynamic Disk Header Format)

動的および差分ディスクイメージの場合、フッタ内の「Data Offset」フィールドが、追加情報を提供する二次構造体を指します。動的ディスクヘッダはセクタ（512バイト）境界に配置する必要があります。

| 動的ディスクヘッダフィールド | サイズ (バイト) |
| --- | --- |
| Cookie | 8 |
| Data Offset | 8 |
| Table Offset | 8 |
| Header Version | 4 |
| Max Table Entries | 4 |
| Block Size | 4 |
| Checksum | 4 |
| Parent Unique ID | 16 |
| Parent Time Stamp | 4 |
| Reserved | 4 |
| Parent Unicode Name | 512 |
| Parent Locator Entry 1〜8 | 各24 |
| Reserved | 256 |

### 動的ディスクヘッダフィールドの説明

* **Cookie**: `"cxsparse"` が格納されます。
* **Data Offset**: 常に `0xFFFFFFFF` に設定します。
* **Table Offset**: ブロック割り当てテーブル (BAT) の絶対バイトオフセット。
* **Header Version**: `0x00010000` で初期化します。
* **Max Table Entries**: BAT内の最大エントリ数。
* **Block Size**: 拡張単位。デフォルトは `0x00200000` (2 MB) です。
* **Checksum**: 動的ヘッダのチェックサム。
* **Parent Unique ID**: 差分ディスク用。親ハードディスクの128ビットUUID。
* **Parent Time Stamp**: 親ハードディスクの変更タイムスタンプ。
* **Parent Unicode Name**: 親ハードディスクのファイル名 (UTF-16)。
* **Parent Locator Entries**: 差分ディスク用の親ロケーター情報（プラットフォーム固有のパスなど）。

**Parent Locator Entry の構造:**

* **Platform Code**: プラットフォーム固有のフォーマットを記述します（`W2ru`, `W2ku`, `Mac`, `MacX`など）。
* **Platform Data Space**: 必要な512バイトセクタ数。
* **Platform Data Length**: 実際の長さをバイト単位で格納します。
* **Platform Data Offset**: ロケーターデータが保存されている絶対ファイルオフセット。

---

## ブロック割り当てテーブルとデータブロック (Block Allocation Table and Data Blocks)

BATは、ハードディスクをバックアップするファイルへの絶対セクタオフセットのテーブルです。動的ディスクヘッダの「Table Offset」から参照されます。BATエントリはそれぞれ4バイト長で、未使用のエントリは `0xFFFFFFFF` に初期化されます。

各データブロックは、セクタビットマップとデータで構成されます。動的ディスクの場合、セクタビットマップは有効なデータを含むセクタ（1）と変更されていないセクタ（0）を示します。

---

## 動的ディスクの実装 (Implementing a Dynamic Disk)

ブロックはオンデマンドで割り当てられます。作成時にはブロックは割り当てられていません。データが書き込まれると、新しいブロックを含むように拡張され、BATが更新されます。

### ディスクセクタからブロック内のセクタへのマッピング

* `BlockNumber = floor(RawSectorNumber / SectorsPerBlock)`
* `SectorInBlock = RawSectorNumber % SectorsPerBlock`
* `ActualSectorLocation = BAT [BlockNumber] + BlockBitmapSectorCount + SectorInBlock`

ブロックが割り当てられると、画像フッタをファイルの末尾にプッシュバックする必要があります。

---

## ハードディスクイメージの分割 (Splitting Hard Disk Images)

Virtual Server 2005より前のバージョンでは、ホストファイルシステムでサポートされている最大ファイルサイズを超える場合、ディスクイメージの分割をサポートしていました。分割ファイルにはヘッダやフッタはなく、生のデータのみです（最後のファイルにはフッタがあります）。拡張子は `.vhd` の後、`.v01`, `.v02` ... と続き、最大64個まで作成可能です。

---

## 差分ハードディスクの実装 (Implementing a Differencing Hard Disk)

差分ハードディスクは、自身の中に親ハードディスクのファイルロケーターを格納します。仮想マシンが差分ディスクを開こうとすると、両方が開かれます。

Windowsでは2種類のプラットフォームロケーターがあります：

* **W2ku**: 親ディスクへの絶対パス (`c:\directory\parent.vhd`)
* **W2ru**: 親ディスクへの相対パス (`.\directory\parent.vhd`)

**書き込み操作**: すべてのデータは差分ディスクに書き込まれ、ビットマップがダーティとマークされます。
**読み取り操作**: ダーティとマークされたセクタは差分ディスクから読み取られ、クリーンなセクタは親ディスクから読み取られます。
**親ディスクの変更**: 差分ディスク作成後は、親ハードディスクを変更してはなりません。

---

## 付録: CHS計算 (Appendix: CHS Calculation)

CHSは、ディスクイメージに存在する合計データセクタ数に基づいて計算されます。

```c
                    C     H    S
if (totalSectors > 65535 * 16 * 255)
{
   totalSectors = 65535 * 16 * 255;
}

if (totalSectors >= 65535 * 16 * 63)
{
   sectorsPerTrack = 255;
   heads = 16;
   cylinderTimesHeads = totalSectors / sectorsPerTrack;
}
else
{
   sectorsPerTrack = 17;
   cylinderTimesHeads = totalSectors / sectorsPerTrack;

   heads = (cylinderTimesHeads + 1023) / 1024;

   if (heads < 4)
   {
      heads = 4;
   }
   if (cylinderTimesHeads >= (heads * 1024) || heads > 16)
   {
      sectorsPerTrack = 31;
      heads = 16;
      cylinderTimesHeads = totalSectors / sectorsPerTrack;
   }
   if (cylinderTimesHeads >= (heads * 1024))
   {
      sectorsPerTrack = 63;
      heads = 16;
      cylinderTimesHead = totalSectors / sectorsPerTrack;
   }
}
cylinders = cylinderTimesHead / heads;

```

---

## チェックサム計算 (Checksum Calculation)

```c
checksum = 0;
driveFooter.Checksum = 0;
for (counter = 0 ; counter < driveFooterSize ; counter++)
{
   checksum += driveFooter[counter];
}
driveFooter.Checksum = ~checksum;

```
