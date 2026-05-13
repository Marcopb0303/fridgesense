// FridgeSense Phase 1 — Analog Food Safety Monitor
// Teensy 4.0 + 6-bit R-2R DAC (pins 14-19)
//
// Phase 1: Potentiometer simulates combined sensor input (0-3.3V → ADC)
// DAC output encodes food safety index (0-2.2V analog)
// Speaker plays distinct tone patterns per alert zone
// LED brightness tracks DAC output continuously
//
// Pin assignments:
//   DAC bits:    pins 14(MSB) 15 16 17 18 19(LSB)
//   POT input:   A0  (pin 14 analog — use separate analog pin, see note)
//   Speaker:     pin 20 (tone output, connect through DAC output node)
//   LED:         pin 21 (PWM, brightness = safety index)
//   MODE button: pin 22 (cycles food profile: fruits/veg/dairy/leftovers)
//
// Note: Use A8 (pin 22 analog) for pot input to avoid conflict with DAC pins

// ─── DAC pin definitions ─────────────────────────────────────────────────────
const int DAC_PINS[6] = {14, 15, 16, 17, 18, 19}; // MSB → LSB

// ─── Other pin definitions ────────────────────────────────────────────────────
const int POT_PIN     = A8;   // potentiometer input (simulates sensor)
const int LED_PIN     = 21;   // PWM LED (brightness = safety index)
const int SPEAKER_PIN = 20;   // tone output
const int MODE_PIN    = 22;   // food profile selector button

// ─── Food safety zones ────────────────────────────────────────────────────────
// DAC output ranges (0-63 codes → 0-2.166V)
// Zone 0: SAFE     0-20   (0.00-0.69V)
// Zone 1: MONITOR  21-38  (0.72-1.31V)  
// Zone 2: WARNING  39-52  (1.34-1.79V)
// Zone 3: ALERT    53-63  (1.82-2.17V)

const int ZONE_MONITOR = 21;
const int ZONE_WARNING = 39;
const int ZONE_ALERT   = 53;

// ─── Food profiles ────────────────────────────────────────────────────────────
// Each profile scales how aggressively the pot reading maps to the safety index
// Higher sensitivity = reaches warning zone sooner
struct FoodProfile {
  const char* name;
  float sensitivity;   // multiplier on raw ADC → DAC mapping
  int   max_days;      // for Phase 2 time tracking
};

const FoodProfile PROFILES[] = {
  {"Fruits",      1.2,  7},
  {"Vegetables",  1.4,  5},
  {"Dairy",       1.0, 14},
  {"Leftovers",   1.6,  3}
};
const int NUM_PROFILES = 4;

// ─── Tone signatures per zone ─────────────────────────────────────────────────
// Each zone has a distinct rhythm and frequency so you know what triggered
// SAFE:    silence
// MONITOR: single soft low tone every 10s
// WARNING: two rising tones every 5s
// ALERT:   rapid triple pulse every 2s

void tone_monitor() {
  tone(SPEAKER_PIN, 440, 200);   // A4, gentle single beep
}

void tone_warning() {
  tone(SPEAKER_PIN, 523, 150);   // C5
  delay(200);
  tone(SPEAKER_PIN, 659, 200);   // E5 — rising interval signals urgency
}

void tone_alert() {
  for (int i = 0; i < 3; i++) {
    tone(SPEAKER_PIN, 880, 100); // A5, rapid triple pulse
    delay(150);
  }
}

// ─── State variables ──────────────────────────────────────────────────────────
int  current_profile  = 0;
int  current_zone     = 0;
int  current_dac_code = 0;
bool btn_last         = HIGH;

unsigned long last_tone_time  = 0;
unsigned long tone_interval   = 10000; // ms between tones (zone-dependent)
unsigned long last_sample_time = 0;
const unsigned long SAMPLE_INTERVAL = 100; // sample pot every 100ms

