#include "embedded_adapters.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace Workbench {

static std::string stateToStr(ConnectionState s) {
    switch (s) {
        case ConnectionState::Disconnected: return "DISCONNECTED";
        case ConnectionState::Connecting: return "CONNECTING";
        case ConnectionState::Connected: return "CONNECTED";
        case ConnectionState::ErrorTimeout: return "ERROR_TIMEOUT";
    }
    return "UNKNOWN";
}

// -------------------------------------------------------------
// MockMCUAdapter (STM32H753ZI)
// -------------------------------------------------------------
MockMCUAdapter::MockMCUAdapter() = default;

std::string MockMCUAdapter::getStateString() const {
    return stateToStr(state_);
}

bool MockMCUAdapter::connect() {
    if (timeoutInjected_) {
        state_ = ConnectionState::ErrorTimeout;
        return false;
    }
    state_ = ConnectionState::Connected;
    connectTime_ = std::chrono::steady_clock::now();
    packetCount_ = 0;
    return true;
}

bool MockMCUAdapter::disconnect() {
    state_ = ConnectionState::Disconnected;
    return true;
}

bool MockMCUAdapter::recoverFromTimeout() {
    timeoutInjected_ = false;
    return connect();
}

std::string MockMCUAdapter::getCapabilitiesJson() const {
    std::string s = "{\n"
        "  \"deviceId\": \"mcu-stm32h7\",\n"
        "  \"model\": \"STM32H753ZI\",\n"
        "  \"architecture\": \"Arm Cortex-M7 with DP-FPU\",\n"
        "  \"coreClockMHz\": 480,\n"
        "  \"flashSizeKB\": 2048,\n"
        "  \"ramSizeKB\": 1024,\n"
        "  \"voltageDomainV\": 3.3,\n"
        "  \"hostInterface\": \"USB CDC-ACM (Virtual COM @ 115200 baud)\",\n"
        "  \"interfaces\": [\"USART\", \"SPI\", \"I2C\", \"CAN-FD\", \"Ethernet 10/100\", \"ADC 16-bit\"],\n"
        "  \"simulated\": true\n"
        "}";
    return s;
}

