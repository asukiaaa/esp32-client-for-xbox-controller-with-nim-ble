#include <Arduino.h>

/** NimBLE_Server Demo:
 *
 *  Demonstrates many of the available features of the NimBLE client library.
 *  
 *  Created: on March 24 2020
 *      Author: H2zero
 * 
*/

#include <NimBLEDevice.h>

void scanEndedCB(NimBLEScanResults results);

static NimBLEAdvertisedDevice* advDevice;

static bool doConnect = false;
static uint32_t scanTime = 0; /** 0 = scan forever */

static NimBLEAddress targetDeviceAddress("44:16:22:5e:b2:d4");

static NimBLEUUID uuidServiceL2cap("00000001-5f60-4c4f-9c83-a7953298d40d");
static NimBLEUUID uuidServiceGeneral("1801");
static NimBLEUUID uuidServiceBattery("180f");

static NimBLEUUID uuidServiceHid("1812");
// UUID Report Charcteristic
static NimBLEUUID uuidCharaReport("2a4d");

static NimBLEUUID uuidCharaPnp("2a50");

static NimBLEUUID uuidCharaHidInformation("2a4a");
static NimBLEUUID uuidCharaPeripheralAppearance("2a01");
static NimBLEUUID uuidCharaPeripheralControlParameters("2a04");

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */  
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        Serial.println("Connected");
        /** After connection we should change the parameters if we don't need fast response times.
         *  These settings are 150ms interval, 0 latency, 450ms timout. 
         *  Timeout should be a multiple of the interval, minimum is 100ms.
         *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
         *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout 
         */
        pClient->updateConnParams(120,120,0,60);
    };

    void onDisconnect(NimBLEClient* pClient) {
        Serial.print(pClient->getPeerAddress().toString().c_str());
        Serial.println(" Disconnected - Starting scan");
        NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
    };
    
    /** Called when the peripheral requests a change to the connection parameters.
     *  Return true to accept and apply them or false to reject and keep 
     *  the currently used parameters. Default will return true.
     */
    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
        if(params->itvl_min < 24) { /** 1.25ms units */
            return false;
        } else if(params->itvl_max > 40) { /** 1.25ms units */
            return false;
        } else if(params->latency > 2) { /** Number of intervals allowed to skip */
            return false;
        } else if(params->supervision_timeout > 100) { /** 10ms units */
            return false;
        }

        return true;
    };
	
    /********************* Security handled here **********************
    ****** Note: these are the same return values as defaults ********/
    uint32_t onPassKeyRequest(){
        Serial.println("Client Passkey Request");
        /** return the passkey to send to the server */
        return 123456;
    };

    bool onConfirmPIN(uint32_t pass_key){
        Serial.print("The passkey YES/NO number: ");
        Serial.println(pass_key);
    /** Return false if passkeys don't match. */
        return true;
    };

    /** Pairing process complete, we can check the results in ble_gap_conn_desc */
    void onAuthenticationComplete(ble_gap_conn_desc* desc){
        if(!desc->sec_state.encrypted) {
            Serial.println("Encrypt connection failed - disconnecting");
            /** Find the client with the connection handle provided in desc */
            NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
            return;
        }
    };
};


/** Define a class to handle the callbacks when advertisments are received */
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        Serial.print("Advertised Device found: ");
        Serial.println(advertisedDevice->toString().c_str());
        Serial.printf("name:%s, address:%s\n", advertisedDevice->getName().c_str(), advertisedDevice->getAddress().toString().c_str());
        Serial.printf("uuidService:%s\n", advertisedDevice->haveServiceUUID() ? advertisedDevice->getServiceUUID().toString().c_str() : "none");

        if (advertisedDevice->getAddress().equals(targetDeviceAddress))
        // if (advertisedDevice->isAdvertisingService(uuidServiceHid))
        {
            Serial.println("Found Our Service");
            /** stop scan before connecting */
            NimBLEDevice::getScan()->stop();
            /** Save the device reference in a global for the client to use*/ 
            advDevice = advertisedDevice;
            /** Ready to connect now */ 
            doConnect = true;
        }
    };
};


