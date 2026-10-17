/*
 * DO_PWM -- PWM on the digital outputs DO1..DO8.
 *
 * What it does: sets the duty cycle and frequency of any output from commands
 * typed in the Serial Monitor (line ending "Newline"):
 *   3 50       DO3 at 50 % duty
 *   3 0        DO3 off; 3 100 is always on
 *   f 2000     2000 Hz from the next output you set (default 1000 Hz)
 *   r 12       12-bit analogWrite() values from the next output you set
 *              (1..16, default 8); the duty is still typed in percent
 * Connect: a load on the output you test (a lamp, or a few kilohm), and a
 *   multimeter on DC volts across it.
 * Expect: each command is answered with one line; the meter reads about
 *   duty x supply voltage.
 * Limits of the board: the eight outputs sit on four timer channels.
 *   DO1/DO5, DO2/DO6, DO3/DO7 and DO4/DO8 each share one channel, so only one
 *   of a pair can run PWM -- setting one turns its partner off.
 *   DO1, DO2, DO5, DO6 share one timer and so one frequency; so do DO3, DO7,
 *   and DO4, DO8. The KNX library takes that first timer: with it, use
 *   DO3, DO4, DO7 or DO8. The highest frequency the output switch follows
 *   has not been measured -- watch the meter as you raise it.
 * Serial Monitor: on the board's USB port.
 */

OPENPLC_APP_VERSION(1, 0, 0);

const int outputs[] = {DOUT_1, DOUT_2, DOUT_3, DOUT_4, DOUT_5, DOUT_6, DOUT_7, DOUT_8};
const char *timerGroup[] = {"DO1, DO2, DO5, DO6", "DO1, DO2, DO5, DO6", "DO3, DO7", "DO4, DO8",
                            "DO1, DO2, DO5, DO6", "DO1, DO2, DO5, DO6", "DO3, DO7", "DO4, DO8"};

String line;
long frequency = 1000;
int resolution = 8;

int partner(int n) {
  return n <= 4 ? n + 4 : n - 4;
}

void setOutput(int n, int percent) {
  int p = partner(n);
  pinMode(outputs[p - 1], OUTPUT);
  digitalWrite(outputs[p - 1], LOW);
  analogWriteFrequency(frequency);
  analogWriteResolution(resolution);
  analogWrite(outputs[n - 1], percent * ((1L << resolution) - 1) / 100);

  Serial.print("DO");
  Serial.print(n);
  Serial.print(" = ");
  Serial.print(percent);
  Serial.print(" % at ");
  Serial.print(frequency);
  Serial.print(" Hz; DO");
  Serial.print(p);
  Serial.print(" off; frequency shared by ");
  Serial.println(timerGroup[n - 1]);
}

void command(String s) {
  s.trim();
  int space = s.indexOf(' ');
  if (space < 0) {
    Serial.println("? use: <output 1-8> <duty 0-100>, f <Hz>, or r <bits>");
    return;
  }
  String head = s.substring(0, space);
  long value = s.substring(space + 1).toInt();
  if (head == "f") {
    if (value < 1 || value > 100000) {
      Serial.println("? frequency 1..100000 Hz");
      return;
    }
    frequency = value;
    Serial.print("frequency ");
    Serial.print(frequency);
    Serial.println(" Hz from the next output you set");
    return;
  }
  if (head == "r") {
    if (value < 1 || value > 16) {
      Serial.println("? resolution 1..16 bits");
      return;
    }
    resolution = value;
    Serial.print("resolution ");
    Serial.print(resolution);
    Serial.print(" bits, full scale ");
    Serial.print((1L << resolution) - 1);
    Serial.println(" from the next output you set");
    return;
  }
  int n = head.toInt();
  if (n < 1 || n > 8 || value < 0 || value > 100) {
    Serial.println("? use: <output 1-8> <duty 0-100>, f <Hz>, or r <bits>");
    return;
  }
  setOutput(n, (int)value);
}

void printBanner() {
  Serial.println("DO_PWM: type <output 1-8> <duty 0-100>, e.g. 3 50; f <Hz> sets the frequency, r <bits> the resolution.");
}

// Prints the description each time the Serial Monitor connects, not once in
// setup(): every reset re-enumerates USB and drops what setup() printed (README).
void printBannerOnConnect() {
  static bool wasConnected = false;
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck < 250) {
    return;  // (bool)Serial takes 10 ms on USB
  }
  lastCheck = millis();
  bool now = (bool)Serial;
  if (now && !wasConnected) {
    printBanner();
  }
  wasConnected = now;
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 8; i++) {
    pinMode(outputs[i], OUTPUT);
    digitalWrite(outputs[i], LOW);
  }
}

void loop() {
  printBannerOnConnect();
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (line.length() > 0) {
        command(line);
        line = "";
      }
    } else {
      line += c;
    }
  }
}
