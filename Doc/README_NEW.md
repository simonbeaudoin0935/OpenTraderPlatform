# L2Trader Documentation

Welcome to the L2Trader documentation! This directory contains comprehensive guides for understanding, developing, and contributing to L2Trader.

## Documentation Index

### Getting Started

**New to L2Trader?** Start here:

1. **[../README.md](../README.md)** - Project overview, features, and quick start
2. **[ARCHITECTURE.md](ARCHITECTURE.md)** - System architecture and design patterns
3. **[DEVELOPMENT.md](DEVELOPMENT.md)** - Development environment setup and coding guidelines

### Core Documentation

#### Architecture & Design

**[ARCHITECTURE.md](ARCHITECTURE.md)** - Comprehensive system architecture documentation

- System architecture diagrams
- Core components (MainApp, TSClient, MainAlgo, StockInstruments, BarCache)
- Threading model and patterns
- Memory management strategies
- Data flow diagrams
- Design patterns used throughout the codebase

**Key Topics**:
- MVC pattern implementation
- Singleton usage (TSClient, MainAlgo)
- Thread-safe communication via Qt signals/slots
- Composition over pointers philosophy
- Smart pointer usage guidelines

#### Authentication & Security

**[AUTHENTICATION.md](AUTHENTICATION.md)** - OAuth 2.0 authentication and security

- OAuth 2.0 Authorization Code Flow
- Token management (access tokens, refresh tokens)
- Automatic token refresh system
- Secure storage (QKeychain integration)
- GUI vs TUI authentication modes
- Security best practices

**Key Topics**:
- TradeStation API OAuth implementation
- CSRF protection
- Platform-specific secure storage (GNOME Keyring, macOS Keychain, Windows Credential Manager)
- Token expiration and refresh scheduling

#### Frontend Implementation

**[FRONTEND.md](FRONTEND.md)** - GUI and TUI frontend architectures

- Frontend abstraction layer
- **GUI Implementation**:
  - Qt Widgets-based interface
  - StockPriceChart (QCustomPlot)
  - Market depth tables
  - Order and position management
  - Keyboard shortcuts
- **TUI Implementation**:
  - ncurses-based terminal interface
  - Headless mode support
  - Order and position monitoring
- Component interaction diagrams

**Key Topics**:
- Dual frontend strategy
- Bidirectional index system for chart performance
- Real-time data visualization
- Session background rendering

### Development

#### Development Guide

**[DEVELOPMENT.md](DEVELOPMENT.md)** - Complete development guide

- **Setup**: Prerequisites, installation, IDE configuration
- **Building**: Build commands, configurations, optimization
- **Testing**: Unit tests, GUI integration tests, test writing
- **Code Quality**: Sanitizers (UBSan, ASan), formatting, compiler warnings
- **Coding Guidelines**: Style, naming, patterns, best practices
- **Constants & SQL**: Management strategies

**Essential for**:
- New contributors
- Setting up development environment
- Understanding build system
- Writing quality code

#### Contributing

**[CONTRIBUTING.md](CONTRIBUTING.md)** - Contribution guidelines

- Getting started
- Development workflow
- Code standards and formatting
- Testing requirements
- Pull request process
- Documentation guidelines
- Code of conduct

**Essential for**:
- All contributors
- Understanding Git workflow
- Meeting code standards
- Submitting pull requests

## Quick Reference

### Architecture Diagrams

