# PASS Unified Engineering Workbench
### R&D Internship Technical Assessment (2026–2027)

A modular, high-reliability engineering workstation targeting **Windows 11 x64**. Integrates 3D CAD geometry visualization, finite-element thermal analysis, multithreaded benchmarking, embedded hardware integration, and analytical backpropagation.

---

## 1. Prerequisites & Target Environment
- **Target OS**: Windows 11 x64 (or Windows 10 x64 version 1903+).
- **Runtime Dependencies**: **Zero**. `Launch.exe` is statically compiled (`-static -static-libgcc -static-libstdc++ -lws2_32`) and relies exclusively on standard Windows OS libraries (`KERNEL32.dll`, `msvcrt.dll`, `SHELL32.dll`, `WS2_32.dll`).
- **Administrative Rights**: **None required**. Runs entirely in user space.
- **Internet Access**: **None required**. All dependencies (including Three.js) are bundled locally for 100% offline execution.
- **Supported Browsers**: Any modern browser (Microsoft Edge, Google Chrome, Mozilla Firefox, Brave).

---

## 2. One-Click Quick Start (Zero-Configuration)

1. Extract the submission ZIP archive (`CandidateID_Workbench.zip`).
2. Double-click **`Launch.exe`** in the root directory.
3. The high-performance C++ backend server boots instantly, binds to `127.0.0.1:8080`, and automatically launches the workbench interface in your default browser.
4. **To Exit**: Press `Enter` or `Ctrl+C` in the launcher console window.

> **One-Action Editor Run Configuration (VS Code / Antigravity)**:
> Select `.vscode/launch.json` configuration **`"1. Launch Packaged Workbench (One-Action)"`** and press `F5`. The workbench compiles (if needed) and opens the application immediately. (Prerequisite: standard C/C++ extension).

---

## 3. High-Level System Architecture

```
+-----------------------------------------------------------------------------------+
|                        OFFLINE SINGLE-PAGE FRONTEND                               |
|   HTML5 / CSS / Three.js 3D Viewport / 2D Canvas Plots / Telemetry Dashboard      |
+-----------------------------------------------------------------------------------+
                                         ▲ HTTP / REST (JSON)
                                         ▼ Port 8080 (127.0.0.1)
+-----------------------------------------------------------------------------------+
|                        C++17 WINSOCK SERVER & API ROUTER                          |
|   Multi-Threaded HTTP Server | Static Asset Server | REST Dispatcher (ws2_32)     |
+-----------------------------------------------------------------------------------+
                                         ▲ In-Memory C++
                                         ▼ Static Library Linkage
+-----------------------------------------------------------------------------------+
|                            C++17 CORE BACKEND MODULES                             |
|  • stl_parser.cpp         : ASCII STL Parser with Topology Validation             |
|  • ansys_csv_parser.cpp   : ANSYS Nodal Temperature CSV Parser                    |
|  • fea_1d_solver.cpp      : 1D Linear Thermal FEA Solver & Convergence Engine     |
|  • multicore_batch.cpp    : Bounded Worker Thread Pool (1, 2, 4 cores)            |
|  • embedded_adapters.cpp  : STM32H7 MCU, Artix-7 FPGA, JTAG Probe Mock Adapters   |
|  • backprop.cpp           : Analytical Backprop & Central Finite Difference Check  |
+-----------------------------------------------------------------------------------+
```

---

## 4. Supported Formats & Capabilities

### Task B: Existing CAD & Simulation Data
- **ASCII STL Geometry (`*.stl`)**:
  - Full syntax parsing (`solid ... facet normal ... outer loop ... vertex ... endsolid`).
  - Topology validation: automatic face normal recomputation if declared normal is zero or degenerate.
  - Coordinate finiteness validation: rejects non-finite numbers (`NaN`, `+Inf`, `-Inf`).
  - Interactive 3D rendering with continuous rotate, pan, zoom, fit-to-view, and wireframe toggle.
  - Tested up to **50 MB** ASCII STL files (~1,000,000 triangles).
- **ANSYS Nodal Temperature CSV (`*.csv`)**:
  - Validates required schema: `node_id,x_m,y_m,z_m,temperature_C`.
  - Rejects empty files, non-numeric values, or non-finite temperatures (`NaN`, `Inf`).
  - Interactive 3D Point Cloud with Turbo/Rainbow colormap and temperature legend ($T_{\min}$ to $T_{\max}$ °C).
  - Raycasted single-node inspection displaying Node ID, XYZ coordinates in meters, and temperature in °C.
  - Tested up to **100 MB** CSV exports (>2,000,000 nodes).

### Task C: Mechanical Thermal FEA
- Steady-state 1D heat conduction along a uniform rod: $-d/dx(kA\,dT/dx) = qA$.
- Prescribed Dirichlet boundary conditions: $T(0) = 100^\circ\text{C}$, $T(L) = 20^\circ\text{C}$.
- Default parameters: $L = 1.0\text{ m}, A = 10^{-4}\text{ m}^2, k = 10.0\text{ W/(m K)}, q = 10,000\text{ W/m}^3$.
- Linear two-node element assembly solved in $O(N)$ time via the Thomas Algorithm.
- Evaluates exact error at element midpoints: $e_{\text{mid}} = \frac{q h^2}{8k}$, proving exact second-order mesh convergence ($p = 2.0000$).

### Task D: Multicore Benchmarking
- Reproducible batch generator with fixed seed (`42`), varied conductivities $k$ and heat generation $q$.
- Bounded thread pool (`std::thread`, `std::mutex`, `std::condition_variable`) scaling across 1, 2, and 4 workers.
- Deterministic input-order result mapping.
- Pure wall-clock compute timing (excluding disk I/O, compilation, and rendering).
- Verified serial vs parallel agreement: $\max |\Delta T| = 0.000000^\circ\text{C} \le 10^{-8\ \circ}\text{C}$.

