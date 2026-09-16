/*
 * ============================================================================
 *  MACHINE AUTOMATIQUE D'OXYCOUPAGE LINEAIRE
 *  Arduino Pro Mini (carte industrielle FACEMAG)
 *  VERSION CORRIGEE : DIP-Switch DM2282 verifies depuis la photo
 * ============================================================================
 *  MATERIEL :
 *    - Carte controleur (bleue) : Arduino Pro Mini 5V/16MHz
 *    - Carte extension (verte)  : MCP23017 (I2C, adresse 0x20)
 *    - Driver axe X             : Leadshine DM2282 (48V DC)
 *    - Moteur axe X             : NEMA 42, pas = 1.8deg (200 pas/tour)
 *    - Vis a billes axe X       : PAS = 10 mm/tour
 *    - Axe Y                    : Variateur SCHNEIDER ATV320U22N4C
 *                                 Moteur async 1.1kW / 96 tr/min
 *                                 R1 = Marche Avant (coupe)
 *                                 R2 = Marche Arriere (retour)
 *    - Fins de course           : Capteurs inductifs PNP NO (24V)
 *    - HMI                      : ESP32-S3 + ecran 7" (I2C Master)
 * ============================================================================
 *  REGLAGE DIP-SWITCH DM2282 (PHOTO VERIFIEE) :
 *
 *  --- COURANT MOTEUR (SW1-SW3) ---
 *  Reglez selon le courant nominal de VOTRE NEMA 42 (lire la plaque moteur) :
 *    1.6A RMS -> SW1=ON  SW2=OFF SW3=OFF
 *    2.3A RMS -> SW1=OFF SW2=ON  SW3=OFF
 *    3.2A RMS -> SW1=ON  SW2=ON  SW3=OFF   <-- Courant courant NEMA 42
 *    3.7A RMS -> SW1=OFF SW2=OFF SW3=ON
 *    4.4A RMS -> SW1=ON  SW2=OFF SW3=ON
 *    5.2A RMS -> SW1=OFF SW2=ON  SW3=ON
 *    5.9A RMS -> SW1=ON  SW2=ON  SW3=ON
 *
 *  SW4 (Standstill current) :
 *    OFF = Half Current (economie, moins de chauffe a l'arret)
 *    ON  = Full Current (couple max a l'arret, plus de chauffe)
 *    RECOMMANDE : OFF
 *
 *  --- MICROSTEPPING (SW5-SW8) ---
 *    Pul/rev  | SW5 | SW6 | SW7 | SW8
 *    ---------|-----|-----|-----|-----
 *    Default  | on  | on  | on  | on
 *    400      | off | on  | on  | on
 *    800      | on  | off | on  | on
 *    1600     | off | off | on  | on
 *    3200     | on  | on  | off | on    <-- RECOMMANDE (voir ci-dessous)
 *    6400     | off | on  | off | on
 *    12800    | on  | off | off | on
 *    25600    | off | off | off | on
 *    1000     | on  | on  | on  | off
 *    2000     | off | on  | on  | off
 *    ...
 *
 *  RECOMMANDATION : Reglez sur 3200 pas/tour
 *    SW5=ON, SW6=ON, SW7=OFF, SW8=ON
 *    Resolution = 10mm / 3200 = 0.003125 mm/pas
 *    Pour 1000mm de tôle = 1000 / 0.003125 = 320 000 pas
 *    Le DM2282 accepte 200 kHz max, donc a 3200 pas/mm :
 *      Vitesse max theorique = 200000 / 320 = 625 mm/s (tres rapide !)
 *      En pratique, commencez a 5-20 mm/s
 * ============================================================================
 */

#include <Wire.h>
#include <Adafruit_MCP23017.h>

// =============================================================================
//  CONFIGURATION MECANIQUE
// =============================================================================

#define MICROSTEPPING           16          // 3200 pas/tour (SW5=ON, SW6=ON, SW7=OFF, SW8=ON)
#define PAS_PAR_TOUR            200         // Moteur NEMA 42 (1.8 deg/pas)
#define PAS_TOTAL_TOUR          (PAS_PAR_TOUR * MICROSTEPPING)  // = 3200
#define PAS_VIS_MM              10.0        // VOTRE VIS : 10 mm par tour

