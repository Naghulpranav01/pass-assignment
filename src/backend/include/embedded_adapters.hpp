#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>

namespace Workbench {

enum class DeviceType {
    Microcontroller,
    FPGA,
    JTAGDebugAdapter
};

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    ErrorTimeout
};

class IDeviceAdapter {
public:
    virtual ~IDeviceAdapter() = default;
    virtual std::string getId() const = 0;
    virtual DeviceType getType() const = 0;
    virtual std::string getTypeName() const = 0;
    virtual std::string getDeviceName() const = 0;
    virtual ConnectionState getState() const = 0;
    virtual std::string getStateString() const = 0;

    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual std::string getCapabilitiesJson() const = 0;
    virtual std::string readTelemetryJson() = 0;
    virtual void injectTimeoutFault(bool enable) = 0;
    virtual bool recoverFromTimeout() = 0;
};

class MockMCUAdapter : public IDeviceAdapter {
public:
    MockMCUAdapter();
    std::string getId() const override { return "mcu-stm32h7"; }
    DeviceType getType() const override { return DeviceType::Microcontroller; }
    std::string getTypeName() const override { return "Microcontroller (Simulated)"; }
    std::string getDeviceName() const override { return "STM32H753ZI (Arm Cortex-M7 @ 480MHz)"; }
    ConnectionState getState() const override { return state_; }
    std::string getStateString() const override;

    bool connect() override;
    bool disconnect() override;
    std::string getCapabilitiesJson() const override;
    std::string readTelemetryJson() override;
    void injectTimeoutFault(bool enable) override { timeoutInjected_ = enable; }
    bool recoverFromTimeout() override;

private:
    ConnectionState state_{ConnectionState::Disconnected};
    bool timeoutInjected_{false};
    uint64_t packetCount_{0};
    std::chrono::steady_clock::time_point connectTime_;
};

class MockFPGAAdapter : public IDeviceAdapter {
public:
    MockFPGAAdapter();
    std::string getId() const override { return "fpga-artix7"; }
    DeviceType getType() const override { return DeviceType::FPGA; }
    std::string getTypeName() const override { return "FPGA (Simulated)"; }
    std::string getDeviceName() const override { return "Xilinx Artix-7 XC7A100T-1CSG324C"; }
    ConnectionState getState() const override { return state_; }
    std::string getStateString() const override;

    bool connect() override;
    bool disconnect() override;
    std::string getCapabilitiesJson() const override;
    std::string readTelemetryJson() override;
    void injectTimeoutFault(bool enable) override { timeoutInjected_ = enable; }
    bool recoverFromTimeout() override;

private:
    ConnectionState state_{ConnectionState::Disconnected};
    bool timeoutInjected_{false};
    uint64_t frameCount_{0};
    std::chrono::steady_clock::time_point connectTime_;
};

class MockJTAGAdapter : public IDeviceAdapter {
public:
    MockJTAGAdapter();
    std::string getId() const override { return "jtag-probe"; }
    DeviceType getType() const override { return DeviceType::JTAGDebugAdapter; }
    std::string getTypeName() const override { return "JTAG Debug Adapter (Simulated)"; }
    std::string getDeviceName() const override { return "High-Speed JTAG/SWD Debug Interface"; }
    ConnectionState getState() const override { return state_; }
    std::string getStateString() const override;

    bool connect() override;
    bool disconnect() override;
    std::string getCapabilitiesJson() const override;
    std::string readTelemetryJson() override;
    void injectTimeoutFault(bool enable) override { timeoutInjected_ = enable; }
    bool recoverFromTimeout() override;

private:
    ConnectionState state_{ConnectionState::Disconnected};
    bool timeoutInjected_{false};
    uint64_t scanCycles_{0};
    std::chrono::steady_clock::time_point connectTime_;
};

class EmbeddedDeviceManager {
public:
    EmbeddedDeviceManager();
    std::vector<std::shared_ptr<IDeviceAdapter>> getDevices() const;
    std::shared_ptr<IDeviceAdapter> getDevice(const std::string& id) const;
    std::string getAllDevicesStatusJson();

private:
    std::vector<std::shared_ptr<IDeviceAdapter>> devices_;
};

} // namespace Workbench