### Task E: Embedded System Integration
- Heterogeneous workbench design for Microcontroller (STM32H7), FPGA (Artix-7), and JTAG Probe.
- Complete interface/pin-voltage table, level-shifting specifications (TXS0108E), and grounding topology.
- In-depth architectural explanation of why a JTAG adapter is an IEEE 1149.1 debug interface and not an interchangeable compute board.
- C++ mock adapters providing live telemetry streaming, timeout fault injection, and automated exponential backoff recovery.

### Task F: Backpropagation & Gradient Verification
- Regression sample $x=1, y=2$, $h = \tanh(wx+b)$, $\hat{y} = vh+c$, loss $J = 0.5(\hat{y}-y)^2$.
- Initial values: $w=0.5, b=0, v=1, c=0, \eta=0.1$.
- Analytical chain-rule gradients verified against central finite differences ($\epsilon = 10^{-6}$):
  $$\left|\frac{\partial J}{\partial \theta}_{\text{analytical}} - \frac{\partial J}{\partial \theta}_{\text{numerical}}\right| < 10^{-12} \le 10^{-5} \quad (\text{PASS})$$
- 100 subsequent training steps plotted, showing smooth monotonic convergence towards zero loss.

---

## 5. CMake Source-Build & Testing Instructions

The workbench is 100% self-contained, but can also be rebuilt from source using CMake 3.16+ and any standard C++17 compiler (GCC/MinGW, Clang, or MSVC).

### Step 1: Configure with CMake
```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
```
*(On Visual Studio, use `-G "Visual Studio 17 2022"`)*

### Step 2: Build Binaries
```bash
cmake --build build --config Release
```
This generates:
- `Launch.exe` (Root directory standalone launcher)
- `bin/workbench_server.exe` (CLI backend server and result exporter)
- `bin/run_tests.exe` (Automated unit and numerical test runner)

### Step 3: Run Automated Test Suite
```bash
.\bin\run_tests.exe
```
Or run via CTest:
```bash
ctest --test-dir build --output-on-failure
```
**Test Results**: 45 / 45 Automated Unit & Numerical Tests Passed (100% Pass Rate).

### Step 4: Export Raw Results & Generate Plots
```bash
.\bin\workbench_server.exe --export
python generate_plots.py
```

---

## 6. Directory Structure

```
├── Launch.exe                  # Standalone Windows 11 executable
├── CMakeLists.txt              # Standard CMake build configuration
├── README.md                   # System documentation & usage guide
├── AI_USE_LOG.md               # Disclosed AI interactions, critical audits, ownership
├── .gitignore                  # Clean version control exclusions
├── .vscode/
│   ├── launch.json             # 1-action Antigravity / VS Code run configurations
│   └── tasks.json              # Automated build tasks
├── src/
│   ├── backend/
│   │   ├── include/            # C++ headers (FEA, Multicore, Embedded, Backprop, Parsers, HTTP)
│   │   └── src/                # C++ implementations & launcher
│   └── frontend/
│       ├── index.html          # Unified 5-view responsive engineering UI
│       ├── styles.css          # Dark engineering design theme
│       ├── app.js              # View controllers, Three.js renderers, API clients
│       └── vendor/
│           └── three.min.js    # Bundled offline WebGL 3D library (MIT License)
├── data/
│   ├── provenance.md           # Dataset provenance, coordinate systems, units, limits
│   ├── sample_bracket.stl      # Valid ASCII STL geometry fixture
│   ├── malformed_geometry.stl  # Syntax error & NaN coordinate test fixture
│   ├── empty_geometry.stl      # 0-byte STL test fixture
│   ├── ansys_thermal_synthetic.csv # Synthetic ANSYS nodal temperature CSV (525 nodes)
│   ├── ansys_malformed.csv     # Corrupt schema & NaN temperature test fixture
│   └── ansys_empty.csv         # 0-byte CSV test fixture
├── tests/
│   └── test_main.cpp           # 45 positive, negative & numerical unit tests
├── results/
│   ├── raw/                    # Raw generated CSV datasets (FEA, Multicore, Backprop)
│   └── plots/                  # 300 DPI high-resolution figures
└── docs/
    ├── report.md               # Full 6-page Comprehensive Technical Report (Markdown)
    ├── report.pdf              # Compiled 6-page Comprehensive Technical Report (PDF)
    ├── block_diagram.svg       # Labelled embedded hardware architecture diagram
    ├── pin_voltage_table.md    # Pin, voltage, level shifting & JTAG specifications
    └── handwritten_backprop/
        ├── page1_forward_derivation.png # Handwritten derivation Page 1
        ├── page2_update_gradient_check.png # Handwritten derivation Page 2
        └── handwritten_backprop.pdf # Combined 2-page handwritten document
```

---

## 7. Known Limitations & Roadmap
1. **Binary STL**: The current CAD parser is optimized for ASCII STL up to 50 MB. Future iterations will incorporate memory-mapped binary STL streaming for multi-gigabyte industrial assemblies.
2. **2D/3D Thermal FEA**: The solver currently focuses on verified 1D linear heat conduction. Extending the element formulation to 2D quad/triangle and 3D hex elements with iterative sparse solvers is planned.
3. **Physical Hardware Abstraction**: Device adapters execute verified simulated telemetry and recovery flows. The C++ adapter interface (`IDeviceAdapter`) is designed for plug-and-play binding with physical drivers (`libusb`, FTDI D2XX, SEGGER J-Link SDK).
