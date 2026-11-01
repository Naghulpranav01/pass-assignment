# Embedded-System Integration Design & Interface Specifications

## 1. System Overview
The workbench integrates three distinct physical and architectural device classes:
1. **Microcontroller Board**: STMicroelectronics STM32H753ZI (Arm Cortex-M7 @ 480 MHz).
2. **FPGA Board**: AMD/Xilinx Artix-7 XC7A100T-1CSG324C.
3. **JTAG Debug Adapter**: SEGGER J-Link / FTDI FT2232H High-Speed TAP Controller.

---

## 2. Interface, Voltage Domains & Pin Specifications Table

| Device Class | Physical Connector | Host Interface & Protocol | Voltage Domains & Power Rails | Grounding Strategy | Level Shifting & Directionality | JTAG Signal Ownership & Pinout Assumptions |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Microcontroller (STM32H753ZI)** | • USB Type-C (Host)<br>• 10-pin 1.27mm Arm Cortex Debug Header | USB 2.0 Full-Speed CDC-ACM (Virtual COM @ 115200 baud, 8-N-1) | • $V_{\text{BUS}} = 5.0\text{ V}$ (Input)<br>• $V_{\text{DD}} = 3.3\text{ V} \pm 5\%$ (I/O & Peripherals)<br>• $V_{\text{CORE}} = 1.2\text{ V}$ (Internal LDO) | Star topology connected to central ground bus at host USB shield | On-board TXS0108E bi-directional level shifter translating $3.3\text{ V}$ MCU GPIO to $1.8\text{ V}$ FPGA I/O | **Target Device (TAP Slave)**:<br>• Driven by Debugger: `TCK` (Pin 4), `TMS` (Pin 2), `TDI` (Pin 8), `nRESET` (Pin 10, open-drain)<br>• Driven by MCU: `TDO/SWO` (Pin 6)<br>• Reference: `VREF` (Pin 1, $3.3\text{ V}$) |
| **FPGA Board (Artix-7 XC7A100T)** | • USB Type-B / Micro-USB<br>• 14-pin 2.0mm Xilinx JTAG Header<br>• 0.1" Dual PMOD headers | USB 2.0 High-Speed via FTDI FT2232H Synchronous 245 FIFO ($40\text{ MB/s}$) | • $V_{\text{EXT}} = 5.0\text{ V} / 2\text{ A}$ Barrel Jack<br>• $V_{\text{CCINT}} = 1.0\text{ V}$ (Core Logic)<br>• $V_{\text{CCAUX}} = 1.8\text{ V}$ (Clocking/JTAG)<br>• $V_{\text{CCO\_BANK}} = 3.3\text{ V} / 1.8\text{ V}$ | Dedicated copper ground plane bonded to chassis ground through $1\text{ M}\Omega \parallel 4.7\text{ nF}$ RC network | High-speed uni-directional buffers (SN74LVC1T45) for high-frequency clock lines; TXB0104 for multi-drop controls | **Target Device (TAP Slave)**:<br>• Driven by Debugger: `TCK` (Pin 4), `TMS` (Pin 6), `TDI` (Pin 8), `PROGRAM_B` (Pin 12)<br>• Driven by FPGA: `TDO` (Pin 10), `DONE` (Pin 2)<br>• Reference: `VREF` (Pin 14, $1.8\text{ V}$ or $3.3\text{ V}$) |
| **JTAG Debug Adapter (J-Link / FTDI)** | • USB 2.0 Type-A Male (Host)<br>• Multi-ribbon target flying leads (20-pin / 10-pin / 14-pin) | USB 2.0 High-Speed (480 Mbps) Bulk Transfer using WinUSB / LibUSB driver stack | • Powered via Host USB ($5.0\text{ V}$, $<150\text{ mA}$)<br>• Output logic levels dynamically track target $V_{\text{REF}}$ ($1.2\text{ V}$ to $5.0\text{ V}$) | Direct galvanic connection to target ground pin; isolated galvanic barriers recommended for high-power boards | Dynamic internal active voltage translators track target $V_{\text{REF}}$ without external level shifters | **Debug Controller (TAP Master)**:<br>• **Sole Owner / Driver**: `TCK`, `TMS`, `TDI`, `nTRST`, `nSRST`<br>• **Receiver / Sampler**: `TDO`, `RTCK` (adaptive clocking), `VREF Sense` |

