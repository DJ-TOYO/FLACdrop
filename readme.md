FLACdrop Kai(改) Ver1.00

FLACdrop KaiはWAV、FLACの相互コンバートが行えます。
またMP3への変換も行えます。
FLACdrop Kai(改)はFLACdropをベースに改造させて頂きました。

変更点
- コマンドライン対応 ※ショートカットへD&D可能
- AUTOモード追加 WAV/FLACの拡張子を判断して相互コンバートします。
  ※コマンドラインにファイルを渡した場合は、AUTOモードになります。
- 設定保存をレジストリからINIファイルに変更。これによりアンインストールはフォルダ削除するだけになります。
  ※レジストリVerも残しております。ビルド構成を変更してビルドして下さい。
- ウィンド位置を覚えるようにした。
- WAVのHi-RES対応。WAVファイルの互換性向上、BIT深度16以外の20/24/32f/64fもサポート。
  ※20/32f/64fは24BITとしてFALCに変換されます。
- FLACからMP3に変換したときのタグ情報向上
  UTF-16対応を行い、日本語等も正しく反映されるようにした。
  ジャケットなど一部のタグが未対応だったが反映されるようにした。
- FLAC ver1.5に差し替え
- LAME V4に差し替え
- デフォルトでスレッド数が1だったが、初回起動時に物理CPUコアを調べてコア数をスレッド数(最大16)にするようにした。
- プログレスバーが100%にならない問題を修正
- 不適切と思われるソース、ロジックを修正


ORIGINAL Git HUB
UmmonPwr
https://github.com/UmmonPwr/FLACdrop


ORIGINAL readme.me
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
FLACdrop can convert between different audio file formats. You can drop the files on it to start the conversion.

Aim was to speed up the encoding process. The used audio libraries are designed as single thread in mind, so to utilize multithread capabilities it is launching independent encoder threads for each file.
FLACdrop can run maximum eight parallel threads. Maximum parallel thread number can be set in the options menu to adjust for actual CPU performance.

Currently it can convert:
- from WAV to MP3
- from WAV to FLAC
- from FLAC to WAV
- from FLAC to MP3

FLACdrop is using the below audio libraries. Only the headers and the pre-built lib files are included:
- libflac 1.3.4 GitHub version ( https://github.com/xiph/flac )
- libogg 1.3.3 GitHub version ( https://github.com/xiph/ogg )
- libmp3lame 3.100.2 ( http://lame.sourceforge.net/ )
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
