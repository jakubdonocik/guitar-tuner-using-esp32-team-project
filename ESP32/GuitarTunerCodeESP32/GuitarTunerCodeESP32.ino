#include <LiquidCrystal.h>
#include <arduinoFFT.h>

// Konfiguracja LCD dla ESP32
// RS, E, D4, D5, D6, D7
LiquidCrystal lcd(13, 12, 14, 27, 26, 25);

// Definicje znaków specjalnych LCD 
byte centerMark[8] = { B00100, B00100, B00100, B00100, B00100, B00100, B00100, B00100 };
byte hollowSquare[8] = { B00000, B11111, B10001, B10001, B10001, B10001, B11111, B00000 };
byte tunedMark[8] = { B00100, B11111, B10101, B10101, B10101, B10101, B11111, B00100 };

// Nazwy dźwięków 
const char* noteNames[] = {"C", "C.", "d", "d.", "E", "F", "F.", "G", "G.", "A", "A.", "b"};
int lastNoteIndex = -1;
int lastSquarePos = -1;
const int centerPos = 8; 

// --- PARAMETRY FFT (Zoptymalizowane dla ESP32) ---
#define SAMPLES       1024      // Zwiększono ze 128 dla dokładniejszego strojenia basów!
#define SAMPLING_FREQ 4000.0    // Obniżono lekko dla lepszej rozdzielczości (krok ~3.9 Hz)
#define ADC_PIN       34        // GPIO34 (Wejście ADC1 w ESP32)
#define NOISE_FLOOR   300.0     // Wyższy próg dla ESP32, gdyż ADC ma rozdzielczość 12-bit (0-4095)

float vReal[SAMPLES];
float vImag[SAMPLES];
const unsigned long samplePeriodUs = (unsigned long)(1000000.0 / SAMPLING_FREQ);

ArduinoFFT<float> FFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);

void setup() {
  Serial.begin(115200);
  
  // Konfiguracja ADC w ESP32 dla pełnego zakresu 0-3.3V
  analogReadResolution(12); // Rozdzielczość 12 bitów
  
  lcd.begin(16, 2);
  lcd.clear();              
  
  lcd.createChar(0, centerMark);
  lcd.createChar(1, hollowSquare);
  lcd.createChar(2, tunedMark);
  
  lcd.setCursor(centerPos, 1);
  lcd.write((uint8_t)0);
}

void loop() {
  // Zebranie próbek z pinu GPIO34
  unsigned long t0 = micros();
  for (int i = 0; i < SAMPLES; i++) {
    vReal[i] = (float)analogRead(ADC_PIN); 
    vImag[i] = 0.0; 

    unsigned long targetTime = t0 + (unsigned long)i * samplePeriodUs;
    while (micros() < targetTime) { /* busy wait */ }
  }

  // Odjęcie składowej stałej (DC offset)
  float mean = 0;
  for (int i = 0; i < SAMPLES; i++) mean += vReal[i];
  mean /= SAMPLES;
  for (int i = 0; i < SAMPLES; i++) vReal[i] -= mean;

  // Analiza FFT
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();
  float peakFreq = FFT.majorPeak();

  // Szukanie maksymalnej amplitudy
  float maxMag = 0;
  for (int i = 2; i < (SAMPLES / 2); i++) {  
    if (vReal[i] > maxMag) maxMag = vReal[i];
  }

  // Wizualizacja na LCD
  if (maxMag > NOISE_FLOOR && peakFreq > 50.0 && peakFreq < 1200.0) {
    
    Serial.print("f0 = "); Serial.print(peakFreq, 1); Serial.print(" Hz | mag = "); Serial.println(maxMag, 0);

    // Konwersja na notację MIDI
    float midiNote = 12 * log(peakFreq / 440.0) / log(2) + 69;
    int noteIndex = (int)round(midiNote) % 12;
    if (noteIndex < 0) noteIndex += 12; 
    
    // Odchylenie w centach
    float cents = (midiNote - round(midiNote)) * 100;
    int currentSquarePos = centerPos + (int)(cents / 6.25);
    currentSquarePos = constrain(currentSquarePos, 0, 15);

    if (noteIndex != lastNoteIndex) {
      lcd.setCursor(0, 0);
      lcd.print("Dzwiek: ");
      lcd.print(noteNames[noteIndex]);
      lcd.print("    "); 
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
    if (lastNoteIndex != -2) {
       lcd.setCursor(0, 0); lcd.print("Dzwiek: ");
       lcd.setCursor(0, 1); lcd.print("                ");
       lcd.setCursor(centerPos, 1); lcd.write((uint8_t)0); 
       lastNoteIndex = -2;
       lastSquarePos = -1;
    }
  }

  delay(50); // Zwiększony lekko delay dla płynniejszego odświeżania na ESP32
}