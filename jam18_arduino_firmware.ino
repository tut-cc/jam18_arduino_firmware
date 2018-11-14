// 水中ロボット用制御プログラム
// PCからシリアル通信で送信します．TTLレベル．
// 1byteのデータですべて操作する
// ArduinoMEGA用です
// シリアル3にRX，TXを接続してPCとつなげる．
// 

// Servo関数はTimerOneを使うためこのTimerOneは使えない．
#include <TimerThree.h>   // PPM信号の生成に使用 https://github.com/PaulStoffregen/TimerThree からダウンロードして「zip形式のライブラリとしてインストール」してください．
#include <Servo.h>


// PPM信号生成用

#define NUM_CHANNEL 6 

//PPM信号のチャンネル（PPM信号に出力する信号の順序，若い番号の信号から順番に送出する）

#define ROLL     0
#define PITCH    1
#define THROTTLE 2
#define YAW      3
#define AUX1     4
#define AUX2     5

#define MIN_PULSE_TIME  1000 // 1000us
#define MAX_PULSE_TIME  2000 // 2000us

#define SYNC_PULSE_TIME  3050 // 3000us
 
#define PIN_PPM 5   // Arduinoの5番ピンに接続する

// ESC制御用
#define throttle_set 2000 //最大回転数時のPWM幅
#define throttle_low 1000 //ESCの最小のPWM幅（800us以上じゃないと認識しない）
 
// PPM信号の初期値

int pitch_val = 1500;
int roll_val = 1500;
int yaw_val = 1500;
int thrt_val = 1500;
int aux_val = 1000;

unsigned int ppm[NUM_CHANNEL];//0:thrt,1:roll,2:pitch,3:yaw

// ESC用
Servo throttle[6];
int level;

int serial_val = 0;
 
void setup() {
    pinMode(PIN_PPM, OUTPUT);
    
    Serial.begin(9600);
    Serial3.begin(9600);
    
    // ppm init
    Timer3.initialize(SYNC_PULSE_TIME);
    Timer3.attachInterrupt(isr_sendPulses);
    isr_sendPulses();
    for(int i=0;i<NUM_CHANNEL;i++){　// PPM信号の初期値
        ppm[i] = 1500;
    }

    // ESCの初期化
    delay(2000);
    throttle[1].attach(9);  //arduinoの 9 番ピンに接続
    throttle[2].attach(10); //arduinoの 10 番ピンに接続
    throttle[3].attach(11); //arduinoの 11 番ピンに接続
    throttle[4].attach(12); //arduinoの 12 番ピンに接続
    delay(1000);
    esc_cal();  // ESCの初期化
    Serial3.println("ready\n\r");
}


void loop(){
    
    if(Serial3.available() > 0){  // シリアルが使用可能なとき
        serial_val = Serial3.read(); 
        Serial.println(serial_val);   // ArduinoMEGAにUSBケーブルを接続してシリアルモニタを開くと受信信号の番号がでてくる．デバッグ用

        switch(serial_val){ // 受信信号に応じて動作を切り替える
            case 1:
            case 2:
            case 3:
            case 4:
                rotateMotor(serial_val);
                break;

           //ここからはドローンコントローラ用が多い
           
            case 5: // 回転stop
                roll_val = 1500;
                break;

            case 6: // 回転+
           // case '6':
                roll_val += 10;
                break;

            case 7: //回転-
            //case '7':
                roll_val -= 10;
                break;

            case 8: //下降
                thrt_val -= 10;
                break;

            case 9:// 上昇
                thrt_val += 10;
                break;

            // 20 - 40 左右 中立は30
            // 50−70　上下　中立は60

            case 100: // 水中葉モータ全部停止
                rotateMotor(5);
                break;

            case 101: // ホバリング:B
                pitch_val = 1500;
                yaw_val = 1500;
                break;
            
            case 102:   // arm
                aux_val = 2000;
                thrt_val = 1000;
                pitch_val = 1000;
                write_ppm();
                delay(100);
                pitch_val = 1500;
                break;
            
            case 103:   // disarm
                aux_val = 1000;
                break;
                

            default:
                break;
        }

        // pitchとyawはアナログ値なのでif文で処理している．
        if( (serial_val >= 20) && ( serial_val <= 40 )){ 
            pitch_val = map(serial_val - 20, 0,20,1250,1750); // シリアルで送られる20-40の数値を1250-1750に対応させている．MIN_PULSE_TIMEからMAX_PULSE_TIMEまでの範囲でしていできる．今回はフルスロットルはいらないので1250と1750を指定している．
        }else if( (serial_val >= 50) && ( serial_val <= 70 )){
            yaw_val = map(serial_val - 50, 0, 20, 1250, 1750);
        }
        write_ppm();
    }

    
}
 

void esc_cal(){ // 水中ロボ用ESCの初期化(キャリブレーション)，Arduino起動時に必ず実行する．（ESCの電源を入れたときにArduinoからの信号で初期化されることがあるので）
    throttle[1].writeMicroseconds(throttle_set);
    throttle[2].writeMicroseconds(throttle_set);
    throttle[3].writeMicroseconds(throttle_set);
    throttle[4].writeMicroseconds(throttle_set);
    Serial3.print("最大周期\n\r");
    delay(1000);  // 安定させる
    throttle[1].writeMicroseconds(throttle_low);
    throttle[2].writeMicroseconds(throttle_low);
    throttle[3].writeMicroseconds(throttle_low);
    throttle[4].writeMicroseconds(throttle_low);
    Serial3.print("最小周期\n\r");
    delay(1000);  // 安定させる
}

// PPM信号の生成

volatile int currentChannel = 0;
 
void isr_sendPulses() {
    digitalWrite(PIN_PPM, LOW);
    
    if (currentChannel == NUM_CHANNEL) { // 送信データが最後のときの動作．同期パルスの生成をする．
        Timer3.setPeriod(SYNC_PULSE_TIME);
        currentChannel = 0; // Will be 0 on next interrupt
    } else {
        
        Timer3.setPeriod(ppm[currentChannel]);  // タイマの周期を書き込む
        currentChannel++;
    }
    
    digitalWrite(PIN_PPM, HIGH); 
}

void write_ppm(void){ // グローバル変数の値を配列に入れる．ついでに制御範囲からの逸脱をしらべる．
    ppm[PITCH] = pitch_val;
    ppm[ROLL] = roll_val;
    ppm[YAW] = yaw_val;
    ppm[THROTTLE] = thrt_val;
    ppm[AUX1] = aux_val;
    
    for(int j=0;j<NUM_CHANNEL;j++){ // PPM信号を範囲内におさえる．制御範囲いから逸脱時は最大or最小値に書き換える
      if(ppm[currentChannel] < MIN_PULSE_TIME){ ppm[currentChannel] = MIN_PULSE_TIME; }
      if(ppm[currentChannel] > MAX_PULSE_TIME){ ppm[currentChannel] = MAX_PULSE_TIME; }
    }
    
}

// ESCを動作させるプログラム

void rotateMotor (int motorNumber){ // 引数に指定したモータのみを回転させる．それ以外のモータは停止．
  for (int i = 1; i <= 5 ; i++){
    if (i == motorNumber){  // モータを回転させる．
      throttle[i].writeMicroseconds(1500);
    }else{
      throttle[i].writeMicroseconds(1000);
    }
  }
}


