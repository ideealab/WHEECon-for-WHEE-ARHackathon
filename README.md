# WHEECon-for-WHEE-ARHackathon
これはM5StackCore2向けのファームウェアです。このファームウェアを使用すれば、プログラミング不要で、STYLYにより制作されたXRコンテンツ内でセンサ情報を利用できます。

ファームウェアの利用方法は下記のURLにご参考ください。
https://drive.google.com/file/d/1lwQie3a9wRp5ZBRF1kbOUnKDnULMeuMI/view?usp=sharing

【Arduino　IDEの利用】

- M5StackCore2のボード追加

 　 [space]1. 「Arduino(macOS) / ファイル(windows)」→「環境設定」をクリックして環境設定の画面を開きます。

 　 2. 「追加のボードマネージャーのURL」で下記のURLを追加します。

     https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json

   3. 「ツール」→「ボード…」→「ボードマネージャ…」と選択します

   4. ボードマネージャが開いたら「m5stack」と入力すると、下のウィンドウに「M5Stack」という名前のものが出るので、「インストール」をクリックします。


- 下記のライブラリのインストールが必要

 1. M5Core2　Library by M5Stack

 2. ESP32 BLE Keyboard library by T-vK

 3. VL53L0X library for Arduino