// Conversion mm <-> pas
#define MM_EN_PAS(mm)           ((long)((mm) * PAS_TOTAL_TOUR / PAS_VIS_MM))
#define PAS_EN_MM(pas)          ((float)(pas) * PAS_VIS_MM / PAS_TOTAL_TOUR)

// Resolution verifiee : 10mm / 3200 = 0.003125 mm/pas
// Pour une coupe de 2000mm : 2000 / 0.003125 = 640 000 pas
// Le DM2282 supporte 200 kHz, donc temps min = 640000 / 200000 = 3.2 secondes

/*
 * --- VITESSE DEPLACEMENT AXE X ---
 * Avec 3200 pas/tour et 10mm/tour => 320 pas/mm
 *
 * Formule : Vitesse_pas/s = Vitesse_mm/s * 320
 *
 *   5 mm/s   -> 1600 pas/s   (lent, securise pour homing)
 *   10 mm/s  -> 3200 pas/s   (moyen)
 *   20 mm/s  -> 6400 pas/s   (normal)
 *   50 mm/s  -> 16000 pas/s  (rapide)
 *   100 mm/s -> 32000 pas/s  (tres rapide, testez progressivement)
 *
 * Le DM2282 supporte max 200000 pas/s, vous avez une marge enorme.
 * Commencez BAS et augmentez progressivement.
 */
#define VITESSE_HOMING_X        1600        // pas/s (5 mm/s) - retour origine
#define VITESSE_POSITIONNEMENT  6400        // pas/s (20 mm/s) - deplacement entre coupes
#define VITESSE_RAPIDE_X        16000       // pas/s (50 mm/s) - deplacement rapide

// =============================================================================
//  MAPPING CARTE BLEUE - ENTREES (optocoupleurs PC817)
//  Capteurs : INDUCTIFS PNP NO
// =============================================================================
#define PIN_ENTREE1_ESTOP       2   // D2 = INT0  -> Arret d'urgence (NF)
#define PIN_ENTREE2_FCX_MIN     3   // D3           -> FC X minimum
#define PIN_ENTREE3_FCX_MAX     4   // D4           -> FC X maximum
#define PIN_ENTREE4_FCY_MIN     5   // D5           -> FC Y minimum
#define PIN_ENTREE5_FCY_MAX     6   // D6           -> FC Y maximum
#define PIN_ENTREE6_PRESO2      7   // D7           -> Pressostat O2
#define PIN_ENTREE7_PRESGAZ     8   // D8           -> Pressostat gaz
#define PIN_ENTREE8_TOLE        9   // D9           -> Presence tole

// =============================================================================
//  MAPPING CARTE BLEUE - SORTIES (ULN2803 U1, collecteur ouvert)
// =============================================================================
#define PIN_SORTIE1_STEP_X      10  // D10 -> Driver DM2282 PUL+
#define PIN_SORTIE2_DIR_X       11  // D11 -> Driver DM2282 DIR+
#define PIN_SORTIE3_ENA_X       12  // D12 -> Driver DM2282 ENA+ (actif LOW)
#define PIN_SORTIE4_RUN_AVANT   13  // D13 -> Variateur ATV320 R1 (Marche Avant)
#define PIN_SORTIE5_BUZZER      A0  // A0  -> Buzzer 24V
#define PIN_SORTIE6_LED_VERTE   A1  // A1  -> Voyant vert "pret"
#define PIN_SORTIE7_LED_ROUGE   A2  // A2  -> Voyant rouge "alarme"
#define PIN_SORTIE8_EXTRACTEUR  A3  // A3  -> Extracteur fumees

// =============================================================================
//  MAPPING CARTE VERTE - MCP23017 (adresse 0x20)
// =============================================================================
#define MCP_ADDR                0x20

