# L2Trader Documentation

This directory contains comprehensive documentation for the L2Trader application, including architecture diagrams, sequence flows, and detailed component analysis.

## Documentation Index

### Application Architecture
- **[Application_Architecture_Diagram.md](Application_Architecture_Diagram.md)** - High-level overview of the application structure and components
- **[GUI_Architecture.md](GUI_Architecture.md)** - GUI component organization and layout
- **[ProgramSequence.md](ProgramSequence.md)** - Application startup and initialization sequence

### Authentication
- **[OAuth_Authentication_Process.md](OAuth_Authentication_Process.md)** - Detailed flow of TradeStation OAuth authentication

### StockPriceChart Component
- **[StockPriceChart_Diagram.md](StockPriceChart_Diagram.md)** - Basic class diagram for the chart component
- **[StockPriceChart_Architecture.md](StockPriceChart_Architecture.md)** - ⭐ **Comprehensive documentation** including:
  - Detailed architecture and class relationships
  - Complete data flow diagrams
  - Event handling for zoom/pan/mouse interactions
  - State management and view preservation
  - Missing bars detection and loading
  - **Crash point analysis** with debugging recommendations
  - Code improvement suggestions

## Using the Documentation

### For General Understanding
Start with:
1. Application_Architecture_Diagram.md - Get the big picture
2. GUI_Architecture.md - Understand the UI structure
3. ProgramSequence.md - See how the app starts up

### For Debugging
- **Chart Crashes**: See [StockPriceChart_Architecture.md](StockPriceChart_Architecture.md) Section 8 "Potential Crash Points"
- **Authentication Issues**: See [OAuth_Authentication_Process.md](OAuth_Authentication_Process.md)

### For Development
- Review the architecture diagram relevant to your component
- Check the StockPriceChart_Architecture.md for examples of comprehensive documentation style

## Diagram Format

All diagrams in this directory use [Mermaid](https://mermaid.js.org/) syntax, which is:
- Rendered automatically in GitHub
- Viewable in VS Code with Mermaid extensions
- Convertible to images using various tools

## Contributing Documentation

When adding new documentation:
1. Use Mermaid diagrams where appropriate
2. Include a table of contents for longer documents
3. Provide debugging recommendations for complex components
4. Update this README with links to new documentation
