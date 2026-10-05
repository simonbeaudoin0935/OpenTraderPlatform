# OpenTraderPlatform Documentation

Welcome to the OpenTraderPlatform documentation. This directory contains comprehensive guides for understanding, developing, and contributing to OpenTraderPlatform.

## Documentation Index

### Getting Started

**New to OpenTraderPlatform?** Start here:

1. **[../README.md](../README.md)** — Project overview, features, and quick start
2. **[ARCHITECTURE.md](ARCHITECTURE.md)** — System architecture and design patterns
3. **[DEVELOPMENT.md](DEVELOPMENT.md)** — Development environment setup and coding guidelines

---

### Core Documentation

#### Architecture & Design

**[ARCHITECTURE.md](ARCHITECTURE.md)** — Comprehensive system architecture

Key topics:
- Dual-API design: Databento (market data) + TradeStation (brokerage)
- Core singletons: DBClient, TSClient, MainAlgo
- Threading model (Databento callback thread, TSClient thread, MainAlgo thread)
- Replay system: ReplayEngine, OrderEmulator, LiveBarAccumulator
- Data flow diagrams (live market data, historical bars, order entry, replay)
- Design patterns (singleton, observer, repository, mock/interceptor)
- Memory management: composition first, smart pointer guidelines, bar class optimization

#### Authentication & Security

**[AUTHENTICATION.md](AUTHENTICATION.md)** — Authentication and credential management

Key topics:
- TradeStation OAuth 2.0 Authorization Code Flow
- Databento API key setup and storage
- Automatic token refresh (20-minute TradeStation tokens, refreshed 5 s early)
- Credential storage requires Qt6Keychain and a working native OS keyring; insecure fallback is disabled
- Security best practices (no committed credentials, minimal scopes, CSRF protection)

#### Frontend Implementation


Key topics:
- GUI frontend architecture and data flow
- GUI main window layout: StockPriceChart, Level2Widget, OrderEntryWidget, tabs, dock widgets
- MarketFlags labels (HALTED, HTB) driven by Databento StatusMsg records
- ChartToolbar with replay play/pause and speed controls
- Order visualization on chart (buy/sell markers, position lines, P&L box)
- Bidirectional index system for chart performance
- Data flow diagrams (backend → frontend, user actions → backend)

#### Risk Management

**[RISK_MANAGEMENT.md](RISK_MANAGEMENT.md)** — Risk engine architecture and runtime behavior

Key topics:
- Account-scoped limits and drawdown basis formulas
- Runtime lock/cooldown semantics and daily rollover
- Order-evaluation gate path shared by GUI and strategy orders
- Live/sim/replay persistence model (including replay restart behavior)
- GUI surfaces: Risk Management tab and compact risk status widget
- Strategy manual-confirm preview bracket + hard mute mode (`M`)

#### Strategy System

**[STRATEGY.md](STRATEGY.md)** — Trading strategy process system (technical)

Key topics:
- Strategy process architecture and supervision
- External strategy lifecycle (LOADED → RUNNING → STOPPED / ERROR)
- Strategy SDK/runtime capabilities (historical bars, symbol claiming, order placement)
- StrategyCard GUI and StrategyQuickView
- Development guide (SDK usage, self-description, samples)
- Known limitations

**[STRATEGY_GUIDE.md](STRATEGY_GUIDE.md)** — Non-technical guide for strategy developers

For users who want to write trading strategies without deep C++ or systems knowledge:
- How the application works at a conceptual level
- What market data is available and when callbacks fire
- How to write a basic strategy step by step
- How to use the GUI to load, start, and monitor strategies
- Replay mode for testing strategies against historical data

### Development

#### Development Guide

**[DEVELOPMENT.md](DEVELOPMENT.md)** — Complete development guide

Key topics:
- Prerequisites: Qt 6.4.2+, C++23, CMake, libssl-dev, libzstd-dev, databento-cpp
- Testing: unit tests, GUI integration tests
- Code quality: UBSan, ASan, clang-format, compiler warnings
- Coding guidelines: naming, ASSUME macros, signal/slot, memory management
- Constants & SQL centralization

#### Contributing

**[CONTRIBUTING.md](CONTRIBUTING.md)** — Contribution guidelines

Key topics:
- Development workflow (fork, branch, PR)
- Code standards and formatting
- Testing requirements
- Pull request process
- Documentation guidelines

#### Destructor & Threading Guidelines

**[Destructor_Guidelines.md](Destructor_Guidelines.md)** — Safe cleanup patterns for Qt threaded objects

Key topics:
- Stop threads before destroying thread-owned objects
- deleteLater() vs. direct delete
- Cross-thread QObject deletion
- Lessons learned from real threading bugs

---

## Quick Reference

### Architecture Diagrams

