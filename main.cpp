#define DEBUG

#include "mbed.h"
#include "ble/BLE.h"
#include "watchdog.h"

#define MIN_CONN_INTERVAL   16 /* (20ms) use 1.25 ms units */
#define MAX_CONN_INTERVAL   60 /* (75ms) use 1.25 ms units */
#define SLAVE_LATENCY       0
#define CONN_SUP_TIMEOUT    500 /* 5.0 seconds 10 ms units */

#ifdef DEBUG
    #define debug printf
#else
    #define debug(...)
#endif

DigitalOut statusLed(P0_21, 0);
DigitalOut connectedLed(P0_22, 1);
DigitalOut led_3(P0_23, 1);
DigitalOut led_4(P0_24, 1);
DigitalOut relay_1(P0_19, 0);
DigitalOut relay_2(P0_18, 0);
DigitalOut relay_3(P0_17, 0);
DigitalOut relay_4(P0_16, 0);
Ticker watchdogTicker;

uint16_t customServiceUUID  = 0xA000;
uint16_t readCharUUID       = 0xA001;
uint16_t writeCharUUID      = 0xA002;

const static int pulse_ms = 500;
const static float WATCH_DOG_TIMEOUT = 9.0;

const static char     DEVICE_NAME[] = "217";
static const uint16_t uuid16_list[] = {0xFFFF};
const static char     SHARED_SECRET[] = "1234";

/* Set Up custom Characteristics */
static uint8_t readValue[10] = {0};
ReadOnlyArrayGattCharacteristic<uint8_t, sizeof(readValue)> readChar(readCharUUID, readValue);

static uint8_t writeValue[10] = {0};
WriteOnlyArrayGattCharacteristic<uint8_t, sizeof(writeValue)> writeChar(writeCharUUID, writeValue);

/* Set up custom service */
GattCharacteristic *characteristics[] = {&readChar, &writeChar};
GattService        customService(customServiceUUID, characteristics, sizeof(characteristics) / sizeof(GattCharacteristic *));

void tellDogHouse() {
    debug("Kicking dog house\r\n");
    WatchDogTimer::kick();
}

void attachWatchdogTicker() {
    watchdogTicker.attach(&tellDogHouse, WATCH_DOG_TIMEOUT / 2);
    debug("Watchdog ticker attached\r\n");
}

void openAll() {
    debug("Opening both relays\r\n");
    relay_1 = 1;
    relay_2 = 1;
    relay_3 = 1;
    relay_4 = 1;
    led_3 = 0;
    led_4 = 0;
    wait_ms(pulse_ms);
    relay_1 = 0;
    relay_2 = 0;
    relay_3 = 0;
    relay_4 = 0;
    led_3 = 1;
    led_4 = 1;
}

void flipRelay(const uint8_t *data, uint16_t length)
{   
    debug("Data received length = %d\r\n", length); 
    const char *hex = reinterpret_cast<const char*>(data);    
    debug("Hex message is \"%s\"\r\n", hex);    
    if (length == 5) {
        debug("Checking code\r\n");
        if (strncmp(hex, SHARED_SECRET, sizeof(SHARED_SECRET) - 1) == 0) {
            openAll();
            debug("Relays closed\r\n");
        }
    } else {
        debug("Data = 0x");
        for(int x = 0; x < length; x++) {
            debug("%x", data[x]);
        }
        debug("\r\n");
    }
    tellDogHouse();
}

void connectionCallback(const Gap::ConnectionCallbackParams_t *params)
{
    debug("Detaching watchdog timeout\r\n");
    tellDogHouse();
    watchdogTicker.detach();
    
    debug("Connected to device\r\n");
    connectedLed = 0;
    
    BLE &ble = BLE::Instance();
    
    Gap::Handle_t handle = params->handle;
    Gap::ConnectionParams_t new_params;
    new_params.minConnectionInterval = MIN_CONN_INTERVAL;
    new_params.maxConnectionInterval = MAX_CONN_INTERVAL;
    new_params.slaveLatency = SLAVE_LATENCY;
    new_params.connectionSupervisionTimeout = CONN_SUP_TIMEOUT;
    ble.gap().updateConnectionParams(handle, &new_params);
}    


/*
 *  Restart advertising when phone app disconnects
*/
void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *)
{
    debug("Device disconnected\r\n");
    connectedLed = 1; // disable led
    relay_1 = 0;
    relay_2 = 0;
    led_3 = 1; // disable led
    led_4 = 1; // disable led
    BLE &ble = BLE::Instance();
    ble.startAdvertising();
    debug("Advertising again\r\n");
    tellDogHouse();
    attachWatchdogTicker();
}

/*
 *  Handle writes to writeCharacteristic
*/
void writeCharCallback(const GattWriteCallbackParams *params)
{
    tellDogHouse();
    /* Check to see what characteristic was written, by handle */
    if(params->handle == writeChar.getValueHandle()) {
        flipRelay(params->data, params->len);
    }
}

/*
 * Initialization callback
 */
void bleInitComplete(BLE::InitializationCompleteCallbackContext *params)
{
    BLE &ble          = params->ble;
    ble_error_t error = params->error;

    if (error != BLE_ERROR_NONE) {
        return;
    }

    ble.onConnection(connectionCallback);
    ble.onDisconnection(disconnectionCallback);
    ble.gattServer().onDataWritten(writeCharCallback);

    /* Setup advertising */
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE); // BLE only, no classic BT
    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED); // advertising type
    ble.gap().setAdvertisingInterval(GapAdvertisingParams::GAP_ADV_PARAMS_INTERVAL_MIN);
    ble.gap().setDeviceName((uint8_t *)DEVICE_NAME);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t *)uuid16_list, sizeof(uuid16_list)); // UUID's broadcast in advertising packet
    ble.setTxPower(0xAE); // Set max output power

    /* Add our custom service */
    ble.addService(customService);

    /* Start advertising */
    ble.startAdvertising();
    debug("Init complete, advertising...\r\n");
    debug("Advertising as \"%s\"\r\n", DEVICE_NAME);
}

/*
 *  Main loop
*/
int main(void)
{
    /* initialize stuff */
    printf("Start\r\n");

    WatchDogTimer::init(WATCH_DOG_TIMEOUT);
    attachWatchdogTicker();
    tellDogHouse();
    
    BLE &ble = BLE::Instance(BLE::DEFAULT_INSTANCE);
    ble.init(bleInitComplete);

    /* SpinWait for initialization to complete. This is necessary because the
     * BLE object is used in the main loop below. */
    while (ble.hasInitialized() == false) {
        /* spin loop */
    }

    /* Infinite loop waiting for BLE interrupt events */
    while (true) {
        ble.waitForEvent(); /* Save power */
    }
}