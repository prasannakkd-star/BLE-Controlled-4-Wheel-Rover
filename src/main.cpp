#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <string>

// =====================================================
//                  L298N MOTOR PINS
// =====================================================

// LEFT SIDE
const int IN1 = 27;
const int IN2 = 26;

// RIGHT SIDE
const int IN3 = 25;
const int IN4 = 33;

// PWM ENABLE PINS
const int ENA = 14;
const int ENB = 32;


// =====================================================
//                    PWM SETTINGS
// =====================================================

const int ENA_CHANNEL = 0;
const int ENB_CHANNEL = 1;

const int PWM_FREQUENCY = 5000;
const int PWM_RESOLUTION = 8;       // 8-bit = 0-255


// =====================================================
//                 BLE NUS UUIDs
// =====================================================

#define SERVICE_UUID \
"6E400001-B5A3-F393-E0A9-E50E24DCCA9E"

#define CHARACTERISTIC_UUID_RX \
"6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

#define CHARACTERISTIC_UUID_TX \
"6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


BLECharacteristic *pTxCharacteristic;

bool deviceConnected = false;


// =====================================================
//                     SPEED
// =====================================================

// Default speed
int baseSpeed = 180;

// Current motor speeds
int currentLeftSpeed = 0;
int currentRightSpeed = 0;

// Target motor speeds
int targetLeftSpeed = 0;
int targetRightSpeed = 0;


// =====================================================
//                 SAFETY TIMEOUT
// =====================================================

// Robot stops automatically if no command is received
// for this amount of time.
//
// 3000 = 3 seconds
//
// Set to 0 if you don't want automatic timeout.

const unsigned long COMMAND_TIMEOUT = 3000;

unsigned long lastCommandTime = 0;


// =====================================================
//                  MOTOR CONTROL
// =====================================================

void setMotor(
    int in1,
    int in2,
    int pwmChannel,
    int speed
)
{
    speed = constrain(speed, -255, 255);


    // -------------------------------
    // FORWARD
    // -------------------------------

    if (speed > 0)
    {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
    }


    // -------------------------------
    // BACKWARD
    // -------------------------------

    else if (speed < 0)
    {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
    }


    // -------------------------------
    // STOP
    // -------------------------------

    else
    {
        digitalWrite(in1, LOW);
        digitalWrite(in2, LOW);
    }


    // -------------------------------
    // PWM
    // -------------------------------

    ledcWrite(
        pwmChannel,
        abs(speed)
    );
}


// =====================================================
//                 UPDATE MOTORS
// =====================================================

void updateMotors()
{
    setMotor(
        IN1,
        IN2,
        ENA_CHANNEL,
        currentLeftSpeed
    );

    setMotor(
        IN3,
        IN4,
        ENB_CHANNEL,
        currentRightSpeed
    );
}


// =====================================================
//               STOP ROBOT IMMEDIATELY
// =====================================================

void emergencyStop()
{
    targetLeftSpeed = 0;
    targetRightSpeed = 0;

    currentLeftSpeed = 0;
    currentRightSpeed = 0;

    updateMotors();
}


// =====================================================
//                 SMOOTH MOTOR RAMP
// =====================================================

void rampMotors()
{
    const int STEP = 10;


    // -------------------------------
    // LEFT MOTOR
    // -------------------------------

    if (currentLeftSpeed < targetLeftSpeed)
    {
        currentLeftSpeed += STEP;
    }
    else if (currentLeftSpeed > targetLeftSpeed)
    {
        currentLeftSpeed -= STEP;
    }


    // -------------------------------
    // RIGHT MOTOR
    // -------------------------------

    if (currentRightSpeed < targetRightSpeed)
    {
        currentRightSpeed += STEP;
    }
    else if (currentRightSpeed > targetRightSpeed)
    {
        currentRightSpeed -= STEP;
    }


    // Prevent overshooting target

    if (abs(currentLeftSpeed - targetLeftSpeed) < STEP)
    {
        currentLeftSpeed = targetLeftSpeed;
    }

    if (abs(currentRightSpeed - targetRightSpeed) < STEP)
    {
        currentRightSpeed = targetRightSpeed;
    }


    currentLeftSpeed =
        constrain(currentLeftSpeed, -255, 255);

    currentRightSpeed =
        constrain(currentRightSpeed, -255, 255);


    updateMotors();
}


