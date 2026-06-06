#include "mbed.h"
#include <DebouncedInput.h>
#include <ServoOutput.h>
#include <UartLogger.h>

using namespace std::chrono;

// Le firmware est pense comme un petit automate:
// 1) on ecoute les capteurs (switch, jack, bouton)
// 2) on decide l'etat systeme
// 3) on agit (servo, LEDs, messages UART)

// Pin mapping based on the provided schematic labels.
// Adjust these constants when the PCB routing is finalized.
static constexpr PinName PIN_SW_CALCULO = D2;
// PIN_SW_EXPERIENCE (D3) est reutilisee temporairement comme BP_SERVO
// car le bouton BP_SERVO n'a pas ete cable sur la carte v1.
// A corriger lors du prochain routage PCB (voir README).
static constexpr PinName PIN_SW_EXPERIENCE = D3; // reserve pour la carte experience (v2)
static constexpr PinName PIN_JACK_TRG = D5;
static constexpr PinName PIN_BP_SERVO = D3;      // WORKAROUND v1: partage la pin SW_EXPERIENCE
static constexpr PinName PIN_LED_EXP = D6;
static constexpr PinName PIN_LED_JACK = D7;
static constexpr PinName PIN_SERVO = D9;

static constexpr bool INPUT_ACTIVE_LOW = true;
static constexpr uint32_t DEBOUNCE_MS = 30;
static constexpr uint32_t LOOP_PERIOD_MS = 5;
static constexpr uint32_t DELAI_SERVO_MS = 5000;
static constexpr uint32_t BLINK_MS = 300;

static constexpr int SERVO_REST_DEG = 0; // Position repos
static constexpr int SERVO_OPEN_DEG = 90; // Position ouverte  (Valeurs à confirmer lors de l'intégration)
static constexpr int SERVO_MIN_PULSE_US = 1000;
static constexpr int SERVO_MAX_PULSE_US = 2000;

enum class SystemState {
    // Systeme coupe: tout est a l'arret.
    ARRET,
    // Systeme arme mais sans action active.
    ATTENTE,
    // Mode CALCULO selectionne.
    MODE_CALCULO,
    // Mode EXPERIENCE selectionne.
    MODE_EXPERIENCE,
    // Temporisation en cours avant ouverture auto.
    TIMER_EN_COURS,
    // Position servo ouverte.
    SERVO_OUVERT,
    // Position servo fermee / repos.
    SERVO_FERME
};

enum class ActiveMode {
    NONE,
    CALCULO,
    EXPERIENCE
};