---

## 3. Why a JTAG Adapter is a Debug Interface, NOT a Generic Compute Board

A common architectural misconception in embedded integration is treating a JTAG adapter as just another "board" on an interchangeable computing bus. This is technically invalid for four fundamental hardware and protocol reasons:

1. **Protocol Asymmetry (Master vs. Slave)**:
   The IEEE 1149.1 Standard defines a synchronous, state-machine driven serial boundary-scan architecture. A JTAG adapter is exclusively a **TAP Master**. It generates the test clock (`TCK`), modulates the test mode select line (`TMS`) to traverse the 16-state TAP state diagram, and shifts instructions and data bits through `TDI` and `TDO`. Conversely, compute boards (MCUs and FPGAs) are **TAP Slaves**. Connecting two TAP masters together causes catastrophic bus contention on `TCK` and `TMS`.

2. **Electrical Voltage Non-Interchangeability**:
   Target chips operate on wildly different I/O rail voltages ($V_{\text{REF}}$): modern FPGAs often feature core logic at $1.0\text{ V}$ and auxiliary logic at $1.8\text{ V}$, whereas microcontrollers typically operate at $3.3\text{ V}$ or $5.0\text{ V}$. A JTAG adapter incorporates target voltage-sensing input circuitry (`VREF_SENSE`) that dynamically gates internal line transceivers to match the target device's operating voltage. Assuming all boards share a generic connector would result in overvoltage burnout (e.g. injecting $5\text{ V}$ into a $1.8\text{ V}$ FPGA boundary-scan cell).

3. **Functionality: Diagnostics vs. General Compute**:
   A compute board runs application firmware, executes RTOS scheduler tasks, manages peripherals (CAN, Ethernet, SPI), or performs DSP filtering. A JTAG adapter has zero general-purpose application compute capability on the host side; its internal firmware exists solely to convert USB packets into raw IEEE 1149.1 bit-bang transactions, read IDCODE registers, halt execution pipelines via hardware breakpoints, and program on-chip flash memory.

4. **Connector Pinout Variations**:
   There is no single universal JTAG connector. The embedded industry uses distinct physical standards:
   - **Arm Cortex Debug**: 10-pin $1.27\text{ mm}$ pitch polarized mini-header.
   - **Legacy Arm JTAG**: 20-pin $2.54\text{ mm}$ pitch boxed header.
   - **AMD/Xilinx JTAG**: 14-pin $2.0\text{ mm}$ pitch keyed header.
   The host workbench accommodates this heterogeneity through modular C++ mock adapters that encapsulate device-specific capabilities, voltages, and pin constraints.

---

## 4. Safe Integration & Fault Recovery Plan
1. **Power-Up Sequencing**:
   Central star ground $\rightarrow$ Auxiliary supplies ($5\text{ V}$ logic) $\rightarrow$ Core rails ($1.0\text{ V}$ / $1.2\text{ V}$) $\rightarrow$ I/O rails ($1.8\text{ V}$ / $3.3\text{ V}$) $\rightarrow$ JTAG $V_{\text{REF}}$ sense confirmation before asserting `TCK`.
2. **Timeout & Communication Fault Handling**:
   If an adapter fails to acknowledge within $2000\text{ ms}$ (simulated in Task E via `injectTimeoutFault()`), the workbench transitions to `ERROR_TIMEOUT`, terminates pending transfers safely without data corruption, and prompts the user or auto-reconnect engine to execute a reset-and-resync sequence.
