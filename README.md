# jam18_arduino_firmware
水中ロボコン in JAMSTEC 2018向けに作った水中ロボのArduinoソフトウェア

Arduino MEGA向け

# 必要なライブラリ
- TimerThree
下記のURLからダウンロードする．
https://github.com/PaulStoffregen/TimerThree
ここを参考にしてダウンロードしたライブラリを読み込む
https://www.arduino.cc/en/Guide/Libraries#toc4

# このソフトの特徴
- PPM信号の生成が可能
- ブラシレスモータのESCを動作可能

# 簡単なプログラムの動作
1. シリアルから8bitの信号を受信
1. 受信信号に当てはまるパターンをswitch文で探す
1. 該当する操作をする．大体はモータの回転数をアップするとか，止めるとか
1. くりかえす．
