#include <Acoustic_Based_Predictive_Maintainance_inferencing.h>
#include <PDM.h>

#define SAMPLE_BUFFER_SIZE (EI_CLASSIFIER_RAW_SAMPLE_COUNT)
int16_t sample_buffer[SAMPLE_BUFFER_SIZE];
int sample_buffer_index = 0;

volatile bool inference_running = false;

#define LED_RED LEDR
#define LED_GREEN LEDG
#define LED_BLUE LEDB

void setLedColor(int red, int green, int blue) {
  digitalWrite(LED_RED, red ? LOW : HIGH);
  digitalWrite(LED_GREEN, green ? LOW : HIGH);
  digitalWrite(LED_BLUE, blue ? LOW : HIGH);
}


short sampleBuffer[2048];
volatile int samplesRead;


void onPDMdata() {

  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2; 
  

  for (int i = 0; i < samplesRead && sample_buffer_index < SAMPLE_BUFFER_SIZE; i++) {
    sample_buffer[sample_buffer_index++] = sampleBuffer[i];
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Acoustic-Based Predictive Maintenance");
  
  PDM.onReceive(onPDMdata);
  if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }
  
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  
  setLedColor(0, 0, 0);
  
  for(int i = 0; i < 3; i++) {
    setLedColor(1, 1, 1);
    delay(100);
    setLedColor(0, 0, 0);
    delay(100);
  }
  
  Serial.println("Setup complete. Starting audio monitoring...");
  microphone_inference_start();
}

void microphone_inference_start() {
  sample_buffer_index = 0;
  inference_running = false;
  Serial.println("Buffer reset. Ready for new audio.");
}

bool microphone_inference_record() {
  inference_running = true;
  
  unsigned long startTime = millis();
  const unsigned long timeout = 5000;
  Serial.print("Collecting audio samples: ");
  

  while (sample_buffer_index < SAMPLE_BUFFER_SIZE) {
    delay(100);

    if (millis() % 1000 < 100) {
      Serial.print(sample_buffer_index);
      Serial.print("/");
      Serial.print(SAMPLE_BUFFER_SIZE);
      Serial.print(" ");
    }

    if (millis() - startTime > timeout) {
      Serial.println("\nTimeout waiting for audio buffer to fill!");
      inference_running = false;
      return false;
    }
  }
  
  Serial.println("\nAudio buffer filled successfully.");
  inference_running = false;
  return true;
}

int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
  if (offset + length > SAMPLE_BUFFER_SIZE) {
    return -1;
  }
  
  for (size_t i = 0; i < length; i++) {
    out_ptr[i] = sample_buffer[offset + i] / 32768.0f;
  }
  
  return 0;
}

void loop() {
  Serial.println("Starting audio capture...");
  
  if (!microphone_inference_record()) {
    Serial.println("Failed to capture audio. Restarting...");
    microphone_inference_start();
    delay(1000);
    return;
  }
  
  Serial.println("Processing captured audio...");
  
  ei::signal_t signal;
  signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
  signal.get_data = &microphone_audio_signal_get_data;
  
  ei_impulse_result_t result;
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
  if (err != EI_IMPULSE_OK) {
    Serial.println("ERR: Failed to run classifier");
    return;
  }
  
  Serial.println("Classification results:");
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    Serial.print("  ");
    Serial.print(result.classification[ix].label);
    Serial.print(": ");
    Serial.println(result.classification[ix].value);
  }
  
  float highest_value = 0;
  int highest_index = -1;
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    if (result.classification[ix].value > highest_value) {
      highest_value = result.classification[ix].value;
      highest_index = ix;
    }
  }
  
  setLedColor(0, 0, 0);
  
  const char* detected_class = result.classification[highest_index].label;
  Serial.print("Detected class: ");
  Serial.print(detected_class);
  Serial.print(" (confidence: ");
  Serial.print(highest_value);
  Serial.println(")");
  
  if (strcmp(detected_class, "Normal Mode") == 0) {
    setLedColor(0, 1, 0);
    Serial.println("Status: Normal operation");
  } 
  else if (strcmp(detected_class, "Crowd Noise") == 0) {
    setLedColor(0, 0, 1);
    Serial.println("Status: Crowd noise detected");
  }
  else if (strcmp(detected_class, "Defect1") == 0) {
    setLedColor(1, 0, 0);
    Serial.println("Status: Defect type 1 detected!");
  }
  else if (strcmp(detected_class, "Defect2") == 0) {
    setLedColor(1, 0, 1);
    Serial.println("Status: Defect type 2 detected!");
  }
  
  Serial.println("---------------------------------");
  delay(500);
  
  microphone_inference_start();
}