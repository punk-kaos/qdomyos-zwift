#include "bluetoothdevicetestsuite.h"
#include "discoveryoptions.h"
#include "bluetooth.h"

const QString testUUID = QStringLiteral("b8f79bac-32e5-11ed-a261-0242ac120002");
QBluetoothUuid uuid{testUUID};

template<typename T>
void BluetoothDeviceTestSuite<T>::tryDetectDevice(bluetooth &bt, const QBluetoothDeviceInfo &deviceInfo) const {

    try {
        // It is possible to use an EXPECT_NO_THROW here, but this
        // way is easier to place a breakpoint on the call to bt.deviceDiscovered.
        bt.deviceDiscovered(deviceInfo);
    } catch (...) {
        FAIL() << "Failed to perform device detection.";
    }
}

template<typename T>
void BluetoothDeviceTestSuite<T>::SetUp() {

    if(this->typeParam.get_isAbstract())
        GTEST_SKIP() << "Device is abstract: " << this->typeParam.get_testName();

    DeviceDiscoveryInfo defaultDiscoveryInfo(true);
    this->enablingConfigurations = this->typeParam.get_configurations(defaultDiscoveryInfo, true);
    this->disablingConfigurations = this->typeParam.get_configurations(defaultDiscoveryInfo, false);

    // Assuming these settings don't matter for this device.
    if(enablingConfigurations.size()==0)
        enablingConfigurations.push_back(defaultDiscoveryInfo);

    this->defaultDiscoveryOptions = discoveryoptions{};
    this->defaultDiscoveryOptions.startDiscovery = false;
    this->defaultDiscoveryOptions.createTemplateManagers = false;

    this->names = this->typeParam.get_deviceNames();

    EXPECT_GT(this->names.size(), 0) << "No bluetooth names configured for test";

    this->testSettings.activate();
}

template<typename T>
void BluetoothDeviceTestSuite<T>::test_deviceDetection_validNames_enabled() {
    BluetoothDeviceTestData& testData = this->typeParam;

    bluetooth bt(this->defaultDiscoveryOptions);

    for(DeviceDiscoveryInfo discoveryInfo : enablingConfigurations) {
        for(QString deviceName : this->names)
        {
            this->testSettings.loadFrom(discoveryInfo);

            QBluetoothDeviceInfo deviceInfo = testData.get_bluetoothDeviceInfo(uuid, deviceName);

            // try to create the device
            this->tryDetectDevice(bt, deviceInfo);
            EXPECT_TRUE(testData.get_isExpectedDevice(bt.device())) << "Failed to detect device using name: " << deviceName.toStdString();

            // restart the bluetooth manager to clear the device
            bt.restart();
        }
    }
}

template<typename T>
void BluetoothDeviceTestSuite<T>::test_deviceDetection_validNames_disabled() {
    BluetoothDeviceTestData& testData = this->typeParam;   

    bluetooth bt(this->defaultDiscoveryOptions);

    if(this->disablingConfigurations.size()==0)
        GTEST_SKIP() << "Device has no disabling configurations: " << testData.get_testName();

    // Test that the device is not identified when disabled in the settings
    for(DeviceDiscoveryInfo discoveryInfo : this->disablingConfigurations) {
        for(QString deviceName : this->names)
        {
            this->testSettings.loadFrom(discoveryInfo);

            QBluetoothDeviceInfo deviceInfo = testData.get_bluetoothDeviceInfo(uuid, deviceName);

            // try to create the device
            this->tryDetectDevice(bt, deviceInfo);
            EXPECT_FALSE(testData.get_isExpectedDevice(bt.device())) << "Created the device when expected not to: " << deviceName.toStdString();

            // restart the bluetooth manager to clear the device
            bt.restart();
        }
    }
}


