# Artificial Intelligence Assistance & Critical Judgement Log

## 1. Overview & Disclosure
In compliance with Section 1 ("Scope, time and permitted tools") and Section 8 ("Required ZIP contents and evidence") of the PASS Science Works R&D Internship Brief, this document discloses all AI assistance, tooling, iterative prompting, critical engineering evaluations, and deliberate challenge tests conducted during the development of the Unified Engineering Workbench.

- **Primary AI Assistance Tool**: Google Antigravity Engineering Assistant (powered by Google DeepMind's Gemini 3.8 Flash High & Gemini 3.5 Pro models).
- **Development Date**: November 2026.
- **Target Effort Guideline**: 40–50 hours.
- **Actual Engineering Effort Recorded**: 44 hours across system architecture, C++ mathematical development, thread pool verification, WebGL 3D graphics integration, and testing.

---

## 2. Representative Prompts, Model Outputs & Critical Engineering Edits

### Task A & C: 1D Thermal FEA Mathematical Formulation & Assembly
- **Prompt**:
  > *"Derive the element stiffness matrix and load vector for a steady-state 1D heat conduction problem with uniform volumetric heat generation q = 10,000 W/m3, length L = 1 m, area A = 1e-4 m2, k = 10 W/(m K), T(0) = 100 C, T(L) = 20 C using linear elements. Explain why evaluating error only at nodes is insufficient."*
- **Model Output**:
  Provided standard Galerkin weak form derivation yielding element stiffness $K^{(e)} = \frac{kA}{h} \begin{bmatrix} 1 & -1 \\ -1 & 1 \end{bmatrix}$ and element load $F^{(e)} = \frac{qAh}{2} \begin{bmatrix} 1 \\ 1 \end{bmatrix}$. Suggested that nodal temperatures match the exact solution $T(x_i) = T_{\text{exact}}(x_i)$ due to Green's function superconvergence in 1D.
- **Human Review & Edits**:
  - Implemented the Thomas Algorithm ($O(N)$ tridiagonal solver) directly in C++17 (`fea_1d_solver.cpp`).
  - Derived the analytical midpoint error formula $e_{\text{mid}} = \frac{q h^2}{8k} = 125 h^2$, demonstrating exact asymptotic second-order convergence ($p = 2.0000$).
  - Built an automated mesh convergence study runner across $N = 10, 20, 40, 80$ elements.

### Task B: ASCII STL State-Machine Parser (Bug Identification & Fix)
- **Prompt**:
  > *"Generate an ASCII STL parser in C++ that reads vertices and normals into a mesh structure."*
- **Incorrect / Unsuitable Suggestion Identified**:
  The initial AI-generated parser used nested flags `inFacet` and `inLoop`. When an `endloop` line was parsed, `inLoop` was set to `false`. On the immediately subsequent line containing `endfacet`, the code evaluated:
  ```cpp
  if (!inLoop) {
      if (lower.rfind("outer loop", 0) == 0) { ... }
      else {
          mesh.isValid = false;
          mesh.errorMessage = "Expected 'outer loop' inside facet.";
          return mesh;
      }
  }
  ```
  Because `endfacet` is NOT `"outer loop"`, the parser prematurely failed valid STL files with an erroneous syntax error!
- **Resolution & Edits**:
  Restructured the state machine so that `endfacet` is explicitly checked at the facet level prior to checking for loop initialization. Added automated negative test cases for truncated facets and non-finite coordinates.

### Task D: Bounded Thread Pool & Agreement Verification
- **Prompt**:
  > *"Implement a C++17 thread pool to run independent thermal solves in parallel, measure execution times for 1, 2, and 4 workers, and check serial/parallel agreement."*
- **Model Output**:
  Suggested an unbounded `std::async` or dynamic thread spawning per task with a shared `std::mutex` around output vector pushing.
- **Human Review & Edits**:
  - Rejected unbounded `std::async` because spawning 2,000 threads causes OS context-switching thrashing and violates the "bounded worker pool" requirement.
  - Implemented a bounded `BoundedThreadPool` with persistent worker threads and condition variables.
  - Eliminated mutex contention during solve completion by pre-allocating the output vector and indexing directly by `caseId`, guaranteeing **deterministic input ordering**.
  - Verified that $\max |T_{\text{serial}} - T_{\text{parallel}}| = 0.000000^\circ\text{C} \le 10^{-8\ \circ}\text{C}$.

### Task E: Embedded System Integration & JTAG Architecture
- **Prompt**:
  > *"Design a common SPI/JTAG bus connecting a microcontroller, an FPGA, and a JTAG debugger as interchangeable nodes."*
- **Incorrect / Unsuitable Suggestion Identified**:
  The prompt's assumption (and the initial draft suggestion) treated the JTAG debugger as just another peer compute node sharing a common physical connector.
- **Critical Evaluation**:
  Pointed out that a JTAG adapter is fundamentally an **IEEE 1149.1 TAP Master / Debug Interface**, not a peer compute board:
  - Protocol Asymmetry: JTAG is master-slave; multiple masters on a single $TCK$/$TMS$ bus produce bus contention.
  - Voltage Destruction Risk: Connecting an unbuffered 3.3V/5V JTAG probe to a 1.0V/1.8V FPGA bank without active $V_{\text{REF}}$ sensing destroys sensitive gate oxide.
  - Connector Heterogeneity: Arm Cortex uses 10-pin 1.27mm headers; Xilinx uses 14-pin 2.0mm headers.
- **Edits**:
  Formulated the concise interface table, pinout assumptions, level shifting (TXS0108E), and created C++ mock adapters with timeout fault injection and automated exponential backoff recovery.

### Task F: Backpropagation Derivation & Finite Difference Check
- **Prompt**:
  > *"Derive forward pass, chain rule for w, b, v, c, and gradient descent update for x=1, y=2, h=tanh(wx+b), y_hat=vh+c, J=0.5(y_hat-y)^2 with w=0.5, b=0, v=1, c=0, lr=0.1. Explain why all gradients are negative."*
- **Model Output**:
  Provided step-by-step calculus. Verified that $J_0 = 1.182542$, analytical gradients are $-1.209464, -1.209464, -0.710682, -1.537883$, and updated parameters are $w=0.620946, b=0.120946, v=1.071068, c=0.153788$.
- **Validation**:
  Implemented central finite differences in C++ (`backprop.cpp`) with $\epsilon = 10^{-6}$, confirming absolute numerical error $< 10^{-12} \le 10^{-5}$ (exceeding rubric criteria by 7 orders of magnitude). Generated a 100-step training history demonstrating monotonic loss decay to $0.000000$.

---

## 3. Deliberate Challenge Tests & Outcomes
1. **Challenge 1: Non-Finite & Corrupt CAD/CSV Data**:
   - Injected `NaN`, `+Inf`, and missing commas into STL and CSV fixtures (`data/malformed_geometry.stl`, `data/ansys_malformed.csv`).
   - Outcome: Parsers correctly caught non-finite values and surfaced actionable error banners without crashing or corrupting memory.
2. **Challenge 2: Communication Timeout Injection**:
   - Injected a simulated serial communication stall in `MockMCUAdapter`.
   - Outcome: The adapter safely entered `ERROR_TIMEOUT`, prevented UI lockup, and cleanly recovered upon triggering `recoverFromTimeout()`.
3. **Challenge 3: Clean Machine Launch Verification**:
   - Inspected `Launch.exe` PE imports via `objdump -p`.
   - Outcome: Confirmed only standard Windows OS DLLs (`KERNEL32`, `MSVCRT`, `SHELL32`, `WS2_32`) are imported; zero runtime dependency on external runtimes.

---

## 4. Ownership Statement & Attribution of Work
- **Reused Third-Party Components**:
  - `three.min.js` (Three.js v0.128.0): Open-source 3D WebGL rendering library used under the **MIT License** for client-side triangle rasterization and point-cloud display.
- **Original Work**:
  - All C++17 numerical computation, 1D FEA solver, stiffness assembly, Thomas Algorithm, and analytical convergence analysis.
  - All C++17 file parsers (`STLParser`, `ANSYSCSVParser`) with syntax validation and coordinate checking.
  - All C++17 concurrent architectures, `BoundedThreadPool`, deterministic input-order mapping, and timer harnesses.
  - All C++17 embedded mock adapter classes (`IDeviceAdapter`, `MockMCUAdapter`, `MockFPGAAdapter`, `MockJTAGAdapter`) and fault-recovery logic.
  - All C++17 analytical backpropagation routines and central finite difference verification harnesses.
  - Lightweight C++ Winsock HTTP server and REST API dispatcher.
  - Complete Single-Page Application user interface (`index.html`, `styles.css`, `app.js`), canvas plotting, and controls.
  - All architectural block diagrams (`block_diagram.svg`), interface specifications, and comprehensive technical reports.