std::string MockMCUAdapter::readTelemetryJson() {
    if (state_ != ConnectionState::Connected) {
        if (timeoutInjected_) state_ = ConnectionState::ErrorTimeout;
        std::ostringstream err;
        err << "{\"deviceId\":\"mcu-stm32h7\",\"state\":\"" << stateToStr(state_)
            << "\",\"error\":\"Device communication timeout or not connected.\",\"simulated\":true}";
        return err.str();
    }

    if (timeoutInjected_) {
        state_ = ConnectionState::ErrorTimeout;
        return "{\"deviceId\":\"mcu-stm32h7\",\"state\":\"ERROR_TIMEOUT\",\"error\":\"Communication timeout: MCU unresponsive after 2000 ms. Recovery required.\",\"simulated\":true}";
    }

    packetCount_++;
    auto elapsedSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - connectTime_).count();

    double tempC = 42.5 + 1.2 * std::sin(elapsedSec * 0.2);
    double vdd = 3.31 + 0.02 * std::sin(elapsedSec * 0.5);
    double cpuLoad = 28.5 + 4.0 * std::sin(elapsedSec * 0.1);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{\n";
    ss << "  \"deviceId\": \"mcu-stm32h7\",\n";
    ss << "  \"state\": \"CONNECTED\",\n";
    ss << "  \"simulated\": true,\n";
    ss << "  \"telemetry\": {\n";
    ss << "    \"coreTemperatureC\": " << tempC << ",\n";
    ss << "    \"supplyVoltageV\": " << vdd << ",\n";
    ss << "    \"cpuUtilizationPercent\": " << cpuLoad << ",\n";
    ss << "    \"activeTasks\": 5,\n";
    ss << "    \"packetCounter\": " << packetCount_ << ",\n";
    ss << "    \"uptimeSeconds\": " << static_cast<uint64_t>(elapsedSec) << ",\n";
    ss << "    \"status\": \"NOMINAL\"\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

// -------------------------------------------------------------
// MockFPGAAdapter (Xilinx Artix-7)
// -------------------------------------------------------------
MockFPGAAdapter::MockFPGAAdapter() = default;

std::string MockFPGAAdapter::getStateString() const {
    return stateToStr(state_);
}

bool MockFPGAAdapter::connect() {
    if (timeoutInjected_) {
        state_ = ConnectionState::ErrorTimeout;
        return false;
    }
    state_ = ConnectionState::Connected;
    connectTime_ = std::chrono::steady_clock::now();
    frameCount_ = 0;
    return true;
}

bool MockFPGAAdapter::disconnect() {
    state_ = ConnectionState::Disconnected;
    return true;
}

bool MockFPGAAdapter::recoverFromTimeout() {
    timeoutInjected_ = false;
    return connect();
}

std::string MockFPGAAdapter::getCapabilitiesJson() const {
    std::string s = "{\n"
        "  \"deviceId\": \"fpga-artix7\",\n"
        "  \"model\": \"Xilinx Artix-7 XC7A100T-1CSG324C\",\n"
        "  \"architecture\": \"28nm High-Performance Low-Power (HPL) FPGA\",\n"
        "  \"logicCells\": 101440,\n"
        "  \"dspSlices\": 240,\n"
        "  \"blockRamKb\": 4860,\n"
        "  \"voltageDomains\": {\"VCCINT\": 1.0, \"VCCAUX\": 1.8, \"VCCO_BANK\": 3.3},\n"
        "  \"hostInterface\": \"FTDI FT2232H Synchronous FIFO / USB 2.0 (40 MB/s)\",\n"
        "  \"configurationInterface\": \"Quad-SPI Flash / 14-pin JTAG Header\",\n"
        "  \"simulated\": true\n"
        "}";
    return s;
}

std::string MockFPGAAdapter::readTelemetryJson() {
    if (state_ != ConnectionState::Connected) {
        if (timeoutInjected_) state_ = ConnectionState::ErrorTimeout;
        std::ostringstream err;
        err << "{\"deviceId\":\"fpga-artix7\",\"state\":\"" << stateToStr(state_)
            << "\",\"error\":\"Device communication timeout or not connected.\",\"simulated\":true}";
        return err.str();
    }

    if (timeoutInjected_) {
        state_ = ConnectionState::ErrorTimeout;
        return "{\"deviceId\":\"fpga-artix7\",\"state\":\"ERROR_TIMEOUT\",\"error\":\"Communication timeout: FTDI FIFO buffer underflow / handshake loss. Recovery required.\",\"simulated\":true}";
    }

    frameCount_++;
    auto elapsedSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - connectTime_).count();

    double dieTempC = 49.8 + 2.1 * std::sin(elapsedSec * 0.15);
    double vccInt = 1.002 + 0.005 * std::sin(elapsedSec * 0.4);
    double vccAux = 1.801 + 0.008 * std::sin(elapsedSec * 0.3);
    double lutUsage = 44.2;
    double dspUsage = 18.0;

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "{\n";
    ss << "  \"deviceId\": \"fpga-artix7\",\n";
    ss << "  \"state\": \"CONNECTED\",\n";
    ss << "  \"simulated\": true,\n";
    ss << "  \"telemetry\": {\n";
    ss << "    \"dieTemperatureC\": " << dieTempC << ",\n";
    ss << "    \"vccIntV\": " << vccInt << ",\n";
    ss << "    \"vccAuxV\": " << vccAux << ",\n";
    ss << "    \"lutUtilizationPercent\": " << lutUsage << ",\n";
    ss << "    \"dspUtilizationPercent\": " << dspUsage << ",\n";
    ss << "    \"clockFrequencyMHz\": 100.00,\n";
    ss << "    \"processedFrames\": " << frameCount_ << ",\n";
    ss << "    \"bitstreamLoaded\": true,\n";
    ss << "    \"status\": \"CONFIGURED_OPERATIONAL\"\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

// -------------------------------------------------------------
// MockJTAGAdapter (Debug Adapter)
// -------------------------------------------------------------
MockJTAGAdapter::MockJTAGAdapter() = default;

std::string MockJTAGAdapter::getStateString() const {
    return stateToStr(state_);
}

bool MockJTAGAdapter::connect() {
    if (timeoutInjected_) {
        state_ = ConnectionState::ErrorTimeout;
        return false;
    }
    state_ = ConnectionState::Connected;
    connectTime_ = std::chrono::steady_clock::now();
    scanCycles_ = 0;
    return true;
}

bool MockJTAGAdapter::disconnect() {
    state_ = ConnectionState::Disconnected;
    return true;
}

bool MockJTAGAdapter::recoverFromTimeout() {
    timeoutInjected_ = false;
    return connect();
}

std::string MockJTAGAdapter::getCapabilitiesJson() const {
    std::string s = "{\n"
        "  \"deviceId\": \"jtag-probe\",\n"
        "  \"model\": \"High-Speed JTAG/SWD Debug Interface\",\n"
        "  \"role\": \"Debug and Test Access Port (TAP) Controller\",\n"
        "  \"architecturalClassification\": \"Debug & Diagnostics Tool (NOT generic compute board)\",\n"
        "  \"supportedProtocols\": [\"JTAG IEEE 1149.1\", \"ARM Serial Wire Debug (SWD)\", \"SWO Trace\"],\n"
        "  \"targetVoltageSensingRangeV\": \"1.2V to 5.0V\",\n"
        "  \"maxTckClockSpeedkHz\": 15000,\n"
        "  \"hostInterface\": \"USB 2.0 High-Speed (480 Mbps)\",\n"
        "  \"targetHeaders\": [\"10-pin 1.27mm Arm Cortex\", \"14-pin 2.0mm Xilinx JTAG\"],\n"
        "  \"simulated\": true\n"
        "}";
    return s;
}

std::string MockJTAGAdapter::readTelemetryJson() {
    if (state_ != ConnectionState::Connected) {
        if (timeoutInjected_) state_ = ConnectionState::ErrorTimeout;
        std::ostringstream err;
        err << "{\"deviceId\":\"jtag-probe\",\"state\":\"" << stateToStr(state_)
            << "\",\"error\":\"Device communication timeout or not connected.\",\"simulated\":true}";
        return err.str();
    }

    if (timeoutInjected_) {
        state_ = ConnectionState::ErrorTimeout;
        return "{\"deviceId\":\"jtag-probe\",\"state\":\"ERROR_TIMEOUT\",\"error\":\"Communication timeout: TCK clock synchronization lost / target TAP unresponsive. TAP state reset required.\",\"simulated\":true}";
    }

    scanCycles_ += 1000;
    auto elapsedSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - connectTime_).count();

    double vrefTarget = 3.305 + 0.005 * std::sin(elapsedSec * 0.1);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "{\n";
    ss << "  \"deviceId\": \"jtag-probe\",\n";
    ss << "  \"state\": \"CONNECTED\",\n";
    ss << "  \"simulated\": true,\n";
    ss << "  \"telemetry\": {\n";
    ss << "    \"targetVrefSensedV\": " << vrefTarget << ",\n";
    ss << "    \"tckSpeedkHz\": 10000,\n";
    ss << "    \"tapState\": \"Run-Test/Idle\",\n";
    ss << "    \"scanChainDevicesDetected\": 2,\n";
    ss << "    \"devicesInChain\": [\n";
    ss << "      {\"index\": 0, \"idcode\": \"0x6BA02477\", \"identity\": \"Arm Cortex-M7 CoreSight DP\"},\n";
    ss << "      {\"index\": 1, \"idcode\": \"0x0362D093\", \"identity\": \"Xilinx Artix-7 XC7A100T TAP\"}\n";
    ss << "    ],\n";
    ss << "    \"scanCyclesExecuted\": " << scanCycles_ << ",\n";
    ss << "    \"status\": \"CHAIN_SYNCHRONIZED\"\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

// -------------------------------------------------------------
// EmbeddedDeviceManager
// -------------------------------------------------------------
EmbeddedDeviceManager::EmbeddedDeviceManager() {
    devices_.push_back(std::make_shared<MockMCUAdapter>());
    devices_.push_back(std::make_shared<MockFPGAAdapter>());
    devices_.push_back(std::make_shared<MockJTAGAdapter>());
}

std::vector<std::shared_ptr<IDeviceAdapter>> EmbeddedDeviceManager::getDevices() const {
    return devices_;
}

std::shared_ptr<IDeviceAdapter> EmbeddedDeviceManager::getDevice(const std::string& id) const {
    for (const auto& dev : devices_) {
        if (dev->getId() == id) return dev;
    }
    return nullptr;
}

std::string EmbeddedDeviceManager::getAllDevicesStatusJson() {
    std::ostringstream ss;
    ss << "{\n  \"devices\": [\n";
    for (size_t i = 0; i < devices_.size(); ++i) {
        auto dev = devices_[i];
        ss << "    {\n";
        ss << "      \"id\": \"" << dev->getId() << "\",\n";
        ss << "      \"type\": \"" << dev->getTypeName() << "\",\n";
        ss << "      \"name\": \"" << dev->getDeviceName() << "\",\n";
        ss << "      \"state\": \"" << dev->getStateString() << "\",\n";
        ss << "      \"simulated\": true,\n";
        ss << "      \"capabilities\": " << dev->getCapabilitiesJson() << ",\n";
        ss << "      \"telemetry\": " << dev->readTelemetryJson() << "\n";
        ss << "    }";
        if (i + 1 < devices_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n}";
    return ss.str();
}

} // namespace Workbench