// Port A = Sorties relais (SOR1-SOR8)
#define MCP_SOR1_EV_O2          0   // GPA0 -> SOR1 : Electrovanne O2
#define MCP_SOR2_EV_GAZ         1   // GPA1 -> SOR2 : Electrovanne gaz
#define MCP_SOR3_ALLUMEUR       2   // GPA2 -> SOR3 : Allumeur piezo/HF
#define MCP_SOR4_VOYANT_V       3   // GPA3 -> SOR4 : Voyant vert
#define MCP_SOR5_VOYANT_R       4   // GPA4 -> SOR5 : Voyant rouge
#define MCP_SOR6_RUN_ARRIERE    5   // GPA5 -> SOR6 : Variateur ATV320 R2 (Marche Arriere)
#define MCP_SOR7_RESERVE        6
#define MCP_SOR8_RESERVE        7

// Port B = Entrees (E1-E8)
#define MCP_E1_ALM_DRIVER       8   // GPB0 -> E1 : Alarme driver DM2282
#define MCP_E2_FLAAMME          9   // GPB1 -> E2 : Capteur flamme (option)
#define MCP_E3_RESERVE          10
#define MCP_E4_RESERVE          11
#define MCP_E5_RESERVE          12
#define MCP_E6_RESERVE          13
#define MCP_E7_RESERVE          14
#define MCP_E8_RESERVE          15

// =============================================================================
//  PROTOCOLE I2C
// =============================================================================
#define I2C_ADDR_ARDUINO        0x08
#define TRAME_HEADER            0xAA
#define TRAME_FOOTER            0x55
#define CMD_PARAMETRES          0x03
#define CMD_DEMARRER            0x01
#define CMD_ARRETER             0x02
#define CMD_STATUT              0x04

// =============================================================================
//  ALARMES
// =============================================================================
#define ALARME_NONE             0
#define ALARME_ESTOP            1
#define ALARME_FC_X             2
#define ALARME_FC_Y             3
#define ALARME_PRESSION_O2      4
#define ALARME_PRESSION_GAZ     5
#define ALARME_FLAAMME          6
#define ALARME_DRIVER           7
#define ALARME_ABSENCE_TOLE     8
#define ALARME_TIMEOUT_COUPAGE  9
#define ALARME_COMM_I2C         10

// =============================================================================
//  PARAMETRES DE COUPE
// =============================================================================
#define TEMPS_PRECHAUFFE_BASE   2000
#define TEMPS_PRECHAUFFE_PAR_MM 1000
#define TEMPS_ALLUMAGE_MAX      3000
#define TEMPS_PURGE_POST_COUPE  4000
#define TEMPS_REFROIDISSEMENT   2000
#define DELAI_VERIF_FLAAMME     800
#define TIMEOUT_COUPAGE_MS      120000UL

// =============================================================================
//  VARIABLES GLOBALES
// =============================================================================
Adafruit_MCP23017 mcp;

volatile uint16_t consigneLargeurMm = 0;
volatile uint16_t consigneQuantite = 0;
volatile uint8_t  consigneEpaisseur = 0;
volatile bool     flagNouvelleConsigne = false;
volatile bool     flagDemarrer = false;
volatile bool     flagArreter = false;

enum EtatMachine {
    ETAT_INIT, ETAT_ATTENTE, ETAT_VERIFICATION, ETAT_HOMING_X,
    ETAT_POSITIONNEMENT_X, ETAT_ALLUMAGE, ETAT_PRECHAUFFE,
    ETAT_COUPAGE, ETAT_FIN_COUPAGE, ETAT_RETOUR_Y, ETAT_TERMINE,
    ETAT_ALARME, ETAT_ARRET_URGENCE
};
volatile EtatMachine etatMachine = ETAT_INIT;

volatile long positionX_Pas = 0;
volatile long cibleX_Pas = 0;
volatile uint16_t numCoupe = 0;
volatile uint8_t alarmeCode = ALARME_NONE;

unsigned long msNow = 0;
unsigned long msAllumage = 0;
unsigned long msPrechauffage = 0;
unsigned long msPurge = 0;
unsigned long msRefroidissement = 0;
unsigned long msStep = 0;
unsigned long msBlink = 0;
unsigned long msBuzzer = 0;
unsigned long msTimeoutCoupe = 0;
unsigned long msRetourY = 0;

volatile bool flagEStop = false;
bool flagBuzzerOn = false;
bool ledEtat = false;
bool homingTermine = false;

uint8_t bufRx[16];
uint8_t bufTx[8];
volatile uint8_t rxLen = 0;