int main() {
    // UART = console de diagnostic pour comprendre ce que fait l'automate.
    UartLogger uart(USBTX, USBRX, 115200);

    // Chaque entree passe par un filtre anti-rebond logiciel.
    DebouncedInput swCalculo(PIN_SW_CALCULO, INPUT_ACTIVE_LOW, DEBOUNCE_MS);
    // DebouncedInput swExperience(PIN_SW_EXPERIENCE, INPUT_ACTIVE_LOW, DEBOUNCE_MS);
    // swExperience desactive en v1: D3 est reutilisee comme BP_SERVO (workaround).
    // A restaurer quand la carte experience sera couplee.
    DebouncedInput jackTrig(PIN_JACK_TRG, INPUT_ACTIVE_LOW, DEBOUNCE_MS);
    DebouncedInput bpServo(PIN_BP_SERVO, INPUT_ACTIVE_LOW, DEBOUNCE_MS);

    // Actionneurs physiques.
    DigitalOut ledExp(PIN_LED_EXP, 0);
    DigitalOut ledJack(PIN_LED_JACK, 0);
    ServoOutput servo(PIN_SERVO, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US, SERVO_REST_DEG);

    // Memoire de l'automate.
    SystemState state = SystemState::ARRET;
    ActiveMode mode = ActiveMode::NONE;

    bool servoOpen = false;
    bool timerRunning = false;
    uint32_t timerStartMs = 0;

    bool ledExpBlink = false;
    bool ledJackBlink = false;
    uint32_t lastBlinkMs = 0;
    bool blinkPhase = false;

    bool lastJackPresent = false;

    uart.info("[INFO] Boot system");
    // Test LEDs au demarrage (spec §13 etape 4).
    uart.info("[INFO] LED test...");
    ledExp = 1; // Allume les 2 LEDs pour verifier leur bon fonctionnement.
    ledJack = 1;
    ThisThread::sleep_for(milliseconds(500));
    ledExp = 0;
    ledJack = 0;
    uart.info("[INFO] LED test OK");
    servo.writeAngle(SERVO_REST_DEG);
    uart.info("[INFO] Servo closed");
    state = SystemState::ATTENTE;

    while (true) {
        // --- Etape 1: acquisition des entrees ---
        const auto nowTp = Kernel::Clock::now();
        const uint32_t now = duration_cast<milliseconds>(nowTp.time_since_epoch()).count();

        swCalculo.update(nowTp);
        // swExperience.update(nowTp);
        jackTrig.update(nowTp);
        bpServo.update(nowTp);

        const bool jackPresent = jackTrig.isActive();
        const bool calculoOn = swCalculo.isActive();
        // experienceOn toujours false en v1 (carte experience non couplee, pin D3 = BP_SERVO).
        const bool experienceOn = false; // swExperience.isActive(); // A activer en v2 quand la carte experience sera connectee.

        // Log uniquement sur changement pour eviter le spam UART.
        if (jackPresent != lastJackPresent) {
            lastJackPresent = jackPresent;
            if (jackPresent) {
                uart.info("[INFO] Jack detected");
            } else {
                uart.info("[INFO] Jack removed");
            }
        }

        // Jack safety has priority and cancels any ongoing sequence immediately.
        if (!jackPresent) {
            timerRunning = false;
            ledJackBlink = false;
            ledExpBlink = false;
            ledJack = 0;

            if (servoOpen) {
                servoOpen = false;
                servo.writeAngle(SERVO_REST_DEG);
                uart.info("[INFO] Servo closed");
            }

            state = SystemState::ATTENTE;
            mode = ActiveMode::NONE;
            ledExp = 0;

            ThisThread::sleep_for(milliseconds(LOOP_PERIOD_MS));
            continue;
        }

        const bool invalidMode = calculoOn && experienceOn;
        if (invalidMode) {
            // Deux modes actifs en meme temps = incoherent => erreur.
            if (!ledJackBlink) {
                uart.info("[ERROR] Invalid mode: CALCULO and EXPERIENCE active");
            }
            ledJackBlink = true;
            timerRunning = false;
            ledExpBlink = false;
            mode = ActiveMode::NONE;
            state = SystemState::ATTENTE;
        } else {
            ledJackBlink = false;
            if (calculoOn) {
                if (mode != ActiveMode::CALCULO) {
                    uart.info("[INFO] Mode CALCULO");
                }
                mode = ActiveMode::CALCULO;
                if (!timerRunning) {
                    state = SystemState::MODE_CALCULO;
                }
            } else if (experienceOn) {
                if (mode != ActiveMode::EXPERIENCE) {
                    uart.info("[INFO] Mode EXPERIENCE");
                }
                mode = ActiveMode::EXPERIENCE;
                if (!timerRunning) {
                    state = SystemState::MODE_EXPERIENCE;
                }
            } else {
                mode = ActiveMode::NONE;
                if (!timerRunning) {
                    state = SystemState::ATTENTE;
                }
            }
        }

        const bool autoConditions = jackPresent && mode != ActiveMode::NONE;
        if (autoConditions && !timerRunning && !servoOpen) {
            // Demarrage one-shot de la tempo automatique.
            timerRunning = true;
            timerStartMs = now;
            state = SystemState::TIMER_EN_COURS;
            ledExpBlink = true;
            uart.info("[INFO] Timer started");
        }

        if (timerRunning && (now - timerStartMs >= DELAI_SERVO_MS)) {
            timerRunning = false;
            ledExpBlink = false;
            servoOpen = true;
            servo.writeAngle(SERVO_OPEN_DEG);
            state = SystemState::SERVO_OUVERT;
            uart.info("[INFO] Timer expired");
            uart.info("[INFO] Servo opened");
        }

        if (bpServo.rose()) {
            // Mode bascule manuel: chaque appui inverse l'etat du servo.
            if (jackPresent) {
                timerRunning = false;
                ledExpBlink = false;

                servoOpen = !servoOpen;
                if (servoOpen) {
                    servo.writeAngle(SERVO_OPEN_DEG);
                    state = SystemState::SERVO_OUVERT;
                    uart.info("[INFO] Servo opened");
                } else {
                    servo.writeAngle(SERVO_REST_DEG);
                    state = SystemState::SERVO_FERME;
                    uart.info("[INFO] Servo closed");
                }
            } else {
                uart.info("[WARN] BP_SERVO ignored: jack absent");
            }
        }

        char command = 0;
        if (uart.readChar(command)) {
            // Mini mode maintenance via terminal serie.
            if (command == 'h' || command == 'H') {
                uart.info("[MAINT] Commands: h=help s=status i=inputs o=open c=close t=toggle");
            } else if (command == 's' || command == 'S') {
                uart.info("[MAINT] State=%d Mode=%d ServoOpen=%d Timer=%d Angle=%d",
                          static_cast<int>(state),
                          static_cast<int>(mode),
                          static_cast<int>(servoOpen),
                          static_cast<int>(timerRunning),
                          servo.lastAngle());
            } else if (command == 'i' || command == 'I') {
                uart.info("[MAINT] Inputs Calculo=%d Jack=%d BP=%d",
                          static_cast<int>(calculoOn),
                          static_cast<int>(jackPresent),
                          static_cast<int>(bpServo.isActive()));
            } else if (command == 'o' || command == 'O') {
                servoOpen = true;
                timerRunning = false;
                ledExpBlink = false;
                servo.writeAngle(SERVO_OPEN_DEG);
                state = SystemState::SERVO_OUVERT;
                uart.info("[MAINT] Servo opened");
            } else if (command == 'c' || command == 'C') {
                servoOpen = false;
                timerRunning = false;
                ledExpBlink = false;
                servo.writeAngle(SERVO_REST_DEG);
                state = SystemState::SERVO_FERME;
                uart.info("[MAINT] Servo closed");
            } else if (command == 't' || command == 'T') {
                servoOpen = !servoOpen;
                timerRunning = false;
                ledExpBlink = false;
                servo.writeAngle(servoOpen ? SERVO_OPEN_DEG : SERVO_REST_DEG);
                state = servoOpen ? SystemState::SERVO_OUVERT : SystemState::SERVO_FERME;
                uart.info("[MAINT] Servo toggled -> %s", servoOpen ? "OPEN" : "CLOSED");
            }
        }

        if (now - lastBlinkMs >= BLINK_MS) {
            // Horloge commune des clignotements LEDs.
            lastBlinkMs = now;
            blinkPhase = !blinkPhase;
        }

        ledJack = ledJackBlink ? static_cast<int>(blinkPhase) : 1;

        if (ledExpBlink) {
            ledExp = static_cast<int>(blinkPhase);
        } else {
            ledExp = (mode == ActiveMode::EXPERIENCE) ? 1 : 0;
        }

        ThisThread::sleep_for(milliseconds(LOOP_PERIOD_MS));
    }
}