// =====================================================
//                 PROCESS COMMAND
// =====================================================

void processCommand(String command)
{
    command.trim();

    if (command.length() == 0)
    {
        return;
    }


    // Convert command to uppercase

    command.toUpperCase();


    Serial.print("Received: ");
    Serial.println(command);


    // =================================================
    //                   SPEED COMMAND
    // =================================================
    //
    // Examples:
    //
    // V100
    // V180
    // V255
    //
    // =================================================

    if (command.startsWith("V"))
    {
        String speedText =
            command.substring(1);

        int newSpeed =
            speedText.toInt();


        if (newSpeed >= 0 && newSpeed <= 255)
        {
            baseSpeed = newSpeed;

            Serial.print("Speed changed to: ");
            Serial.println(baseSpeed);


            if (deviceConnected)
            {
                String response =
                    "Speed: " + String(baseSpeed);

                pTxCharacteristic->setValue(
                    response.c_str()
                );

                pTxCharacteristic->notify();
            }
        }
        else
        {
            Serial.println(
                "Invalid speed. Use V0 to V255"
            );
        }

        lastCommandTime = millis();

        return;
    }


    // =================================================
    //                     FORWARD
    // =================================================

    if (command == "F")
    {
        targetLeftSpeed = baseSpeed;
        targetRightSpeed = baseSpeed;
    }


    // =================================================
    //                    BACKWARD
    // =================================================

    else if (command == "B")
    {
        targetLeftSpeed = -baseSpeed;
        targetRightSpeed = -baseSpeed;
    }


    // =================================================
    //                    SHARP LEFT
    // =================================================

    else if (command == "L")
    {
        targetLeftSpeed = -baseSpeed;
        targetRightSpeed = baseSpeed;
    }


    // =================================================
    //                    SHARP RIGHT
    // =================================================

    else if (command == "R")
    {
        targetLeftSpeed = baseSpeed;
        targetRightSpeed = -baseSpeed;
    }


    // =================================================
    //               GENTLE FORWARD LEFT
    // =================================================

    else if (command == "G")
    {
        targetLeftSpeed = baseSpeed / 2;
        targetRightSpeed = baseSpeed;
    }


    // =================================================
    //               GENTLE FORWARD RIGHT
    // =================================================

    else if (command == "I")
    {
        targetLeftSpeed = baseSpeed;
        targetRightSpeed = baseSpeed / 2;
    }


    // =================================================
    //               GENTLE BACKWARD LEFT
    // =================================================

    else if (command == "H")
    {
        targetLeftSpeed = -(baseSpeed / 2);
        targetRightSpeed = -baseSpeed;
    }


    // =================================================
    //               GENTLE BACKWARD RIGHT
    // =================================================

    else if (command == "J")
    {
        targetLeftSpeed = -baseSpeed;
        targetRightSpeed = -(baseSpeed / 2);
    }


    // =================================================
    //                       STOP
    // =================================================

    else if (command == "S")
    {
        targetLeftSpeed = 0;
        targetRightSpeed = 0;
    }


    // =================================================
    //                 UNKNOWN COMMAND
    // =================================================

    else
    {
        Serial.print("Unknown command: ");
        Serial.println(command);

        return;
    }


    // Reset timeout

    lastCommandTime = millis();


    // Debug information

    Serial.print("Target Left: ");
    Serial.print(targetLeftSpeed);

    Serial.print(" | Target Right: ");
    Serial.println(targetRightSpeed);
}


// =====================================================
//               BLE SERVER CALLBACKS
// =====================================================

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;

        lastCommandTime = millis();

        Serial.println();
        Serial.println("================================");
        Serial.println("BLE CONNECTED");
        Serial.println("================================");
    }


    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;

        Serial.println();
        Serial.println("BLE DISCONNECTED");
        Serial.println("Emergency stopping robot...");


        // SAFETY STOP

        emergencyStop();


        delay(100);


        // Restart advertising

        pServer->getAdvertising()->start();

        Serial.println(
            "BLE advertising restarted."
        );
    }
};


// =====================================================
//             BLE CHARACTERISTIC CALLBACK
// =====================================================