// =============================================================================
//  FONCTIONS DRIVER DM2282
// =============================================================================
inline void stepPulse() {
    digitalWrite(PIN_SORTIE1_STEP_X, HIGH);
    delayMicroseconds(8);
    digitalWrite(PIN_SORTIE1_STEP_X, LOW);
    delayMicroseconds(8);
}

inline void driverEnable()  { digitalWrite(PIN_SORTIE3_ENA_X, LOW); }
inline void driverDisable() { digitalWrite(PIN_SORTIE3_ENA_X, HIGH); }

inline void setDirX(bool positif) {
    digitalWrite(PIN_SORTIE2_DIR_X, positif ? HIGH : LOW);
    delayMicroseconds(10);
}

// =============================================================================
//  FONCTIONS TORCHE & VARIATEUR
// =============================================================================
// IMPORTANT : Testez si vos relais sont active HIGH ou active LOW
// Si le relais s'active quand vous envoyez LOW, inversez HIGH/LOW ci-dessous

inline void torcheGazOn()    { mcp.digitalWrite(MCP_SOR2_EV_GAZ, HIGH); }
inline void torcheGazOff()   { mcp.digitalWrite(MCP_SOR2_EV_GAZ, LOW); }
inline void torcheO2On()     { mcp.digitalWrite(MCP_SOR1_EV_O2, HIGH); }
inline void torcheO2Off()    { mcp.digitalWrite(MCP_SOR1_EV_O2, LOW); }
inline void allumeurOn()     { mcp.digitalWrite(MCP_SOR3_ALLUMEUR, HIGH); }
inline void allumeurOff()    { mcp.digitalWrite(MCP_SOR3_ALLUMEUR, LOW); }
inline void torcheToutOff()  { torcheO2Off(); torcheGazOff(); allumeurOff(); }

// Variateur ATV320 : R1 = Marche Avant, R2 = Marche Arriere
inline void vfdAvantOn()     { digitalWrite(PIN_SORTIE4_RUN_AVANT, HIGH); }
inline void vfdAvantOff()    { digitalWrite(PIN_SORTIE4_RUN_AVANT, LOW); }
inline void vfdArriereOn()   { mcp.digitalWrite(MCP_SOR6_RUN_ARRIERE, HIGH); }
inline void vfdArriereOff()  { mcp.digitalWrite(MCP_SOR6_RUN_ARRIERE, LOW); }
inline void vfdStop()        { vfdAvantOff(); vfdArriereOff(); }

inline void extracteurOn()   { digitalWrite(PIN_SORTIE8_EXTRACTEUR, HIGH); }
inline void extracteurOff()  { digitalWrite(PIN_SORTIE8_EXTRACTEUR, LOW); }

// =============================================================================
//  SECURITES
// =============================================================================
bool verifierSecurites() {
    if (digitalRead(PIN_ENTREE1_ESTOP) == LOW) {
        alarmeCode = ALARME_ESTOP; return false;
    }
    if (digitalRead(PIN_ENTREE6_PRESO2) == LOW) {
        alarmeCode = ALARME_PRESSION_O2; return false;
    }
    if (digitalRead(PIN_ENTREE7_PRESGAZ) == LOW) {
        alarmeCode = ALARME_PRESSION_GAZ; return false;
    }
    if (digitalRead(PIN_ENTREE8_TOLE) == LOW) {
        alarmeCode = ALARME_ABSENCE_TOLE; return false;
    }
    return true;
}

// =============================================================================
//  DEPLACEMENT X NON BLOQUANT
// =============================================================================
bool deplacerX_NonBloquant(long cible, unsigned int vitessePps) {
    static long pasRestants = 0;
    static int sens = 0;
    static unsigned long periodeUs = 0;
    static bool enCours = false;

    if (!enCours) {
        pasRestants = cible - positionX_Pas;
        if (pasRestants == 0) return true;
        sens = (pasRestants > 0) ? 1 : -1;
        setDirX(sens > 0);
        periodeUs = 1000000UL / vitessePps;
        enCours = true;
        driverEnable();
        delayMicroseconds(20);
    }

    unsigned long now = micros();
    if (now - msStep >= periodeUs) {
        msStep = now;

        if (sens > 0 && digitalRead(PIN_ENTREE3_FCX_MAX) == LOW) {
            enCours = false; driverDisable();
            alarmeCode = ALARME_FC_X; etatMachine = ETAT_ALARME; return false;
        }
        if (sens < 0 && digitalRead(PIN_ENTREE2_FCX_MIN) == LOW) {
            enCours = false; driverDisable();
            alarmeCode = ALARME_FC_X; etatMachine = ETAT_ALARME; return false;
        }

        stepPulse();
        positionX_Pas += sens;
        pasRestants -= sens;

        if (pasRestants == 0) {
            enCours = false; driverDisable(); return true;
        }
    }
    return false;
}

