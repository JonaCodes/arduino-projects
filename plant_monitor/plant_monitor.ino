// ======================================
// ======================================
// PINS
// ======================================
// ======================================
const int PLANT_1_SENSOR_PIN = A5;
const int PLANT_1_LED_PIN = 2;

const int PLANT_2_SENSOR_PIN = A4;
const int PLANT_2_LED_PIN = 4;

const int DISTANCE_SENSOR_TRIGGER_PIN = 8;
const int DISTANCE_SENSOR_ECHO_PIN = 9;

const int PIEZO_PIN = 3;
// ======================================
// ======================================
// PIEZO SOS CONSTS
// ======================================
// ======================================
const int shortDuration = 100;
const int longDuration = 500;
const int pauseDuration = 100;
const int letterPause = 300;

// new
struct PiezoSegment {
  int duration;
  bool play;
};

const PiezoSegment SHORT_NOTE = { 100, true };
const PiezoSegment LONG_NOTE = { 300, true };
const PiezoSegment SEGMENT_PAUSE = { 80, false };
const PiezoSegment LONG_SEGMENT_PAUSE = { 200, false };
const PiezoSegment CYCLE_PAUSE = { 1500, false };

const PiezoSegment sosCycle[] = {
  SHORT_NOTE, SEGMENT_PAUSE,
  SHORT_NOTE, SEGMENT_PAUSE,
  SHORT_NOTE, LONG_SEGMENT_PAUSE,
  LONG_NOTE, SEGMENT_PAUSE,
  LONG_NOTE, SEGMENT_PAUSE,
  SHORT_NOTE, SEGMENT_PAUSE,
  SHORT_NOTE, SEGMENT_PAUSE,
  SHORT_NOTE, CYCLE_PAUSE
};

bool isPiezoSegmentRunning = false;
int currentSegmentIndex = 0;
unsigned long currentSegmentStartTime = 0;

const unsigned long timeToWaitForSound = 600000;  // 10 min
// ======================================
// ======================================

struct Plant {
  int id;
  int ledPin;
  int sensorPin;
  int moistureThreshold;
  bool ledState;
  bool isLowOnWater;
  unsigned long waterLowStartTime;
  unsigned long lastBlinkTime;

  Plant(int plantId, int led, int sensor, int moistureThreshold)
    : id(plantId), ledPin(led), sensorPin(sensor), moistureThreshold(moistureThreshold),
      ledState(false), isLowOnWater(false), waterLowStartTime(0), lastBlinkTime(0) {}
};

Plant plants[] = {
  Plant(1, PLANT_1_LED_PIN, PLANT_1_SENSOR_PIN, 430),
  Plant(2, PLANT_2_LED_PIN, PLANT_2_SENSOR_PIN, 440)
};

int getDistance() {
  long duration;
  int distance;

  digitalWrite(DISTANCE_SENSOR_TRIGGER_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(DISTANCE_SENSOR_TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(DISTANCE_SENSOR_TRIGGER_PIN, LOW);

  duration = pulseIn(DISTANCE_SENSOR_ECHO_PIN, HIGH);
  distance = duration * 0.034 / 2;  // Convert time to distance in cm

  return distance;
}

unsigned long timeSince(unsigned long timestamp) {
  return millis() - timestamp;
}

void blinkLED(Plant& plant, int interval) {
  unsigned long currentMillis = millis();
  if (currentMillis - plant.lastBlinkTime < interval) return;

  plant.ledState = !plant.ledState;
  plant.lastBlinkTime = currentMillis;
  digitalWrite(plant.ledPin, plant.ledState ? HIGH : LOW);
}

void playAsyncSOS() {
  if (!isPiezoSegmentRunning) {
    isPiezoSegmentRunning = true;
    PiezoSegment currentSegment = sosCycle[currentSegmentIndex];
    currentSegmentStartTime = millis();

    currentSegment.play ? tone(PIEZO_PIN, 100) : noTone(PIEZO_PIN);
    return;
  }

  PiezoSegment segment = sosCycle[currentSegmentIndex];
  if (timeSince(currentSegmentStartTime) > segment.duration) {
    isPiezoSegmentRunning = false;
    currentSegmentIndex++;

    if (currentSegmentIndex >= sizeof(sosCycle) / sizeof(sosCycle[0])) {
      resetPiezo();
    }
  }
}

void resetPiezo() {
  isPiezoSegmentRunning = false;
  currentSegmentIndex = 0;
  currentSegmentStartTime = 0;
  noTone(PIEZO_PIN);
}

void resetPlant(Plant& plant) {
  plant.waterLowStartTime = 0;
  plant.isLowOnWater = false;
  plant.ledState = false;
  plant.lastBlinkTime = 0;
  digitalWrite(plant.ledPin, LOW);
}

void handlePlant(Plant& plant) {
  int plantSensorValue = analogRead(plant.sensorPin);
  int distance = getDistance();

  bool isSomeoneClose = distance > 30 && distance < 50;
  bool hasEnoughTimePassedSinceLowWater = plant.waterLowStartTime != 0 && (timeSince(plant.waterLowStartTime) >= timeToWaitForSound);
  plant.isLowOnWater = plantSensorValue > plant.moistureThreshold;

  if (plant.isLowOnWater) {
    blinkLED(plant, 500);
    if (plant.waterLowStartTime == 0) plant.waterLowStartTime = millis();
  } else resetPlant(plant);

  bool isInMiddleOfSosCycle = currentSegmentIndex > 0 && currentSegmentIndex < sizeof(sosCycle) / sizeof(sosCycle[0]);
  if ((isSomeoneClose && hasEnoughTimePassedSinceLowWater) || isInMiddleOfSosCycle) playAsyncSOS();
  if (timeSince(currentSegmentStartTime) > 350 && !isInMiddleOfSosCycle) resetPiezo();
}

void setup() {
  for (Plant plant : plants) {
    pinMode(plant.sensorPin, INPUT);
    pinMode(plant.ledPin, OUTPUT);
  }

  pinMode(DISTANCE_SENSOR_TRIGGER_PIN, OUTPUT);
  pinMode(DISTANCE_SENSOR_ECHO_PIN, INPUT);
  pinMode(PIEZO_PIN, OUTPUT);

  Serial.begin(9600);
}

void loop() {
  for (Plant& plant : plants) { handlePlant(plant); }
}