/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
    std::string str = (isNotify == true) ? "Notification" : "Indication"; 
    str += " from ";
    /** NimBLEAddress and NimBLEUUID have std::string operators */
    str += std::string(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress());
    str += ": Service = " + std::string(pRemoteCharacteristic->getRemoteService()->getUUID());
    str += ", Characteristic = " + std::string(pRemoteCharacteristic->getUUID());
    // str += ", Value = " + std::string((char*)pData, length);
    Serial.println(str.c_str());
    Serial.print("value: ");
    for (int i = 0; i < length; ++i) {
        Serial.printf(" %02x", pData[i]);
    }
    Serial.println("");
}

/** Callback to process the results of the last scan or restart it */
void scanEndedCB(NimBLEScanResults results){
    Serial.println("Scan Ended");
}


/** Create a single global instance of the callback class to be used by all clients */
static ClientCallbacks clientCB;

bool write(NimBLERemoteCharacteristic* pCharaReportWritable, const uint8_t* data, int len) {
    Serial.println("Write value");
    auto result = pCharaReportWritable->writeValue(data, len, true);
    if (!result) {
        Serial.println("retry wrigin");
    }
    result = pCharaReportWritable->writeValue(data, len, true);
    if (result) {
        Serial.println("Wrote value");
    } else {
        Serial.println("failed writing");
    }
    return result;
}

void printValue(std::__cxx11::string str) {
    Serial.printf("str: %s\n", str.c_str());
    Serial.printf("hex:");
    for (auto v : str) {
        Serial.printf(" %02x", v);
    }
    Serial.println("");
}

void charaPrintId(NimBLERemoteCharacteristic *pChara) {
    Serial.printf(
        "s:%s c:%s h:%d",
        pChara->getRemoteService()->getUUID().toString().c_str(),
        pChara->getUUID().toString().c_str(),
        pChara->getHandle());
}

void charaCheckDescriptor(NimBLERemoteCharacteristic *pChara) {
    charaPrintId(pChara);
    Serial.print(" ");
    auto vecDescriptor = pChara->getDescriptors(true);
    if (vecDescriptor == nullptr || vecDescriptor->size() == 0) {
        Serial.println("does not have descriptor");
        return;
    } else {
        Serial.printf("has %d descriptors\n", vecDescriptor->size());
    }
    for (auto descriptor: *vecDescriptor) {
        Serial.println(descriptor->getUUID().toString().c_str());
        auto value = descriptor->readValue();
        printValue(value);
    }
}

void charaRead(NimBLERemoteCharacteristic *pChara) {
    if (pChara->canRead()) {
        charaPrintId(pChara);
        Serial.println(" canRead");
        auto str = pChara->readValue();
        if (str.size() == 0) {
            str = pChara->readValue();
        }
        printValue(str);
    }
}

void charaSubscribeNotification(NimBLERemoteCharacteristic *pChara) {
    if (pChara->canNotify()) {
        charaPrintId(pChara);
        Serial.println(" canNotify ");
        if (pChara->subscribe(true, notifyCB, true)) {
            Serial.println("set notifyCb");
            // return true;
        } else {
            Serial.println("failed to subscribe");
            // return false; // Disconnect if subscribe failed
        }
    }
}