class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(
        BLECharacteristic *pCharacteristic
    )
    {

        // ---------------------------------------------
        // IMPORTANT:
        // Your BLE library returns std::string
        // ---------------------------------------------

        std::string rawValue =
            pCharacteristic->getValue();


        if (rawValue.length() == 0)
        {
            return;
        }


        // Convert std::string → Arduino String

        String value =
            String(rawValue.c_str());


        Serial.print("BLE Data: ");
        Serial.println(value);


        processCommand(value);
    }
};


// =====================================================
//                       SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);

    delay(500);


    Serial.println();
    Serial.println("================================");
    Serial.println("       ESP32 ROBOT CAR");
    Serial.println("================================");


    // =================================================
    //                 MOTOR PINS
    // =================================================

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);

    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);


    // =================================================
    //                  PWM SETUP
    // =================================================
    //
    // OLD ESP32 ARDUINO API
    //
    // This is the API your current project expects.
    //
    // =================================================

    ledcSetup(
        ENA_CHANNEL,
        PWM_FREQUENCY,
        PWM_RESOLUTION
    );

    ledcAttachPin(
        ENA,
        ENA_CHANNEL
    );


    ledcSetup(
        ENB_CHANNEL,
        PWM_FREQUENCY,
        PWM_RESOLUTION
    );

    ledcAttachPin(
        ENB,
        ENB_CHANNEL
    );


    // Start with motors stopped

    emergencyStop();


    // =================================================
    //                     BLE SETUP
    // =================================================

    BLEDevice::init("ESP32_ROBOT");


    BLEServer *pServer =
        BLEDevice::createServer();

    pServer->setCallbacks(
        new MyServerCallbacks()
    );


    // Create BLE service

    BLEService *pService =
        pServer->createService(
            SERVICE_UUID
        );


    // =================================================
    //                     TX
    // =================================================

    pTxCharacteristic =
        pService->createCharacteristic(
            CHARACTERISTIC_UUID_TX,
            BLECharacteristic::PROPERTY_NOTIFY
        );


    pTxCharacteristic->addDescriptor(
        new BLE2902()
    );


    // =================================================
    //                     RX
    // =================================================

    BLECharacteristic *pRxCharacteristic =
        pService->createCharacteristic(
            CHARACTERISTIC_UUID_RX,
            BLECharacteristic::PROPERTY_WRITE
        );


    pRxCharacteristic->setCallbacks(
        new MyCallbacks()
    );


    // Start service

    pService->start();


    // =================================================
    //                  BLE ADVERTISING
    // =================================================

    BLEAdvertising *pAdvertising =
        BLEDevice::getAdvertising();


    pAdvertising->addServiceUUID(
        SERVICE_UUID
    );


    pAdvertising->setScanResponse(true);


    BLEDevice::startAdvertising();


    // =================================================
    //                    READY
    // =================================================

    Serial.println();
    Serial.println("BLE Robot Ready!");
    Serial.println("Device: ESP32_ROBOT");
    Serial.println();
    Serial.println("Commands:");
    Serial.println("-----------------------------");
    Serial.println("F  = Forward");
    Serial.println("B  = Backward");
    Serial.println("L  = Sharp Left");
    Serial.println("R  = Sharp Right");
    Serial.println("G  = Forward Left");
    Serial.println("I  = Forward Right");
    Serial.println("H  = Backward Left");
    Serial.println("J  = Backward Right");
    Serial.println("S  = Stop");
    Serial.println("V0-V255 = Speed");
    Serial.println("-----------------------------");
    Serial.println();
}


// =====================================================
//                        LOOP
// =====================================================

void loop()
{
    // Smooth acceleration/deceleration

    rampMotors();


    // =================================================
    //                 SAFETY TIMEOUT
    // =================================================

    if (
        COMMAND_TIMEOUT > 0 &&
        deviceConnected &&
        millis() - lastCommandTime > COMMAND_TIMEOUT
    )
    {
        if (
            targetLeftSpeed != 0 ||
            targetRightSpeed != 0
        )
        {
            Serial.println(
                "Command timeout -> STOP"
            );

            targetLeftSpeed = 0;
            targetRightSpeed = 0;
        }
    }


    delay(20);
}