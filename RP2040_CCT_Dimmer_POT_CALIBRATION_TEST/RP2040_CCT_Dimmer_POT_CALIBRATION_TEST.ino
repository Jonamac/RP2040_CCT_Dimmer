// === RP2040 Pot Calibration Tool ===
// Shows raw ADC values for both pots continuously
// Works with Serial Plotter
// Tracks min/max for each pot

const int DUTY_POT_PIN = A0;   // adjust if needed
const int CCT_POT_PIN  = A1;   // adjust if needed

// Smooth ADC by averaging 12 samples (same as your main code)
int readADC(int pin) {
  long sum = 0;
  for (int i = 0; i < 12; i++) {
    sum += analogRead(pin);
  }
  return sum / 12;
}

int dutyMin = 4095;
int dutyMax = 0;

int cctMin = 4095;
int cctMax = 0;

void setup() {
  analogReadResolution(12);   // <-- REQUIRED for 12-bit ADC
  Serial.begin(115200);
  delay(500);

  Serial.println("=== POT CALIBRATION STARTED ===");
  Serial.println("Turn each pot fully left and right several times.");
  Serial.println("Watch Serial Plotter for jitter and range.");
  Serial.println(" ");
}

void loop() {
  int dutyRaw = readADC(DUTY_POT_PIN);
  int cctRaw  = readADC(CCT_POT_PIN);

  // Track min/max
  if (dutyRaw < dutyMin) dutyMin = dutyRaw;
  if (dutyRaw > dutyMax) dutyMax = dutyRaw;

  if (cctRaw < cctMin) cctMin = cctRaw;
  if (cctRaw > cctMax) cctMax = cctRaw;

  // === Serial Plotter format ===
  // Plotter expects: label:value label:value ...
  Serial.print("Duty:");
  Serial.print(dutyRaw);
  Serial.print("  CCT:");
  Serial.print(cctRaw);

  Serial.print("  DutyMin:");
  Serial.print(dutyMin);
  Serial.print("  DutyMax:");
  Serial.print(dutyMax);

  Serial.print("  CCTMin:");
  Serial.print(cctMin);
  Serial.print("  CCTMax:");
  Serial.println(cctMax);

  delay(10);  // ~100 Hz update rate
}