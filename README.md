# AeroTerminal

A High-Performance Native Workspace and Decision Intelligence System for Discretionary Traders.

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Aero-Terminal/AeroTerminal/blob/main/LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)](https://isocpp.org/)
[![Python](https://img.shields.io/badge/Python-3.10+-3776AB?logo=python&logoColor=white)](https://www.python.org/)
[![WebView2](https://img.shields.io/badge/WebView2-Evergreen-blue?logo=microsoftedge)](https://developer.microsoft.com/en-us/microsoft-edge/webview2/)

[Website](https://landing-page-aeroterminal.vercel.app/) · [Releases](https://github.com/Aero-Terminal/AeroTerminal/releases) · [License](#license) · [Discussions](https://github.com/Aero-Terminal/AeroTerminal/discussions)

---

## Overview

AeroTerminal is a native C++17 desktop workspace designed to eliminate the fragmentation discretionary traders face every day. By consolidating real-time charting, an infinite vector-based research whiteboard, and broker order execution into a single C++/OpenGL and ImGui viewport, AeroTerminal removes cognitive window-switching overhead, minimizes execution latency, and guarantees complete trade journaling discipline through its Thesis-Locked Execution Gate.

---

## Features

| Feature | Description |
|---------|-------------|
| **High-Performance Charting** | GPU-accelerated candlestick charts rendered via OpenGL, featuring real-time data ingestion, multi-chart synchronization, and dynamic indicator overlays. |
| **Research Whiteboard** | An embedded infinite-canvas vector whiteboard powered by an offline tldraw bundle for quick ideation, trade sketching, and canvas persisting. |
| **Saathi AI Assistant** | Fully offline local/remote LLM agent framework to evaluate trade journals, critique setups, and provide real-time coaching without exposing trade data. |
| **Algorithmic Workspace** | An integrated Jupyter Lab instance embedded directly within the application window, running local Python kernels for quant analysis and backtesting. |
| **Thesis-Locked Gate** | Hard-coded risk compliance rules that prevent order submission until the trader fills in stop-loss, take-profit limits, and documents their trade thesis. |
| **Isolated Local Storage** | A secure SQLite database backend that automatically stores user preferences, journals, and local cache with encrypted broker API credentials via Windows DPAPI. |

---

## Installation and Setup

### Option 1: Standalone Binary Deployment (Recommended)

To deploy the pre-compiled binary:

1. Download the executable bundle from the [Releases page](https://github.com/Aero-Terminal/AeroTerminal/releases).
2. Ensure both **AeroTerminal.exe** and **WebView2Loader.dll** are in the same folder.
3. Double-click **AeroTerminal.exe** to run.

On startup, the application automatically handles path resolution and initializes isolated workspace storage in `%APPDATA%\AeroTerminal\storage\`.

---

### Option 2: Build from Source (Manual)

To build the workstation locally, ensure you meet the system prerequisites below.

#### Prerequisites

| Tool | Version | Purpose and Links |
|------|---------|-------------------|
| **C++ Compiler** | **MinGW-w64 (GCC 7.3.0+)** or **MSVC** | Standard C++17 compiler toolchain. |
| **CMake** | **3.25+** | Project build configuration system. [Download](https://cmake.org/download/) |
| **WebView2 Runtime** | **Evergreen** | Runs the embedded tldraw and Jupyter pages. Pre-installed on Windows 11; required for Windows 10. [Direct Download Link](https://msedge.sf.dl.delivery.mp.microsoft.com/filestreamingservice/files/06cb87b1-2856-42d4-a28b-b892a0a2df33/MicrosoftEdgeWebview2Setup.exe) |
| **Python** | **3.10 - 3.12** | Core runtime for Jupyter and Saathi AI helpers. [Download](https://www.python.org/downloads/) |
| **Jupyter** | **Notebook 7.x** | Run `pip install jupyter` or `pip install notebook==7.2.2` to set up the kernel environment. |

#### Build Instructions

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/Aero-Terminal/AeroTerminal.git
   cd AeroTerminal
   ```

2. **Generate the Build System**:
   Create the build directory and configure the project with CMake:
   ```powershell
   cmake -G "MinGW Makefiles" -B build
   ```

3. **Compile the Workstation**:
   Compile the source code to produce the optimized executable and bundle assets:
   ```powershell
   cmake --build build
   ```
   This generates `AeroTerminal.exe` and places it in `build/` alongside `WebView2Loader.dll`.

4. **Launch the Application**:
   Always launch the application from the repository root (or let the auto-resolution handle it) to load assets:
   ```powershell
   .\build\AeroTerminal.exe
   ```

---

## Roadmap

| Timeline | Milestone |
|----------|-----------|
| **Phase 1 (Completed)** | C++/OpenGL viewport rendering, ImGui layout structure, offline tldraw whiteboard canvas, SQLite schema. |
| **Phase 2 (In Progress)** | Jupyter Lab integration routing, broker REST/WebSocket APIs (Binance, Tradier), DPAPI encryption for credentials. |
| **Phase 3 (Q3 2026)** | Offline AI assistance with local LLM integration via Pybind11/llama.cpp, customizable indicator scripting. |
| **Future** | Advanced options chains analytics, multi-broker order routing engine, cloud-sync for whiteboards. |

---

## License

Cloning, forking, or modifying this repository does not grant commercial rights. A paid Commercial License is required for any business, hedge fund, proprietary trading group, or internal company use. See [docs/COMMERCIAL_LICENSE.md](docs/COMMERCIAL_LICENSE.md) or contact licensing for details.

**Dual Licensed: AGPL-3.0 (Open Source) + Commercial License**

| License Type | Permitted Uses | Restriction Details |
|--------------|----------------|---------------------|
| **Free (AGPL-3.0)** | Personal trading, individual learning, academic research, open-source contributions. | Code modifications must remain open source under the AGPL-3.0 license. |
| **Commercial License** | Startups, hedge funds, prop desks, asset managers, SaaS offerings, and business-level white-labeling. | Requires a paid subscription. Contact **licensing@aeroterminal.com** for inquiries. |

**Trademarks:** "AeroTerminal" and the logo are trademarks of AeroTerminal. Rebranding or renaming in a fork does not extinguish the commercial licensing requirements under our dual-licensing framework.
