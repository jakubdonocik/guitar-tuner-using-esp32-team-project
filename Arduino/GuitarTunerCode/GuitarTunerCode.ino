#include <LiquidCrystal.h>
#include <arduinoFFT.h>

// Konfiguracja LCD
// RS, E, D4, D5, D6, D7
LiquidCrystal lcd(4, 6, 10, 11, 12, 13);

// Definicje znaków specjalnych LCD 
byte centerMark[8] = { B00100, B00100, B00100, B00100, B00100, B00100, B00100, B00100 };
byte hollowSquare[8] = { B00000, B11111, B10001, B10001, B10001, B10001, B11111, B00000 };
byte tunedMark[8] = { B00100, B11111, B10101, B10101, B10101, B10101, B11111, B00100 };

// Nazwy dźwięków w notacji tradycyjnej (kropka oznacza półton/krzyżyk, np. C. = C#)
const char* noteNames[] = {"C", "C.", "d", "d.", "E", "F", "F.", "G", "G.", "A", "A.", "b"};
int lastNoteIndex = -1;
int lastSquarePos = -1;
const int centerPos = 8; // Pozycja środkowa paska strojenia na LCD (kolumna 😎

// --- PARAMETRY FFT ---
#define SAMPLES       128       
#define SAMPLING_FREQ 4800.0    // Częstotliwość próbkowania [Hz]. 
#define ADC_PIN       A0        // Pin z sygnałem z przedwzmacniacza/mikrofonu
#define NOISE_FLOOR   50.0      // Próg szumu

float vReal[SAMPLES];
float vImag[SAMPLES];
const unsigned long samplePeriodUs = (unsigned long)(1000000.0 / SAMPLING_FREQ);

ArduinoFFT<float> FFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);

void setup() {
  Serial.begin(115200);
  analogReference(DEFAULT);  
  
  lcd.begin(16, 2);
  lcd.clear();              
  
  lcd.createChar(0, centerMark);
  lcd.createChar(1, hollowSquare);
  lcd.createChar(2, tunedMark);
  
  lcd.setCursor(centerPos, 1);
  lcd.write((uint8_t)0);
}

void loop() {
  // Zebranie próbek z pinu A0
  unsigned long t0 = micros();
  for (int i = 0; i < SAMPLES; i++) {
    vReal[i] = (float)analogRead(ADC_PIN); 
    vImag[i] = 0.0; 

    unsigned long targetTime = t0 + (unsigned long)i * samplePeriodUs;
    while (micros() < targetTime) { /* busy wait */ }
  }

  // FFT
  float mean = 0;
  for (int i = 0; i < SAMPLES; i++) mean += vReal[i];
  mean /= SAMPLES;
  for (int i = 0; i < SAMPLES; i++) vReal[i] -= mean;

  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();
  float peakFreq = FFT.majorPeak();

  float maxMag = 0;
  for (int i = 2; i < (SAMPLES / 2); i++) {  
    if (vReal[i] > maxMag) maxMag = vReal[i];
  }

  // Wizualizacja na LCD
  if (maxMag > NOISE_FLOOR && peakFreq > 50.0 && peakFreq < 1200.0) {
    
    Serial.print(F("f0 = ")); Serial.print(peakFreq, 1); Serial.print(F(" Hz | mag = ")); Serial.println(maxMag, 0);

    // Konwersja częstotliwości na numer nuty w standardzie MIDI
    float midiNote = 12 * log(peakFreq / 440.0) / log(2) + 69;
    int noteIndex = (int)round(midiNote) % 12;
    if (noteIndex < 0) noteIndex += 12; // Zabezpieczenie przed ujemnym indeksem tablicy
    
    // Obliczanie odchylenia od idealnego dźwięku w centach (-50 do +50)
    float cents = (midiNote - round(midiNote)) * 100;
    int currentSquarePos = centerPos + (int)(cents / 6.25);
    currentSquarePos = constrain(currentSquarePos, 0, 15);

    if (noteIndex != lastNoteIndex) {
      lcd.setCursor(0, 0);
      lcd.print("Dzwiek: ");
      lcd.print(noteNames[noteIndex]);
      lcd.print("   "); 
      lastNoteIndex = noteIndex;
    }

    if (currentSquarePos != lastSquarePos) {
      if (lastSquarePos != -1) {
        lcd.setCursor(lastSquarePos, 1);
        if (lastSquarePos == centerPos) {
          lcd.write((uint8_t)0);
        } else {
          lcd.print(" "); 
        }
      }

      lcd.setCursor(currentSquarePos, 1);
      if (currentSquarePos == centerPos) {
        lcd.write(2); 
      } else {
        lcd.write(1); 
        
        lcd.setCursor(centerPos, 1);
        if (currentSquarePos != centerPos) {
          lcd.write((uint8_t)0);
        }
      }
      lastSquarePos = currentSquarePos;
    }
    
  } else {
    // Sekcja wykonywana przy ciszy lub gdy sygnał jest zbyt cichy
    if (lastNoteIndex != -2) {
       lcd.setCursor(0, 0); lcd.print("Dzwiek: ");
       lcd.setCursor(0, 1); lcd.print("                ");
       lcd.setCursor(centerPos, 1); lcd.write((uint8_t)0); 
       lastNoteIndex = -2;
       lastSquarePos = -1;
    }
  }

  delay(20); 
}