// =============================================================================
//  HOMING X
// =============================================================================
bool homingX_NonBloquant() {
    static uint8_t phase = 0;
    static unsigned long msDebutRecul = 0;

    switch (phase) {
        case 0:
            setDirX(false);
            driverEnable();
            if (digitalRead(PIN_ENTREE2_FCX_MIN) == LOW) {
                phase = 1;
                msDebutRecul = millis();
            } else {
                if (micros() - msStep >= 625) {  // 1600 pas/s = 5mm/s
                    msStep = micros();
                    stepPulse(); positionX_Pas--;
                }
            }
            return false;

        case 1:
            if (millis() - msDebutRecul < 200) return false;
            setDirX(true);
            if (digitalRead(PIN_ENTREE2_FCX_MIN) == HIGH) {
                positionX_Pas = 0;
                phase = 0;
                driverDisable();
                return true;
            }
            if (micros() - msStep >= 5000) {  // Lent : 200 pas/s
                msStep = micros();
                stepPulse(); positionX_Pas++;
            }
            return false;
    }
    return false;
}

// =============================================================================
//  BUZZER & LEDS
// =============================================================================
void buzzerDeclencher(unsigned long dureeMs) {
    digitalWrite(PIN_SORTIE5_BUZZER, HIGH);
    msBuzzer = millis();
    flagBuzzerOn = true;
}

void buzzerGerer() {
    if (flagBuzzerOn && (millis() - msBuzzer >= 500)) {
        digitalWrite(PIN_SORTIE5_BUZZER, LOW);
        flagBuzzerOn = false;
    }
}

// =============================================================================
//  I2C CALLBACKS
// =============================================================================
void receiveEvent(int bytes) {
    rxLen = 0;
    while (Wire.available() && rxLen < 16) bufRx[rxLen++] = Wire.read();
    if (rxLen >= 8 && bufRx[0] == TRAME_HEADER && bufRx[7] == TRAME_FOOTER) {
        uint8_t cmd = bufRx[1];
        switch (cmd) {
            case CMD_PARAMETRES:
                consigneLargeurMm = ((uint16_t)bufRx[2] << 8) | bufRx[3];
                consigneQuantite  = ((uint16_t)bufRx[4] << 8) | bufRx[5];
                consigneEpaisseur = bufRx[6];
                flagNouvelleConsigne = true;
                break;
            case CMD_DEMARRER: flagDemarrer = true; break;
            case CMD_ARRETER:  flagArreter = true; break;
        }
    }
}