All diagrams use [Mermaid](https://mermaid.js.org/) syntax (viewable on GitHub, VSCode with extension).

| Diagram | Location |
|---------|----------|
| System architecture | [ARCHITECTURE.md § System Architecture](ARCHITECTURE.md#system-architecture) |
| Program startup sequence | [ARCHITECTURE.md § Program Startup Sequence](ARCHITECTURE.md#program-startup-sequence) |
| Live market data pipeline | [ARCHITECTURE.md § Live Market Data Pipeline](ARCHITECTURE.md#live-market-data-pipeline) |
| Historical bar request flow | [ARCHITECTURE.md § Historical Bar Request Flow](ARCHITECTURE.md#historical-bar-request-flow) |
| Order entry flow | [ARCHITECTURE.md § Order Entry Flow](ARCHITECTURE.md#order-entry-flow) |
| Replay data flow | [ARCHITECTURE.md § Replay Data Flow](ARCHITECTURE.md#replay-data-flow) |
| TradeStation OAuth flow | [AUTHENTICATION.md § Initial Authentication Flow](AUTHENTICATION.md#initial-authentication-flow) |
| Token refresh flow | [AUTHENTICATION.md § Token Refresh Flow](AUTHENTICATION.md#token-refresh-flow) |

### Common Tasks

| Task | Documentation |
|------|---------------|
| Set up development environment | [DEVELOPMENT.md § Setup](DEVELOPMENT.md#development-setup) |
| Build the project | [DEVELOPMENT.md § Building](DEVELOPMENT.md#building) |
| Run tests | [DEVELOPMENT.md § Testing](DEVELOPMENT.md#testing) |
| Understand the overall architecture | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Configure Databento API key | [AUTHENTICATION.md § Databento API Key](AUTHENTICATION.md#databento-api-key) |
| Configure TradeStation OAuth | [AUTHENTICATION.md § TradeStation OAuth 2.0](AUTHENTICATION.md#tradestation-oauth-20) |
| Understand GUI components | [FRONTEND.md](FRONTEND.md) |
| Understand risk engine formulas and persistence | [RISK_MANAGEMENT.md](RISK_MANAGEMENT.md) |
| Develop a trading strategy (technical) | [STRATEGY.md § Development Guide](STRATEGY.md#development-guide) |
| Develop a trading strategy (beginner) | [STRATEGY_GUIDE.md](STRATEGY_GUIDE.md) |
| Add code formatting | [DEVELOPMENT.md § Code Quality](DEVELOPMENT.md#code-quality-tools) |
| Submit changes | [CONTRIBUTING.md](CONTRIBUTING.md) |
| Memory management patterns | [ARCHITECTURE.md § Memory Management](ARCHITECTURE.md#memory-management) |
| Threading patterns | [ARCHITECTURE.md § Threading Model](ARCHITECTURE.md#threading-model) |

### Key Concepts

#### Dual-API Architecture

OpenTraderPlatform uses two separate services:
- **Databento** — all market data (Level 2, trades, bars, replay archives)
- **TradeStation** — all brokerage (OAuth, orders, positions, accounts)

#### Bar Timestamp Convention

Bars use **open-time** (Databento native):
- A bar covering 4:00:00–4:00:59 is timestamped `4:00:00`
- Trading day: 900 bars, index 0 = `4:00 AM`, index 899 = `6:59 PM`

#### Trading Modes

| Mode | Market Data | Order Execution | Ledger |
|------|-------------|----------------|--------|
| **Live** | Databento live stream | TradeStation real API | `Ledgers/Live/Ledger.db` |
| **Simulation** | Databento live stream | TradeStation sim API (`sim-api`) | `Ledgers/Simulation/Ledger.db` |
| **Replay** | Local `.dbn.zst` files | OrderEmulator (local) | `Ledgers/Replay/Ledger_YYYY-MM-DD_HHMMSS.db` |
| **Review** | None (ledger-only inspection) | Disabled / read-only | `Ledgers/Replay/Ledger_YYYY-MM-DD_HHMMSS.db` |

#### Threading Summary

| Thread | Owner | What Runs There |
|--------|-------|----------------|
| Main | QApplication | GUI event loop, FrontEnd, MainApp |
| Databento internal | DBClient (LiveThreaded) | Level2/Trade/Status callbacks |
| QThreadPool workers | DBClient | Historical fetches, replay downloads |
| TSClient worker | TSClient | REST requests, OAuth, order/position streams |
| MainAlgo worker | MainAlgo | Trading logic, bar processing, replay engine |
| Database | DatabaseThread (per BarCache) | SQLite read/write |

---

## Documentation Standards

### When Adding Documentation

1. Use **Mermaid** for diagrams
2. Include a **table of contents** for longer documents
3. **Cross-reference** related documentation
4. **Update this index** when adding new files
5. Keep content **accurate** — prefer brevity over completeness when trade-offs arise

### Directory Structure

```
Doc/
├── README.md                  # This file — documentation index
├── ARCHITECTURE.md            # System architecture
├── AUTHENTICATION.md          # TradeStation OAuth + Databento API key
├── RISK_MANAGEMENT.md         # Risk engine, limits, formulas, and persistence
├── STRATEGY.md                # Strategy process runtime (technical)
├── STRATEGY_GUIDE.md          # Strategy development (non-technical)
├── DEVELOPMENT.md             # Development guide
├── CONTRIBUTING.md            # Contribution guidelines
└── Destructor_Guidelines.md   # Qt threading cleanup patterns
```

### Style Guidelines

- **Headers**: ATX-style (`#`, `##`, `###`)
- **Code blocks**: Specify language for syntax highlighting
- **Lists**: `-` for unordered, `1.` for ordered
- **Links**: Relative paths for internal docs
- **Emphasis**: `**bold**` for important, `*italic*` for emphasis
- **Tables**: For structured comparisons

---

## Getting Help

- **Issues**: https://github.com/simonbeaudoin0935/OpenTraderPlatform/issues
- **Discussions**: https://github.com/simonbeaudoin0935/OpenTraderPlatform/discussions