/** Handles the provisioning of clients and connects / interfaces with the server */
bool connectToServer() {
    NimBLEClient* pClient = nullptr;
    /** Check if we have a client we should reuse first **/
    if (NimBLEDevice::getClientListSize()) {
        /** Special case when we already know this device, we send false as the 
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        pClient->secureConnection();
        if(pClient){
            // pClient->disconnect();
            if (!pClient->connect(advDevice)) {
            // if(!pClient->connect(advDevice, false)) {
                Serial.println("Reconnect failed");
                return false;
            }
            Serial.println("Reconnected client");
        } 
        /** We don't already have a client that knows this device,
         *  we will check for a client that is disconnected that we can use.
         */
        else {
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }
    
    /** No client to reuse? Create a new one. */
    if(!pClient) {
        if(NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
            Serial.println("Max clients reached - no more connections available");
            return false;
        }
        
        pClient = NimBLEDevice::createClient();
        
        Serial.println("New client created");
    
        pClient->setClientCallbacks(&clientCB, false);
        /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout. 
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less 
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout 
         */
        pClient->setConnectionParams(12,12,0,51);
        /** Set how long we are willing to wait for the connection to complete (seconds), default is 30. */
        pClient->setConnectTimeout(5);
        

        if (!pClient->connect(advDevice)) {
            /** Created a client but failed to connect, don't need to keep it as it has no data */
            NimBLEDevice::deleteClient(pClient);
            Serial.println("Failed to connect, deleted client");
            return false;
        }
    }         
    
    if(!pClient->isConnected()) {
        if (!pClient->connect(advDevice)) {
            Serial.println("Failed to connect");
            return false;
        }
    }
    
    Serial.print("Connected to: ");
    Serial.println(pClient->getPeerAddress().toString().c_str());
    Serial.print("RSSI: ");
    Serial.println(pClient->getRssi());

    auto vecService = pClient->getServices(true);
    if (vecService == nullptr || vecService->size() == 0) {
        Serial.println("no services");
        return false;
    }

    // auto pService = pClient->getService(uuidServiceL2cap);
    // charasRead(pService->getCharacteristics(true));

    for (auto pService: *vecService) {
        // if (pService->getUUID().equals(uuidServiceL2cap)) {
        //     continue; // skip
        // }
        Serial.println(pService->toString().c_str());
        auto sUuid = pService->getUUID();

        auto vecChara = pService->getCharacteristics(true);
        if (vecChara == nullptr || vecChara->size() == 0) {
            Serial.println("no characteristics");
            // return false;
            continue;
        }

        for (auto pChara: *vecChara) {
            charaCheckDescriptor(pChara);
            charaRead(pChara);
            // charaSubscribeNotification(pChara);

            auto cUuid = pChara->getUUID();
            if (pChara->canWrite()) {
                Serial.print("canWrite ");
                Serial.print(sUuid.toString().c_str());
                Serial.print(" ");
                Serial.println(cUuid.toString().c_str());
                // if (sUuid.equals(uuidServiceL2cap)) {
                //     uint8_t data[] = {0, 1};
                //     write(pChara, data, 2);
                // }
            }
        }

    }

    // // set notificatino after all
    // for (auto pService: *vecService) {
    //     auto vecChara = pService->getCharacteristics(true);
    //     charasSubscribeNotification(vecChara);
    // }

    Serial.println("Done with this device!");
    return true;
}

void setup (){
    Serial.begin(115200);
    Serial.println("Starting NimBLE Client");
    /** Initialize NimBLE, no device name spcified as we are not advertising */
    NimBLEDevice::init("");
    // NimBLEDevice::setMTU(BLE_ATT_MTU_MAX);
    // NimBLEDevice::setMTU(256);
    Serial.println("After init");
    
    /** Set the IO capabilities of the device, each option will trigger a different pairing method.
     *  BLE_HS_IO_KEYBOARD_ONLY    - Passkey pairing
     *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
     *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
     */
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY); // use passkey
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison
  
    /** 2 different ways to set security - both calls achieve the same result.
     *  no bonding, no man in the middle protection, secure connections.
     *     
     *  These are the default values, only shown here for demonstration.   
     */ 
    //NimBLEDevice::setSecurityAuth(false, false, true); 
    // NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);
    // NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC);
    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC);
    // NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_SC);
  
    // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // set display output capability
    // NimBLEDevice::setSecurityAuth(false, true, true); // no bonding, man in the middle protection, secure connection
    // NimBLEDevice::setSecurityAuth(true, true, true);

    /** Optional: set the transmit power, default is 3db */
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
    
    /** Optional: set any devices you don't want to get advertisments from */
    // NimBLEDevice::addIgnored(NimBLEAddress ("aa:bb:cc:dd:ee:ff")); 

    /** create new scan */  
    NimBLEScan* pScan = NimBLEDevice::getScan(); 
    
    /** create a callback that gets called when advertisers are found */
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    
    /** Set scan interval (how often) and window (how long) in milliseconds */
    pScan->setInterval(45);
    pScan->setWindow(15);
    
    /** Active scan will gather scan response data from advertisers
     *  but will use more energy from both devices
     */
    pScan->setActiveScan(true);
    /** Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
     *  Optional callback for when scanning stops. 
     */
    pScan->start(scanTime, scanEndedCB);
}


void loop (){
    /** Loop here until we find a device we want to connect to */
    while(!doConnect){
        delay(1);
    }
    
    doConnect = false;
    
    /** Found a device we want to connect to, do it now */
    if(connectToServer()) {
        Serial.println("Success! we should now be getting notifications, scanning for more!");
    } else {
        Serial.println("Failed to connect, starting scan");
    }
    
    NimBLEDevice::getScan()->start(scanTime,scanEndedCB);
}