// ─── DAC write ────────────────────────────────────────────────────────────────
void dac_write(int code) {
  // code: 0-63, MSB on pin 14, LSB on pin 19
  code = constrain(code, 0, 63);
  for (int i = 0; i < 6; i++) {
    digitalWrite(DAC_PINS[i], (code >> (5 - i)) & 0x01);
  }
  current_dac_code = code;
}

// ─── Safety index computation ─────────────────────────────────────────────────
int compute_dac_code(int adc_val, float sensitivity) {
  // adc_val: 0-1023 (10-bit on Teensy analog read)
  // Maps through profile sensitivity to 0-63 DAC code
  float normalized = (float)adc_val / 1023.0;
  float scaled     = normalized * sensitivity;
  scaled           = constrain(scaled, 0.0, 1.0);
  return (int)(scaled * 63.0);
}

// ─── Zone detection ───────────────────────────────────────────────────────────
int get_zone(int code) {
  if (code >= ZONE_ALERT)   return 3;
  if (code >= ZONE_WARNING) return 2;
  if (code >= ZONE_MONITOR) return 1;
  return 0;
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  for (int i = 0; i < 6; i++) {
    pinMode(DAC_PINS[i], OUTPUT);
    digitalWrite(DAC_PINS[i], LOW);
  }
  pinMode(LED_PIN,     OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(MODE_PIN,    INPUT_PULLUP);

  Serial.begin(115200);
  delay(500);

  Serial.println("FridgeSense Phase 1 — Ready");
  Serial.println("Rotate pot to simulate sensor input.");
  Serial.print("Active profile: ");
  Serial.println(PROFILES[current_profile].name);

  // Startup tone — three quick beeps confirms system is live
  for (int i = 0; i < 3; i++) {
    tone(SPEAKER_PIN, 660, 80);
    delay(120);
  }
}

// ─── Main loop ────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── Button: cycle food profile ──────────────────────────────────────────────
  bool btn = digitalRead(MODE_PIN);
  if (btn == LOW && btn_last == HIGH) {
    current_profile = (current_profile + 1) % NUM_PROFILES;
    Serial.print("Profile → ");
    Serial.println(PROFILES[current_profile].name);
    // Confirm tone: two quick beeps
    tone(SPEAKER_PIN, 784, 80); delay(120);
    tone(SPEAKER_PIN, 988, 80);
    delay(50);
  }
  btn_last = btn;

  // ── Sample pot and update DAC ───────────────────────────────────────────────
  if (now - last_sample_time >= SAMPLE_INTERVAL) {
    last_sample_time = now;

    int adc_val = analogRead(POT_PIN);
    int dac_code = compute_dac_code(adc_val, PROFILES[current_profile].sensitivity);
    int zone     = get_zone(dac_code);

    dac_write(dac_code);

    // LED brightness tracks DAC code linearly (0-63 → 0-255 PWM)
    analogWrite(LED_PIN, dac_code * 4);

    // Update tone interval based on zone
    if (zone != current_zone) {
      current_zone = zone;
      switch (zone) {
        case 0: tone_interval = 0;      break; // silence
        case 1: tone_interval = 10000;  break; // every 10s
        case 2: tone_interval = 5000;   break; // every 5s
        case 3: tone_interval = 2000;   break; // every 2s
      }
      Serial.print("Zone → ");
      const char* zone_names[] = {"SAFE", "MONITOR", "WARNING", "ALERT"};
      Serial.print(zone_names[zone]);
      Serial.print("  DAC code: ");
      Serial.print(dac_code);
      Serial.print("  (~");
      Serial.print(dac_code * 2166.0 / 63.0, 0);
      Serial.println(" mV)");
    }
  }

  // ── Tone playback (non-blocking, interval-based) ────────────────────────────
  if (current_zone > 0 && tone_interval > 0) {
    if (now - last_tone_time >= tone_interval) {
      last_tone_time = now;
      switch (current_zone) {
        case 1: tone_monitor(); break;
        case 2: tone_warning(); break;
        case 3: tone_alert();   break;
      }
    }
  }
}
