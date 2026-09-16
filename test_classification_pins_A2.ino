/*
  ==========================================================
  SCRIPT V2 - Classification des pins (carte A2)
  Arduino Pro Mini
  ==========================================================

  Contrairement a la V1 (INPUT_PULLUP), ce script lit chaque
  pin candidat en mode INPUT flottant (pas de pull-up interne)
  et fait plusieurs lectures rapides pour voir si le signal
  est stable ou bruite.

  Interpretation :
    - "STABLE LOW"  -> le pin est charge par un composant
                        (probablement l'entree du ULN2803A,
                        donc c'est un pin de SORTIE, pas
                        une ENTREE)
    - "STABLE HIGH" -> le pin est probablement une vraie
                        ENTREE au repos (le module opto a
                        souvent sa propre resistance pull-up)
    - "BRUITE"      -> le pin ne semble connecte a rien de
                        precis (flottant, a ignorer ou
                        verifier a part)

  COMMANDES (Moniteur Serie, 115200 bauds) :
    C  -> lance la classification de tous les pins candidats
    I  -> mode surveillance continue (comme avant, mais sans
          pull-up, donc plus fiable pour les vrais pins ENTREE
          detectes comme "STABLE HIGH")
    X  -> stop
*/

const uint8_t candidatePins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A0, A1, A2, A3, A4, A5, A6, A7};
const uint8_t nbPins = sizeof(candidatePins) / sizeof(candidatePins[0]);
const uint8_t NB_SAMPLES = 20;

enum Mode { IDLE, TEST_INPUTS };
Mode currentMode = IDLE;

void setup() {
  Serial.begin(115200);
  delay(300);
  printMenu();
}

void loop() {
  if (Serial.available()) {
    char c = toupper(Serial.read());
    if (c == 'C') {
      classifyPins();
    } else if (c == 'I') {
      startMonitor();
    } else if (c == 'X') {
      stopTest();
    }
  }

  if (currentMode == TEST_INPUTS) {
    monitorOnce();
  }
}

void printMenu() {
  Serial.println();
  Serial.println(F("=== Classification pins carte A2 (V2) ==="));
  Serial.println(F("C = classifier tous les pins (stable HIGH / stable LOW / bruite)"));
  Serial.println(F("I = surveillance continue (actionne les ENTREEx)"));
  Serial.println(F("X = stop"));
  Serial.println();
}

void stopTest() {
  for (uint8_t i = 0; i < nbPins; i++) pinMode(candidatePins[i], INPUT);
  currentMode = IDLE;
  Serial.println(F(">> Arrete."));
  printMenu();
}

// ---------------- CLASSIFICATION ----------------
void classifyPins() {
  Serial.println(F(">> Classification en cours (ne touche a rien pendant ce test)..."));
  Serial.println();

  for (uint8_t i = 0; i < nbPins; i++) {
    uint8_t pin = candidatePins[i];
    pinMode(pin, INPUT); // flottant, PAS de pull-up

    uint8_t highCount = 0;
    for (uint8_t s = 0; s < NB_SAMPLES; s++) {
      if (digitalRead(pin) == HIGH) highCount++;
      delay(5);
    }

    Serial.print(F("Pin D"));
    printPinName(pin);
    Serial.print(F(" : "));

    if (highCount == NB_SAMPLES) {
      Serial.println(F("STABLE HIGH  <-- candidat ENTREE au repos"));
    } else if (highCount == 0) {
      Serial.println(F("STABLE LOW   <-- probablement charge (ULN2803A / pin de SORTIE)"));
    } else {
      Serial.print(F("BRUITE ("));
      Serial.print(highCount);
      Serial.print('/');
      Serial.print(NB_SAMPLES);
      Serial.println(F(" HIGH)  <-- pin flottant, rien de connecte de precis"));
    }
  }

  Serial.println();
  Serial.println(F(">> Classification terminee. Teste ensuite 'I' sur les pins STABLE HIGH en actionnant tes ENTREEx."));
  printMenu();
}

// ---------------- SURVEILLANCE ----------------
bool lastState[20];
bool firstRead = true;

void startMonitor() {
  currentMode = TEST_INPUTS;
  for (uint8_t i = 0; i < nbPins; i++) pinMode(candidatePins[i], INPUT); // flottant
  firstRead = true;
  Serial.println(F(">> Surveillance demarree (sans pull-up). Actionne ENTREE1..8 une par une."));
  Serial.println(F(">> Seuls les pins STABLE HIGH detectes par 'C' sont pertinents ici."));
}

void monitorOnce() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 50) return;
  lastCheck = millis();

  for (uint8_t i = 0; i < nbPins; i++) {
    bool raw = digitalRead(candidatePins[i]);

    if (firstRead || raw != lastState[i]) {
      Serial.print(F("Pin D"));
      printPinName(candidatePins[i]);
      Serial.print(F(" : "));
      Serial.println(raw ? F("HIGH") : F("LOW  <-- changement detecte"));
    }
    lastState[i] = raw;
  }
  firstRead = false;
}

void printPinName(uint8_t pin) {
  if (pin >= A0) {
    Serial.print('A');
    Serial.print(pin - A0);
  } else {
    Serial.print(pin);
  }
}