void requestEvent() {
    bufTx[0] = TRAME_HEADER;
    bufTx[1] = CMD_STATUT;
    bufTx[2] = (uint8_t)etatMachine;
    bufTx[3] = alarmeCode;
    bufTx[4] = (uint8_t)(numCoupe >> 8);
    bufTx[5] = (uint8_t)(numCoupe & 0xFF);
    bufTx[6] = (uint8_t)(positionX_Pas >> 8);
    bufTx[7] = TRAME_FOOTER;
    Wire.write(bufTx, 8);
}

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
    Serial.begin(115200);
    Serial.println(F("\n=== OXYCOUPAGE - FACEMAG v3 ==="));
    Serial.print(F("Microstepping: ")); Serial.print(MICROSTEPPING);
    Serial.print(F(" -> ")); Serial.print(PAS_TOTAL_TOUR); Serial.println(F(" pas/tour"));
    Serial.print(F("Resolution: ")); Serial.print(PAS_EN_MM(1), 4); Serial.println(F(" mm/pas"));
    Serial.print(F("Pas/mm: ")); Serial.println(PAS_TOTAL_TOUR / PAS_VIS_MM);

    // Entrees
    pinMode(PIN_ENTREE1_ESTOP, INPUT_PULLUP);
    pinMode(PIN_ENTREE2_FCX_MIN, INPUT_PULLUP);
    pinMode(PIN_ENTREE3_FCX_MAX, INPUT_PULLUP);
    pinMode(PIN_ENTREE4_FCY_MIN, INPUT_PULLUP);
    pinMode(PIN_ENTREE5_FCY_MAX, INPUT_PULLUP);
    pinMode(PIN_ENTREE6_PRESO2, INPUT_PULLUP);
    pinMode(PIN_ENTREE7_PRESGAZ, INPUT_PULLUP);
    pinMode(PIN_ENTREE8_TOLE, INPUT_PULLUP);

    // Sorties
    pinMode(PIN_SORTIE1_STEP_X, OUTPUT);      digitalWrite(PIN_SORTIE1_STEP_X, LOW);
    pinMode(PIN_SORTIE2_DIR_X, OUTPUT);         digitalWrite(PIN_SORTIE2_DIR_X, LOW);
    pinMode(PIN_SORTIE3_ENA_X, OUTPUT);         digitalWrite(PIN_SORTIE3_ENA_X, HIGH);
    pinMode(PIN_SORTIE4_RUN_AVANT, OUTPUT);     digitalWrite(PIN_SORTIE4_RUN_AVANT, LOW);
    pinMode(PIN_SORTIE5_BUZZER, OUTPUT);         digitalWrite(PIN_SORTIE5_BUZZER, LOW);
    pinMode(PIN_SORTIE6_LED_VERTE, OUTPUT);     digitalWrite(PIN_SORTIE6_LED_VERTE, LOW);
    pinMode(PIN_SORTIE7_LED_ROUGE, OUTPUT);     digitalWrite(PIN_SORTIE7_LED_ROUGE, LOW);
    pinMode(PIN_SORTIE8_EXTRACTEUR, OUTPUT);    digitalWrite(PIN_SORTIE8_EXTRACTEUR, LOW);

    // MCP23017
    if (!mcp.begin(MCP_ADDR)) {
        Serial.println(F("ERREUR MCP23017 !"));
        alarmeCode = ALARME_COMM_I2C;
        etatMachine = ETAT_ALARME;
    } else {
        for (int i = 0; i < 8; i++) {
            mcp.pinMode(i, OUTPUT);
            mcp.digitalWrite(i, LOW);
        }
        for (int i = 8; i < 16; i++) {
            mcp.pinMode(i, INPUT);
            mcp.pullUp(i, HIGH);
        }
        Serial.println(F("MCP23017 OK"));
    }

    // I2C Slave
    Wire.begin(I2C_ADDR_ARDUINO);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);
    Serial.println(F("I2C OK (0x08)"));

    // Interruption E-Stop
    attachInterrupt(digitalPinToInterrupt(PIN_ENTREE1_ESTOP), []() {
        static unsigned long last = 0;
        unsigned long n = millis();
        if (n - last > 50) { last = n; flagEStop = true; }
    }, FALLING);

    delay(300);
    buzzerDeclencher(300);
    Serial.println(F("Pret. Etat: ATTENTE"));
    etatMachine = ETAT_ATTENTE;
}

