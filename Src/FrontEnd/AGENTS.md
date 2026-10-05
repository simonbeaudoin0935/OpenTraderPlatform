# FrontEnd/ Directory - UI Architecture - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/`
**Purpose**: Main-thread UI surface for OpenTraderPlatform.
**Implementation**: GUI-only (`GUIFrontend`, Qt Widgets).

## Current Architecture

- `GUIFrontend` (`Src/FrontEnd/GUI/GUIFrontend.*`) is the concrete frontend object.
- `MainApp` owns `GUIFrontend` directly (no frontend abstraction layer).
- High-frequency market data is pull-based: GUI polls `DisplaySnapshot` at ~30 Hz.
- Account/order/position/auth/data-usage notifications are still pushed through Qt signals.

## Directory Structure

- `GUI/`: Qt Widgets frontend components (main window, tabs, chart, widgets).

## Data Flow

### Backend -> Frontend

- `TSClient::authStateChanged` -> `GUIFrontend::tradeStationAuthStateChanged`.
- `TSClient::totalDataReceivedBytesIncreased` -> `GUIFrontend::onTSClientDataUsageUpdate`.
- `DBClient::dataUsageUpdated` -> `GUIFrontend::onDBClientDataUsageUpdate`.
- `MainAlgo` order/position/balance/account signals -> corresponding `GUIFrontend` slots.
- Chart/Level2/trade render state is pulled by `GUIFrontend::onDisplayRefreshTick()` from `DisplaySnapshot`.

### Frontend -> Backend

- Symbol selection, order placement, replay controls, and strategy interactions are forwarded from GUI widgets through `GUIFrontend` to `MainAlgo`/`MainApp`/clients using queued signals or `QMetaObject::invokeMethod` as appropriate.

## Implementation Notes

- Keep all QWidget/UI mutations on the main thread.
- Preserve pull-based chart refresh behavior; do not reintroduce per-tick cross-thread GUI update storms.
- For new user actions, emit `INPT` logs through centralized logging helpers.
- Prefer adding behavior in GUI subcomponents under `Src/FrontEnd/GUI/` instead of expanding `MainApp`.

## Related Docs

- `Src/FrontEnd/GUI/AGENTS.md`
- `Doc/FRONTEND.md`
- `Src/Core/AGENTS.md`
- `Src/Algo/AGENTS.md`
