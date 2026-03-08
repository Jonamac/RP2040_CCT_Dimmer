![Platform](https://img.shields.io/badge/platform-RP2040-blue)
![Language](https://img.shields.io/badge/language-C++-brightgreen)
![Status](https://img.shields.io/badge/status-in_development-orange)
![License](https://img.shields.io/badge/license-MIT-lightgrey)
# RP2040_CCT_Dimmer
RP2040 ZERO CCT PWM Dimmer with display

# RP2040 CCT Dimmer (Work in Progress)

This project is an in‑development firmware and hardware design for a custom 
RP2040‑based CCT (Correlated Color Temperature) LED dimmer. The goal is to 
create a professional‑grade lighting controller with smooth perceptual dimming, 
accurate CCT mixing, multiple operating modes, and a clean user interface.

## ⚠️ Project Status: Under Construction
This repository is currently in an active development phase.  
Features, behavior, and file structure are all subject to change as the system 
is refined, stabilized, and expanded.

A number of modes and subsystems are being redesigned, including:
- Replacement of legacy OVERRIDE modes with a new **FREQ** (strobe/frequency) mode
- Addition of a **CAL** (calibration/diagnostic) mode
- Cleanup of NORMAL, DUMB, STANDBY, and DEMO transitions
- General codebase stabilization and refactoring

Expect rapid iteration and breaking changes.

## 🤝 About AI Assistance
This project is being developed collaboratively with the help of **Microsoft Copilot**, 
used as a technical assistant for:
- Architectural planning  
- Code review and refactoring  
- Debugging and troubleshooting  
- Feature design and documentation  

All final decisions, testing, and hardware integration are performed manually.

## 📁 Repository Structure
The project uses a multi‑file Arduino‑style layout, including:
- `RP2040_CCT_Dimmer.ino` — main entry point  
- `modes.cpp/.h` — mode state machine  
- `pots.cpp/.h` — analog input handling  
- `ledmix.cpp/.h` — LED mixing and PWM logic  
- `display_ui.cpp/.h` — OLED UI  
- `state.cpp/.h` — global state and configuration  
- `freq_mode.cpp/.h` — (new) frequency table for upcoming FREQ mode  

## 🛠️ Hardware
The firmware targets a custom PCB built around:
- Raspberry Pi RP2040 (RP2040 ZERO)
- Dual‑channel LED PWM outputs  
- Two analog pots (brightness + CCT)  
- OLED display  
- Buzzer  
- Optional thermistor and fan support (future)

## 📌 Notes
This README is temporary and will be replaced with full documentation once the 
core system is stable.
