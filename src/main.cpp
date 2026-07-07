#include <Arduino.h>

// Current wiring:
// Motor A:
// ESP32 GPIO3 -> L298N ENA
// ESP32 GPIO4 -> L298N IN1
// ESP32 GPIO5 -> L298N IN2
//
// Motor B:
// ESP32 GPIO10 -> L298N ENB
// ESP32 GPIO6  -> L298N IN3
// ESP32 GPIO7  -> L298N IN4
//
// Also required:
// ESP32 GND -> L298N GND
// Motor power supply + -> L298N +12V/VMS
// Motor power supply - -> L298N GND
// Motor A wires -> L298N OUT1 and OUT2
// Motor B wires -> L298N OUT3 and OUT4
//
// Servos:
// ESP32 GPIO2  -> servo 1 signal wire
// ESP32 GPIO11 -> servo 2 signal wire
// Servo red wires -> separate 5V supply/regulator
// Servo brown/black wires -> supply GND
// Servo supply GND -> ESP32 GND
constexpr uint8_t MOTOR_A_EN_PIN = 3;
constexpr uint8_t MOTOR_A_IN1_PIN = 4;
constexpr uint8_t MOTOR_A_IN2_PIN = 5;

constexpr uint8_t MOTOR_B_EN_PIN = 10;
constexpr uint8_t MOTOR_B_IN3_PIN = 6;
constexpr uint8_t MOTOR_B_IN4_PIN = 7;

constexpr uint8_t SERVO_1_PIN = 2;
constexpr uint8_t SERVO_2_PIN = 11;
constexpr uint8_t RGB_LED_PIN = 8;

constexpr uint32_t MOTOR_PWM_HZ = 1000;
constexpr uint8_t MOTOR_PWM_BITS = 8;

constexpr uint32_t SERVO_PWM_HZ = 50;
constexpr uint8_t SERVO_PWM_BITS = 16;
constexpr uint32_t SERVO_PERIOD_US = 20000;
constexpr uint32_t SERVO_MIN_US = 600;
constexpr uint32_t SERVO_MAX_US = 2400;

// Gentle limits for continuous low-speed running with about 8V into an L298N.
constexpr uint8_t MOTOR_RUN_SPEED = 130;
constexpr uint8_t MOTOR_RAMP_STEP = 5;
constexpr uint16_t MOTOR_RAMP_DELAY_MS = 150;

constexpr int SERVO_MIN_ANGLE = 45;
constexpr int SERVO_MAX_ANGLE = 135;
constexpr uint16_t SERVO_STEP_DELAY_MS = 40;

uint32_t pulseToServoDuty(uint32_t pulseUs) {
  const uint32_t maxDuty = (1UL << SERVO_PWM_BITS) - 1;
  return (pulseUs * maxDuty) / SERVO_PERIOD_US;
}

uint32_t angleToServoPulse(int angle) {
  angle = constrain(angle, 0, 180);
  return map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
}

void writeServoAngle(int angle) {
  const uint32_t pulseUs = angleToServoPulse(angle);
  const uint32_t duty = pulseToServoDuty(pulseUs);

  ledcWrite(SERVO_1_PIN, duty);
  ledcWrite(SERVO_2_PIN, duty);

  static uint32_t lastPrintMs = 0;
  if (millis() - lastPrintMs >= 1000) {
    Serial.print("Servo angles: ");
    Serial.print(angle);
    Serial.print(" | pulse: ");
    Serial.print(pulseUs);
    Serial.println(" us");
    lastPrintMs = millis();
  }
}

void setStatusColor(uint8_t red, uint8_t green, uint8_t blue) {
#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, red, green, blue);
#else
  rgbLedWrite(RGB_LED_PIN, red, green, blue);
#endif
}

void writeMotorSpeed(uint8_t speed) {
  ledcWrite(MOTOR_A_EN_PIN, speed);
  ledcWrite(MOTOR_B_EN_PIN, speed);
}

void rampSpeed(uint8_t fromSpeed, uint8_t toSpeed);

void setDirectionForward() {
  digitalWrite(MOTOR_A_IN1_PIN, HIGH);
  digitalWrite(MOTOR_A_IN2_PIN, LOW);
  digitalWrite(MOTOR_B_IN3_PIN, HIGH);
  digitalWrite(MOTOR_B_IN4_PIN, LOW);
  setStatusColor(0, 80, 0);
}