// =============================================================================
//  LOOP - MACHINE A ETATS
// =============================================================================
void loop() {
    msNow = millis();
    buzzerGerer();

    if (msNow - msBlink >= 600) {
        msBlink = msNow;
        ledEtat = !ledEtat;
        digitalWrite(PIN_SORTIE6_LED_VERTE, (etatMachine == ETAT_ATTENTE) ? ledEtat : LOW);
    }

    if (flagEStop) { flagEStop = false; etatMachine = ETAT_ARRET_URGENCE; }
    digitalWrite(PIN_SORTIE7_LED_ROUGE, (alarmeCode != ALARME_NONE) ? HIGH : LOW);

    switch (etatMachine) {

        case ETAT_INIT:
            etatMachine = ETAT_ATTENTE;
            break;

        case ETAT_ATTENTE:
            torcheToutOff();
            vfdStop();
            driverDisable();
            extracteurOff();

            if (flagNouvelleConsigne) {
                Serial.print(F("Consigne: L=")); Serial.print(consigneLargeurMm);
                Serial.print(F("mm Qty=")); Serial.println(consigneQuantite);
                flagNouvelleConsigne = false;
                buzzerDeclencher(200);
            }
            if (flagDemarrer && consigneQuantite > 0 && consigneLargeurMm > 0) {
                flagDemarrer = false;
                numCoupe = 0;
                homingTermine = false;
                etatMachine = ETAT_VERIFICATION;
                Serial.println(F("DEMARRAGE"));
            }
            if (flagArreter) flagArreter = false;
            break;

        case ETAT_VERIFICATION:
            Serial.println(F("Verif securites..."));
            if (!verifierSecurites()) {
                etatMachine = ETAT_ALARME;
                Serial.print(F("ALARME:")); Serial.println(alarmeCode);
                break;
            }
            extracteurOn();
            buzzerDeclencher(400);
            etatMachine = ETAT_HOMING_X;
            break;

        case ETAT_HOMING_X:
            if (!homingTermine) {
                if (homingX_NonBloquant()) {
                    homingTermine = true;
                    Serial.println(F("Origine X OK"));
                    etatMachine = ETAT_POSITIONNEMENT_X;
                }
            }
            break;

        case ETAT_POSITIONNEMENT_X:
            {
                long decalage = MM_EN_PAS((long)consigneLargeurMm * numCoupe);
                cibleX_Pas = decalage;
                Serial.print(F("Deplacement X "));
                Serial.print(numCoupe + 1); Serial.print(F("/")); Serial.println(consigneQuantite);

                if (deplacerX_NonBloquant(cibleX_Pas, VITESSE_POSITIONNEMENT)) {
                    Serial.println(F("Position X OK"));
                    etatMachine = ETAT_ALLUMAGE;
                }
            }
            break;

        case ETAT_ALLUMAGE:
            {
                static uint8_t ph = 0;
                switch (ph) {
                    case 0:
                        Serial.println(F("Allumage: gaz ON"));
                        torcheGazOn();
                        msAllumage = msNow;
                        ph = 1;
                        break;
                    case 1:
                        if (msNow - msAllumage >= 600) {
                            Serial.println(F("Allumage: piezo ON"));
                            allumeurOn();
                            msAllumage = msNow;
                            ph = 2;
                        }
                        break;
                    case 2:
                        if (msNow - msAllumage >= TEMPS_ALLUMAGE_MAX) {
                            allumeurOff();
                            if (mcp.digitalRead(MCP_E2_FLAAMME) == LOW) {
                                Serial.println(F("Flamme OK"));
                                ph = 0;
                                etatMachine = ETAT_PRECHAUFFE;
                                msPrechauffage = msNow;
                            } else {
                                Serial.println(F("ERREUR flamme !"));
                                alarmeCode = ALARME_FLAAMME;
                                torcheToutOff();
                                ph = 0;
                                etatMachine = ETAT_ALARME;
                            }
                        } else if (msNow - msAllumage >= DELAI_VERIF_FLAAMME) {
                            if (mcp.digitalRead(MCP_E2_FLAAMME) == HIGH) {
                                allumeurOff();
                            }
                        }
                        break;
                }
            }
            break;

        case ETAT_PRECHAUFFE:
            {
                unsigned long t = TEMPS_PRECHAUFFE_BASE + (unsigned long)consigneEpaisseur * TEMPS_PRECHAUFFE_PAR_MM;
                if (t > 15000) t = 15000;
                if (msNow - msPrechauffage >= t) {
                    Serial.println(F("Prechauffage OK -> O2 ON"));
                    torcheO2On();
                    delay(150);
                    etatMachine = ETAT_COUPAGE;
                    msTimeoutCoupe = msNow;
                }
            }
            break;

        case ETAT_COUPAGE:
            {
                static bool coupeEnCours = false;
                if (!coupeEnCours) {
                    Serial.println(F("=== COUPE ==="));
                    vfdAvantOn();
                    coupeEnCours = true;
                }
                if (!verifierSecurites()) {
                    vfdStop();
                    torcheToutOff();
                    coupeEnCours = false;
                    etatMachine = ETAT_ALARME;
                    break;
                }
                if (digitalRead(PIN_ENTREE5_FCY_MAX) == LOW) {
                    Serial.println(F("FC Y max atteint"));
                    vfdStop();
                    coupeEnCours = false;
                    etatMachine = ETAT_FIN_COUPAGE;
                    msPurge = msNow;
                }
                if (msNow - msTimeoutCoupe > TIMEOUT_COUPAGE_MS) {
                    vfdStop();
                    torcheToutOff();
                    coupeEnCours = false;
                    alarmeCode = ALARME_TIMEOUT_COUPAGE;
                    etatMachine = ETAT_ALARME;
                }
            }
            break;

        case ETAT_FIN_COUPAGE:
            {
                static bool phasePurge = false;
                torcheO2Off();
                if (!phasePurge && msNow - msPurge >= 2000) {
                    torcheGazOff();
                    phasePurge = true;
                }
                if (msNow - msPurge >= TEMPS_PURGE_POST_COUPE) {
                    phasePurge = false;
                    numCoupe++;
                    if (numCoupe >= consigneQuantite) {
                        Serial.println(F("TERMINE"));
                        extracteurOff();
                        etatMachine = ETAT_TERMINE;
                        buzzerDeclencher(1500);
                    } else {
                        etatMachine = ETAT_RETOUR_Y;
                        msRetourY = msNow;
                    }
                }
            }
            break;

        case ETAT_RETOUR_Y:
            {
                static bool retourEnCours = false;
                if (!retourEnCours) {
                    Serial.println(F("Retour Y arriere..."));
                    vfdArriereOn();
                    retourEnCours = true;
                }
                if (digitalRead(PIN_ENTREE4_FCY_MIN) == LOW) {
                    vfdArriereOff();
                    retourEnCours = false;
                    if (msNow - msRetourY >= TEMPS_REFROIDISSEMENT) {
                        etatMachine = ETAT_HOMING_X;
                        homingTermine = false;
                    }
                }
                if (msNow - msRetourY > 30000UL) {
                    vfdArriereOff();
                    retourEnCours = false;
                    alarmeCode = ALARME_FC_Y;
                    etatMachine = ETAT_ALARME;
                }
            }
            break;

        case ETAT_TERMINE:
            torcheToutOff();
            vfdStop();
            driverDisable();
            extracteurOff();
            if (flagArreter) {
                flagArreter = false;
                etatMachine = ETAT_ATTENTE;
            }
            break;

        case ETAT_ALARME:
            {
                static bool ack = false;
                torcheToutOff();
                vfdStop();
                driverDisable();
                extracteurOff();
                if (!ack) {
                    Serial.print(F("ALARME ")); Serial.println(alarmeCode);
                    buzzerDeclencher(2000);
                    ack = true;
                }
                digitalWrite(PIN_SORTIE5_BUZZER, (msNow % 300 < 150) ? HIGH : LOW);
                if (flagArreter) {
                    flagArreter = false;
                    alarmeCode = ALARME_NONE;
                    ack = false;
                    digitalWrite(PIN_SORTIE5_BUZZER, LOW);
                    etatMachine = ETAT_ATTENTE;
                }
            }
            break;

        case ETAT_ARRET_URGENCE:
            {
                static bool estopAck = false;
                torcheToutOff();
                vfdStop();
                driverDisable();
                extracteurOff();
                if (!estopAck) {
                    Serial.println(F("!!! E-STOP !!!"));
                    estopAck = true;
                }
                digitalWrite(PIN_SORTIE5_BUZZER, HIGH);
                digitalWrite(PIN_SORTIE7_LED_ROUGE, HIGH);
                if (digitalRead(PIN_ENTREE1_ESTOP) == HIGH && flagArreter) {
                    flagArreter = false;
                    flagEStop = false;
                    estopAck = false;
                    alarmeCode = ALARME_NONE;
                    digitalWrite(PIN_SORTIE5_BUZZER, LOW);
                    digitalWrite(PIN_SORTIE7_LED_ROUGE, LOW);
                    etatMachine = ETAT_ATTENTE;
                }
            }
            break;
    }
    delayMicroseconds(50);
}