template<typename T>
void BluetoothDeviceTestSuite<T>::test_deviceDetection_validNames_invalidBluetoothDeviceInfo()  {
    BluetoothDeviceTestData& testData = this->typeParam;

    bluetooth bt(this->defaultDiscoveryOptions);
    bool hasInvalidBluetoothDeviceInfo = false;

    if(testData.get_testInvalidBluetoothDeviceInfo()) {
        hasInvalidBluetoothDeviceInfo = true;

        std::vector<DeviceDiscoveryInfo> allConfigurations;
        for(DeviceDiscoveryInfo discoveryInfo : this->disablingConfigurations)
            allConfigurations.push_back(discoveryInfo);
        for(DeviceDiscoveryInfo discoveryInfo : this->enablingConfigurations)
            allConfigurations.push_back(discoveryInfo);

        // Test that the device is not identified when there is an invalid bluetooth device info
        for(DeviceDiscoveryInfo discoveryInfo : allConfigurations) {
            for(QString deviceName : this->names)
            {
                this->testSettings.loadFrom(discoveryInfo);

                QBluetoothDeviceInfo deviceInfo = testData.get_bluetoothDeviceInfo(uuid, deviceName, false);

                // try to create the device
                this->tryDetectDevice(bt, deviceInfo);
                EXPECT_FALSE(testData.get_isExpectedDevice(bt.device())) << "Created the device when bluetooth device info is invalid: " << deviceName.toStdString();

                // restart the bluetooth manager to clear the device
                bt.restart();
            }
        }
    }

    if(!hasInvalidBluetoothDeviceInfo)
        GTEST_SKIP() << "Device test data has no invalid bluetooth device info defined: " << testData.get_testName();
}

template<typename T>
void BluetoothDeviceTestSuite<T>::test_deviceDetection_exclusions() {
    BluetoothDeviceTestData& testData = this->typeParam;

    bluetooth bt(this->defaultDiscoveryOptions);

    // Test that it doesn't detect this device if its higher priority "namesakes" are already detected.
    auto exclusions = testData.get_exclusions();
    for(auto exclusion : exclusions) {
        for(DeviceDiscoveryInfo enablingDiscoveryInfo : enablingConfigurations) {
            DeviceDiscoveryInfo discoveryInfo(enablingDiscoveryInfo);

            for(QString deviceName : this->names)
            {
                // get an enabling configuration for the exclusion
                DeviceDiscoveryInfo exclusionDiscoveryInfo(true);
                QString exclusionDeviceName = exclusion.get()->get_deviceNames()[0];
                QBluetoothDeviceInfo exclusionDeviceInfo = exclusion.get()->get_bluetoothDeviceInfo(uuid, deviceName);
                std::vector<DeviceDiscoveryInfo> configurationsEnablingTheExclusion = exclusion.get()->get_configurations(exclusionDiscoveryInfo, true);

                if(configurationsEnablingTheExclusion.size()>0) {
                    // get a configuration that should enable the detection of the excluding device
                    exclusionDiscoveryInfo = configurationsEnablingTheExclusion[0];
                }
                this->testSettings.loadFrom(exclusionDiscoveryInfo);

                // try to create the excluding device
                this->tryDetectDevice(bt, exclusionDeviceInfo);
                EXPECT_TRUE(exclusion.get()->get_isExpectedDevice(bt.device())) << "Failed to create exclusion device: " << exclusion.get()->get_testName();

                // now configure to have the bluetooth object try, but fail to detect the target device
                this->testSettings.loadFrom(discoveryInfo);
                QBluetoothDeviceInfo deviceInfo = testData.get_bluetoothDeviceInfo(uuid, deviceName);

                this->tryDetectDevice(bt, deviceInfo);
                EXPECT_FALSE(testData.get_isExpectedDevice(bt.device())) << "Detected the " << testData.get_testName()
                                                                         << " from "
                                                                         << deviceName.toStdString()
                                                                         << " in spite of exclusion";

                bt.restart();
            }
        }
    }
}

template<typename T>
void BluetoothDeviceTestSuite<T>::test_deviceDetection_invalidNames_enabled()
{
    BluetoothDeviceTestData& testData = this->typeParam;

    auto invalidNames = testData.get_failingDeviceNames();

    if(invalidNames.length()==0)
        GTEST_SKIP() << "No invalid names have been generated or explicitly defined for this device: " << testData.get_testName();

    bluetooth bt(this->defaultDiscoveryOptions);    

    // Test that it doesn't detect this device for the "wrong" names
    for(DeviceDiscoveryInfo enablingDiscoveryInfo : enablingConfigurations) {

        DeviceDiscoveryInfo discoveryInfo(enablingDiscoveryInfo);

        for(QString deviceName : invalidNames)
        {
            this->testSettings.loadFrom(discoveryInfo);

            QBluetoothDeviceInfo deviceInfo = testData.get_bluetoothDeviceInfo(uuid, deviceName);

            // try to create the device
            this->tryDetectDevice(bt, deviceInfo);
            EXPECT_FALSE(testData.get_isExpectedDevice(bt.device())) << "Detected device from invalid name: "
                                                                     << deviceName.toStdString();

            // restart the bluetooth manager to clear the device
            bt.restart();
        }
    }
}