void setDirectionReverse() {
  digitalWrite(MOTOR_A_IN1_PIN, LOW);
  digitalWrite(MOTOR_A_IN2_PIN, HIGH);
  digitalWrite(MOTOR_B_IN3_PIN, LOW);
  digitalWrite(MOTOR_B_IN4_PIN, HIGH);
  setStatusColor(80, 0, 0);
}

void motorsCoastStop() {
  writeMotorSpeed(0);
  digitalWrite(MOTOR_A_IN1_PIN, LOW);
  digitalWrite(MOTOR_A_IN2_PIN, LOW);
  digitalWrite(MOTOR_B_IN3_PIN, LOW);
  digitalWrite(MOTOR_B_IN4_PIN, LOW);
  setStatusColor(0, 0, 80);
}

void startMotorsLowSpeed() {
  Serial.println("Starting DC motors at low constant speed");
  setDirectionForward();
  rampSpeed(0, MOTOR_RUN_SPEED);
}

void rampSpeed(uint8_t fromSpeed, uint8_t toSpeed) {
  if (fromSpeed <= toSpeed) {
    for (uint8_t speed = fromSpeed; speed <= toSpeed; speed += MOTOR_RAMP_STEP) {
      writeMotorSpeed(speed);
      Serial.print("Speed: ");
      Serial.println(speed);
      delay(MOTOR_RAMP_DELAY_MS);
    }
  } else {
    for (int speed = fromSpeed; speed >= toSpeed; speed -= MOTOR_RAMP_STEP) {
      writeMotorSpeed(speed);
      Serial.print("Speed: ");
      Serial.println(speed);
      delay(MOTOR_RAMP_DELAY_MS);
    }
  }

  writeMotorSpeed(toSpeed);
}

void setup() {
  pinMode(MOTOR_A_IN1_PIN, OUTPUT);
  pinMode(MOTOR_A_IN2_PIN, OUTPUT);
  pinMode(MOTOR_B_IN3_PIN, OUTPUT);
  pinMode(MOTOR_B_IN4_PIN, OUTPUT);

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("L298N low-speed motors + slow dual-servo sweep");
  Serial.println("Motor A: GPIO3=ENA, GPIO4=IN1, GPIO5=IN2");
  Serial.println("Motor B: GPIO10=ENB, GPIO6=IN3, GPIO7=IN4");
  Serial.println("Servo 1: GPIO2=signal");
  Serial.println("Servo 2: GPIO11=signal");
  Serial.println("RGB LED: white=boot, green=motors running");

  if (!ledcAttach(MOTOR_A_EN_PIN, MOTOR_PWM_HZ, MOTOR_PWM_BITS)) {
    Serial.println("Failed to attach PWM to ENA");
    while (true) {
      delay(1000);
    }
  }

  if (!ledcAttach(MOTOR_B_EN_PIN, MOTOR_PWM_HZ, MOTOR_PWM_BITS)) {
    Serial.println("Failed to attach PWM to ENB");
    while (true) {
      delay(1000);
    }
  }

  if (!ledcAttach(SERVO_1_PIN, SERVO_PWM_HZ, SERVO_PWM_BITS)) {
    Serial.println("Failed to attach PWM to servo 1 pin");
    while (true) {
      delay(1000);
    }
  }

  if (!ledcAttach(SERVO_2_PIN, SERVO_PWM_HZ, SERVO_PWM_BITS)) {
    Serial.println("Failed to attach PWM to servo 2 pin");
    while (true) {
      delay(1000);
    }
  }

  setStatusColor(80, 80, 80);
  writeServoAngle(90);
  delay(1000);
  motorsCoastStop();
  startMotorsLowSpeed();
}

void loop() {
  for (int angle = SERVO_MIN_ANGLE; angle <= SERVO_MAX_ANGLE; angle++) {
    writeServoAngle(angle);
    delay(SERVO_STEP_DELAY_MS);
  }

  for (int angle = SERVO_MAX_ANGLE; angle >= SERVO_MIN_ANGLE; angle--) {
    writeServoAngle(angle);
    delay(SERVO_STEP_DELAY_MS);
  }
}