All diagrams use [Mermaid](https://mermaid.js.org/) syntax and are viewable in GitHub, VSCode (with extension), and can be exported to images.

**Main Diagrams**:
- System Architecture → [ARCHITECTURE.md](ARCHITECTURE.md#system-architecture)
- Program Startup Sequence → [ARCHITECTURE.md](ARCHITECTURE.md#program-startup-sequence)
- OAuth Flow → [AUTHENTICATION.md](AUTHENTICATION.md#initial-authentication-flow)
- Token Refresh → [AUTHENTICATION.md](AUTHENTICATION.md#token-refresh-flow)
- GUI Component Hierarchy → [FRONTEND.md](FRONTEND.md#component-hierarchy)
- Market Data Pipeline → [ARCHITECTURE.md](ARCHITECTURE.md#market-data-pipeline)

### Common Tasks

| Task | Documentation |
|------|---------------|
| **Set up development environment** | [DEVELOPMENT.md § Setup](DEVELOPMENT.md#development-setup) |
| **Build the project** | [DEVELOPMENT.md § Building](DEVELOPMENT.md#building) |
| **Run tests** | [DEVELOPMENT.md § Testing](DEVELOPMENT.md#testing) |
| **Understand architecture** | [ARCHITECTURE.md](ARCHITECTURE.md) |
| **OAuth authentication** | [AUTHENTICATION.md](AUTHENTICATION.md) |
| **GUI components** | [FRONTEND.md § GUI](FRONTEND.md#gui-implementation) |
| **Code formatting** | [DEVELOPMENT.md § Code Quality](DEVELOPMENT.md#code-quality-tools) |
| **Submit changes** | [CONTRIBUTING.md § PR Process](CONTRIBUTING.md#pull-request-process) |
| **Memory management** | [ARCHITECTURE.md § Memory](ARCHITECTURE.md#memory-management) |
| **Threading patterns** | [ARCHITECTURE.md § Threading](ARCHITECTURE.md#threading-model) |

### Key Concepts

#### Design Philosophy

L2Trader follows these core principles:

1. **Composition Over Inheritance**: Prefer direct member objects over pointers
2. **Explicit Dependencies**: Use dependency injection, avoid hidden singletons where possible
3. **Thread Safety**: Qt signal/slot for cross-thread communication
4. **Smart Pointers**: Consistent memory management patterns
5. **Early Return**: Minimize nesting by handling errors first

#### Threading Model

- **Main Thread**: GUI/TUI event loop
- **TSClient Thread**: Network operations, OAuth
- **MainAlgo Thread**: Trading logic, bar processing
- **Database Threads**: SQLite operations (per BarCache)

Communication via **Qt signal/slot** with automatic queuing for thread safety.

#### Memory Management Hierarchy

1. **Composition** (preferred): Direct member objects
2. **Qt Parent-Child**: For QObjects with available parent
3. **std::unique_ptr**: Exclusive ownership
4. **std::shared_ptr**: Shared ownership (Bar vectors)
5. **QPointer**: Observing Qt objects with uncertain lifetime

## Documentation Standards

### When Adding Documentation

1. **Use Mermaid** for diagrams where appropriate
2. **Include table of contents** for longer documents
3. **Cross-reference** related documentation
4. **Update this index** when adding new files
5. **Keep diagrams up-to-date** with code changes

### Documentation Structure

```
Doc/
├── README.md              # This file - documentation index
├── ARCHITECTURE.md        # System architecture
├── AUTHENTICATION.md      # OAuth and security
├── FRONTEND.md            # GUI and TUI
├── DEVELOPMENT.md         # Development guide
└── CONTRIBUTING.md        # Contribution guidelines
```

### Style Guidelines

- **Headers**: Use ATX-style (`#`, `##`, `###`)
- **Code blocks**: Specify language for syntax highlighting
- **Lists**: Use `-` for unordered, `1.` for ordered
- **Links**: Use relative paths for internal docs
- **Emphasis**: `**bold**` for important, `*italic*` for emphasis
- **Tables**: Use for structured data comparison

## Recent Updates

### January 2026

- ✅ Consolidated documentation into 5 main files
- ✅ Removed obsolete migration and implementation notes
- ✅ Updated all architecture diagrams
- ✅ Added comprehensive threading and memory management docs
- ✅ Expanded authentication documentation with GUI/TUI modes
- ✅ Created unified frontend documentation

### Key Improvements

- **Better Organization**: Clear structure with focused documents
- **Comprehensive Coverage**: All major topics covered in depth
- **Cross-Referencing**: Easy navigation between related topics
- **Up-to-Date**: All content reflects current codebase
- **Consolidated**: Reduced from 29 files to 5 core documents

## Contributing to Documentation

Documentation improvements are always welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Documentation Issues

Found outdated or incorrect information? Please:

1. Open an issue describing the problem
2. Reference the specific file and section
3. Suggest corrections if possible

### Documentation Pull Requests

When updating documentation:

- Keep language clear and concise
- Update diagrams if architecture changes
- Maintain consistent formatting
- Update this index if adding new sections

## Getting Help

- **Issues**: https://github.com/simonbeaudoin0935/L2Trader/issues
- **Discussions**: https://github.com/simonbeaudoin0935/L2Trader/discussions

---

**Note**: This documentation describes the current state of L2Trader. Some features may be under active development. Check the main README and issue tracker for the latest information on feature availability.
