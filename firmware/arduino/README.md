# ColorCAN Arduino版

WCH Arduino Core 1.0.6でビルドしたCH32X035G8U6用のPIOC CAN試験FWです．CANは1 MbpsのClassic CANで，ch32fun版と同じDIP設定，RGBC，ハートビートを使います．Arduino版はUSB ISPでの書き込みとベリファイを確認済みです．CAN通信は実機未確認です．[書き込み用HEX](../colorcan_can_pioc_arduino.hex)

デバッグログはPA2（USART2 TX）から115200 bpsで出力します．3.3 VのUSB-UART変換器を使う場合は，変換器のRXをPA2，GNDを基板のGNDに接続してください．USB-Cからの書き込みは通常のUSB ISP手順です．

## ビルド

WindowsでArduino CLIとPowerShell 7を使用します．リポジトリのルートで実行してください．`arduino-cli` をPATHから呼び出せない場合は，Arduino IDEのユーザー別インストール先にあるCLIを探します．別の場所にある場合は `-ArduinoCli <パス>` で指定できます．

```powershell
pwsh -NoProfile -File .\firmware\arduino\build.ps1 -InstallCore
```

2回目以降は `-InstallCore` を省略できます．HEXは `firmware/arduino/build/colorcan_can_pioc_arduino.hex` に生成されます．このスクリプトは，CH32X035のRAM上位4 KiBをPIOC用に確保したリンカ設定を生成します．

## ライセンスと対応ソース

Arduino版にはWCH Arduino Core 1.0.6が静的リンクされています．Arduino CoreのLGPL対象部分にはGNU LGPL 2.1以降が適用されます．WCHのBSD対象部分にはBSD 3-Clauseが適用されます．Arduino版を自己使用のために改変し，その改変のデバッグに必要なリバースエンジニアリングを行えます．

- [ColorCANアプリケーションのソース](source/)
- [Arduino Core 1.0.6の対応ソース（`cores/arduino`）](third-party/arduino-core-ch32-1.0.6-source.zip) · [GNU LGPL 2.1全文](third-party/LGPL-2.1.txt)
- [WCH公式ボードパッケージ1.0.6（CH32X035のSDK・ボード定義を含む）](https://github.com/openwch/arduino_core_ch32/releases/download/1.0.6/arduino_core_ch32-1.0.6.zip) · [WCHのBSD 3-Clause表記](third-party/WCH-BSD-3-Clause.txt)
- [newlibの著作権表示とライセンス](third-party/newlib-COPYING.NEWLIB) · [GCC Runtime Library Exception](third-party/GCC-Runtime-Exception.txt) · [GNU GPLv3全文](third-party/GPL-3.0.txt)

Arduino Coreを改変して再リンクする場合は，初回ビルド後に対応ソースZIPの `arduino` ディレクトリを展開し，`%TEMP%\ColorCAN-Arduino\data\packages\WCH\hardware\ch32v\1.0.6\cores\arduino` のソースと差し替えて編集します．その後，`-InstallCore` を付けずにビルドコマンドを実行します．
