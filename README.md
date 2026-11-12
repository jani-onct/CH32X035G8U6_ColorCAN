# IRC27 ColorCAN

TCS34725FNで測定した赤・緑・青・Clearの値を、Classic CANで送るカラーセンサ基板です。基板シルクは **v1.0.0**、CANのハートビートで通知するプロトコル版は **1** です。

<img src="images/usb-side.png" alt="ColorCANのUSB-C面。上部にカラーセンサと照明LED、下部にUSB-CとBOOTボタン" width="350">
<img src="images/can-side.png" alt="ColorCANのCAN面。中央にDIPスイッチ、下部にJST XAコネクタ" width="350">

基板の配置図はKiCadの3D表示です。DIPスイッチの操作方向は、基板の `ON=1` と `TERM ON` を参照してください。

## 基板仕様

| 項目 | 仕様 |
|---|---|
| 外形・取付穴 | 30 × 28 mm、四隅に直径3.2 mmのM3用取付穴 |
| マイコン | WCH CH32X035G8U6、QFN-28 |
| カラーセンサ | ams OSRAM TCS34725FN。I²Cの7-bitアドレスは `0x29`。試験FWは積分時間50.4 ms、ゲイン1倍 |
| 照明・電源表示 | センサ両側の白色LED 2個、電源表示の赤色LED 1個 |
| 電源 | 5 V入力。XC6206P332MR-Gで基板内の3.3 Vを生成 |
| USB-C（J1） | 5 V給電、USB ISP書き込み、仮想COMでのデバッグ |
| CAN（J2） | JST XA 4極、2.5 mmピッチ、横型（S04B-XASK-1）。SN65HVD230使用、Rsは10 kΩでGNDへ接続 |
| DIP（SW1） | OTAX KHS42E、4連。3連でノード番号、1連で120 ΩのCAN終端を設定 |

## 配線とDIP設定

**DIP面を手前にして、XAコネクタを下**に向けたとき、J2のピンは左から次の順です。

| ピン | シルク | 信号 |
|---:|---|---|
| 1 | `GND` | 電源・CAN共通GND |
| 2 | `+5V` | 5 V電源入力 |
| 3 | `L` | CAN_L |
| 4 | `H` | CAN_H |

**USB-CのVBUSとJ2の+5Vは基板上で同じネットです。給電元は片方にしてください。** USB給電でCANを試す場合は、J2のGND・CAN_L・CAN_Hを接続し、J2の+5Vを開放します。J2給電中にUSBデバッグする場合は、VBUSを切ったデータ用USBケーブルを使用します。

| 基板上の位置 | 基板での役割 | KHS42E本体の番号 |
|---|---|---:|
| 一番上 | ノード番号の重み `1`（PA6） | 4 |
| 上から2段目 | ノード番号の重み `2`（PA7） | 3 |
| 上から3段目 | ノード番号の重み `4`（PB0） | 2 |
| 一番下 | CAN_HとCAN_Lの間の120 Ω終端 | 1 |

この向きでは、**左に倒すとON、右に倒すとOFF**です。KHS42Eを取り付ける場合は、本体の `ON` 印字を左、本体の `1` をXAコネクタ側に向けます。[KHS42Eの向きが分かる商品写真](https://akizukidenshi.com/img/goods/3/108929.jpg)

ONにした重みの合計がノード番号（0〜7）です。ONでGPIOはLowになり、FWではそのビットを1として扱います。設定を変えたら基板の電源を入れ直します。CANバスの物理的な両端でのみ120 Ω終端を有効にします。

## CANを受信する

**1 Mbps、Classic CAN、標準11-bit ID、データ長8バイト。** ノード番号はCAN IDの下位3ビットに入ります。

| フレーム | CAN ID | 送信タイミング |
|---|---|---|
| RGBC | `0x240 + ノード番号` | 有効な測定値を取得できたとき、100 msごと |
| ハートビート | `0x248 + ノード番号` | 約1秒ごと、またはセンサ状態が変化したとき |

1. ColorCAN 1台と、1 MbpsのCAN受信機1台を短い配線でつなぎます。CAN_H同士、CAN_L同士、GND同士を接続し、両端を120 Ωで終端します。
2. 受信機を通常モード（ACKあり）に設定します。まずID用DIPをすべてOFFにしてColorCANを起動すると、ノード番号は `0` です。
3. 受信機で `0x240` のRGBCと `0x248` のハートビートを確認します。IRC27 ParameterGatewayではWeb UIの「ColorCAN受信」に表示されます。

このFWは**ColorCAN 1台が送信するベンチ試験用**です。CH32X035のTIM1とDMAで送信と信号の読み返しを行います。複数の送信ノードが共存する運用バスに組み込むには、CANの調停、再送、エラー処理を備えた実装が必要です。ベンチではParameterGatewayでノード0の両フレームを受信確認済みです。

- [CAN仕様とデコード例](docs/CAN.md)
- [CANデータベース（DBC、ノード0〜7）](docs/ColorCAN.dbc)

## USBデバッグ

- 通常起動すると仮想COMポートにRGBCを約500 msごと、CAN送信の集計を約1秒ごとに出力します。COMポートの設定例は `115200` です。
- ログの `ACK` は送信中に確認したACKの件数、`NACK` はACKなし、`BUSY` は送信前にバスが空いていなかった回数、`ERR` は読み返し不一致やDMAエラーなどです。
- USB ISPで書き込む場合は、J2給電を外し、`BOOT` を押したままUSB-Cを接続します。USBで通常起動するときは `BOOT` を押さずに接続します。

## 基板内の信号

| 機能 | CH32X035側 | 接続先 |
|---|---|---|
| I²C | PA4（SCL）、PA5（SDA） | TCS34725FN。各信号を4.7 kΩで3.3 Vへプルアップ |
| センサ割り込み | PA1 | TCS34725FNのINTに接続。試験FWはI²Cポーリングで測定 |
| CAN | PC19（TX）、PC18（RX） | SN65HVD230のD、R |
| USB | PC16（D−）、PC17（D+） | USB-C。BOOTボタンはPC17側に接続 |
| UARTテストパッド | PA2（TX）、PA3（RX） | TP1、TP2 |

## 部品資料

- [WCH CH32X035](https://github.com/openwch/ch32x035)
- [ams OSRAM TCS3472データシート](https://look.ams-osram.com/m/7ec5bcc3e40679be/original/TCS3472-DS000390.pdf)
- [TI SN65HVD230データシート](https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf)
- [OTAX KHSシリーズ](https://www.otax.co.jp/service/khs/)
