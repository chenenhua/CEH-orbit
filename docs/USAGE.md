# CEH-Orbit Debug Version Usage Guide

This document is intended for developers who are downloading, compiling, running, and debugging the **CEH-Orbit single-file public debug version** for the first time.

It focuses on:

- Project directory structure  
- Build preparation  
- Running the Qt application  
- UI components and functionality  
- Debug workflow  
- Common issues  
- Suggestions for further development  

---

## 1. Project Purpose

This project is a **research-oriented Qt visualization and debugging tool for the CEH-Orbit protocol**.

It is NOT intended for production use. Its main goals are:

- Verify the correctness of the CEH-Orbit sign/verify loop  
- Visualize `OrbitHead / LSH / Phase / EncodedOrbit_Z / RecoveredOrbit_W`  
- Perform attack simulation, collision testing, and stability analysis  
- Serve as a prototype for protocol, UI, and further development  

---

## 2. Directory Structure

Recommended layout:

```text
Demo/
├── assets/
├── docs/
├── cmake-build-debug/
├── CMakeLists.txt
├── main.cpp
├── README.md
├── LICENSE.md
└── resources.qrc
```

---

## 3. Environment Requirements

- macOS / Linux / Windows  
- Qt 6.x  
- CMake ≥ 3.16  
- C++17 / C++20  
- OpenSSL 3.x  

---

## 4. Build Instructions

### Command Line

```bash
mkdir build
cd build
cmake ..
cmake --build . -j
```

---

## 5. UI Overview

### Left Panel

- Message input  
- Random seed  
- Buttons: KeyGen / Sign / Verify / Attack / Auto Demo / Start / Stop  

### Right Panel

- LSH visualization  
- Phase visualization  
- Z waveform  
- Recovered W waveform  

---

## 6. Debug Workflow

Basic:

1. KeyGen  
2. Sign  
3. Verify  

Expected:

- Verify = PASS  
- LSH distance = 0  
- Phase deviation = 0  

---

## 7. Common Issues

- Build error → check Qt/OpenSSL paths  
- No graph → ensure Sign executed  
- Graph not changing → seed fixed  

---

## 8. Development Suggestions

- Add tolerance mode  
- Increase dimension (256/512)  
- Add UI controls  
- Add CSV export  

---

## 9. Version Positioning

This version is:

- Research prototype  
- Debuggable  
- Demonstration-ready  

Not:

- Production-ready  
- Formally proven  
- Side-channel secure  

---

## 10. Author

Chen Enhua  
Email: a106079595@qq.com
