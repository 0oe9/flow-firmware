#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

//  Designer: [Kim Hyejin]
//  Context: Flow Prototype / Emotional Communication Device


// ---------------- Pin Definition ----------------
// 사용되는 센서 및 액추에이터 연결 핀
const int LDR1_PIN   = A0;   // 상단 조도 센서 ① (빛의 변화 감지)
const int LDR2_PIN   = A2;   // 상단 조도 센서 ② (보정용)
const int SHOCK_PIN  = 3;    // 충격 센서 (리듬 감지)
const int MOTOR_PIN  = 9;    // 진동 모터
const int LED_PIN    = 6;    // 네오픽셀 LED (감정 빛 표현)

// DFPlayerMini 오디오 모듈
const int DFPLAYER_RX = 4;   // DF TX → Arduino D4
const int DFPLAYER_TX = 5;   // DF RX ← Arduino D5
SoftwareSerial dfSerial(DFPLAYER_RX, DFPLAYER_TX);
DFRobotDFPlayerMini dfp;
bool dfpReady = false;

// ---------------- LED Strip Setup ----------------
Adafruit_NeoPixel strip = Adafruit_NeoPixel(8, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------------- Light Thresholds ----------------
// 손의 존재와 감싸는 정도를 구분하기 위한 조도 기준
const int TH1      = 200;    // 얹기 감지
const int TH2      = 500;    // 감싸기 감지
const int TOUCH_TH = 300;    // 손의 접근 감지 임계값

// ---------------- System State ----------------
bool firstTouchMode = true;        // 최초 접촉 상태
unsigned long lastNoTouchTime = 0; // 손이 떨어진 시각
bool offSoundPlayed = false;       // ‘이별 사운드’ 중복 방지

// ---------------- Idle State ----------------
// 사용자의 무반응 상태를 감지해 ‘잊혀진 감정의 여운’을 표현
unsigned long lastActivityMs = 0;
bool idleMode = false;
int idleTrack = 3; // 0003~0006 : 잔잔한 연결음

// ---------------- Shock Sensor ----------------
bool shockState = false;
unsigned long lastShockMs   = 0;
unsigned long lastShockTime = 0;
int shockCount = 0;
const unsigned long shockDebounceMs = 80;
const unsigned long shockTimeout    = 300;
const int MOTOR_STRENGTH = 220;

// ------------------------------------------------
//  Activity Tracking : “감정적 상호작용이 있었는가?”
// ------------------------------------------------
void markActivity() {
  lastActivityMs = millis();
  if (idleMode && dfpReady) dfp.stop();
  idleMode = false;
}

// ------------------------------------------------
//  Setup : 시스템 초기화 및 첫 감정 신호 재생
// ------------------------------------------------
void setup() {
  pinMode(SHOCK_PIN, INPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  Serial.begin(9600);

  strip.begin();
  strip.show();

  // DFPlayer 초기화
  dfSerial.begin(9600);
  if (dfp.begin(dfSerial)) {
    dfpReady = true;
    dfp.volume(30);
    delay(500);
    dfp.play(1);  // 0001: ‘Awakening’ — 전원 ON 사운드
  }

  lastActivityMs = millis();
}

// ------------------------------------------------
//  Loop : 사용자와의 감정적 상호작용 시퀀스
// ------------------------------------------------
void loop() {
  int ldr1Val = analogRead(LDR1_PIN);
  int ldr2Val = analogRead(LDR2_PIN);
  bool darkBoth = (ldr1Val > TOUCH_TH && ldr2Val > TOUCH_TH);

  // ---------------- 손이 떠난 상태 ----------------
  if (!darkBoth) {
    if (lastNoTouchTime == 0) lastNoTouchTime = millis();

    // 30초간 손이 닿지 않으면 — ‘이별 사운드’
    if (!offSoundPlayed && (millis() - lastNoTouchTime > 30000UL)) {
      if (dfpReady) {
        dfp.volume(30);
        dfp.play(2);  // 0002: ‘Goodbye’ — 연결 해제의 음
      }
      firstTouchMode = true;
      offSoundPlayed = true;
    }

    // LED와 진동 비활성화
    if (!firstTouchMode) {
      setColor(0, 0, 0);
      analogWrite(MOTOR_PIN, 0);
    }

  // ---------------- 손이 닿은 상태 ----------------
  } else {
    lastNoTouchTime = 0;
    offSoundPlayed = false;

    // 첫 접촉 : ‘재회’의 사운드와 흰빛
    if (firstTouchMode) {
      if (dfpReady) {
        dfp.volume(30);
        dfp.play(1);  // 0001: ‘Reconnection’
      }
      firstTouchEffect(255, 255, 255);
      firstTouchMode = false;
      markActivity();

    } else {
      // 감싸기 — 따뜻한 노란빛 + 부드러운 호흡 진동
      if (ldr1Val > TH2) {
        breatheColorAndMotor(225, 225, 100, 200);
        markActivity();
      }
      // 얹기 — 푸른빛 + 잔잔한 진동
      else if (ldr1Val > TH1) {
        breatheColorAndMotor(100, 150, 255, 150);
        markActivity();
      }
    }
  }

  // ---------------- 충격 (리듬) 반응 ----------------
  int shockVal = digitalRead(SHOCK_PIN);
  if (shockVal == HIGH && !shockState) {
    unsigned long now = millis();
    if (now - lastShockMs > shockDebounceMs) {
      shockCount++;
      lastShockTime = now;
      lastShockMs = now;
      markActivity();
    }
    shockState = true;
  } else if (shockVal == LOW && shockState) {
    shockState = false;
  }

  // 감정 리듬 확정 — 짧은 진동으로 회신
  if (shockCount > 0 && (millis() - lastShockTime > shockTimeout)) {
    vibratePattern(shockCount, MOTOR_STRENGTH);
    markActivity();
    shockCount = 0;
  }

  // ---------------- Idle Sound (무반응 상태) ----------------
  if (!idleMode && (millis() - lastActivityMs > 30000UL)) {
    if (dfpReady) {
      dfp.volume(28);
      dfp.play(idleTrack);  // 0003~0006: 잔잔한 존재감
      idleMode = true;
      idleTrack++;
      if (idleTrack > 6) idleTrack = 3;
    }
  }

  delay(50);
}

// ------------------------------------------------
//  감정 표현 루틴
// ------------------------------------------------

// 빛과 색
void setColor(int r, int g, int b) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

// 첫 접촉 — 밝아지고 사라지는 ‘숨결’ 효과
void firstTouchEffect(int r, int g, int b) {
  for (int i = 0; i < 255; i += 15) { setColor(r*i/255, g*i/255, b*i/255); delay(40); }
  delay(1000);
  for (int i = 255; i > 0; i -= 15) { setColor(r*i/255, g*i/255, b*i/255); delay(40); }
}

// 빛과 진동이 함께 호흡하는 루프
void breatheColorAndMotor(int r, int g, int b, int motorMax) {
  for (int i = 0; i < 255; i += 7) {
    setColor(r*i/255, g*i/255, b*i/255);
    analogWrite(MOTOR_PIN, motorMax*i/255);
    delay(90);
  }
  for (int i = 255; i > 0; i -= 7) {
    setColor(r*i/255, g*i/255, b*i/255);
    analogWrite(MOTOR_PIN, motorMax*i/255);
    delay(90);
  }
  analogWrite(MOTOR_PIN, 0);
}

// 충격 패턴에 따른 감정 진동 (응답의 울림)
void vibratePattern(int count, int strength) {
  for (int i = 0; i < count; i++) {
    analogWrite(MOTOR_PIN, strength);
    delay(200);
    analogWrite(MOTOR_PIN, 0);
    delay(200);
  }
}
