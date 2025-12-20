# L2Trader Documentation

This directory contains comprehensive documentation for the L2Trader application, including architecture diagrams, sequence flows, and detailed component analysis.

## Documentation Index

### Application Architecture
- **[Application_Architecture_Diagram.md](Application_Architecture_Diagram.md)** - High-level overview of application components and signal/slot connections
  - Updated: Reflects current architecture with TSClient singleton, OrdersReceiver, BalanceWindow, OrderEntryWidget, OrderWindow
  - Removed: FMPClient, StockScreener, BreakingNewsFetcher (not in current codebase)
- **[GUI_Architecture.md](GUI_Architecture.md)** - GUI component organization and layout
  - Updated: Includes all current widgets (BalanceWindow, OrderWindow, OrderEntryWidget, RecorderTab)
  - Uses qcustomplot for StockPriceChart (Qt Charts removed)
- **[ProgramSequence.md](ProgramSequence.md)** - Application startup and initialization sequence
  - Updated: Reflects actual MainApp flow with OAuth authentication and thread initialization

### Authentication
- **[OAuth_Authentication_Process.md](OAuth_Authentication_Process.md)** - Detailed flow of TradeStation OAuth authentication
  - Comprehensive documentation of OAuth 2.0 implementation with secure storage

### StockPriceChart Component (qcustomplot-based)
- **[StockPriceChart_Diagram.md](StockPriceChart_Diagram.md)** - Class diagrams for the chart component
  - Updated: Uses qcustomplot classes (QCustomPlot, QCPFinancial, QCPBars, QCPItemLine, QCPItemRect)
- **[StockPriceChart_Architecture.md](StockPriceChart_Architecture.md)** - ⭐ **Comprehensive documentation**
  - ⚠️ **Note**: Contains some outdated Qt Charts references - core architecture concepts remain valid but implementation details reference old Qt Charts API. Use in conjunction with updated StockPriceChart_Diagram.md and StockPriceChart_BarReception.md for current implementation.
  - Includes: Detailed architecture, data flow diagrams, event handling, state management, missing bars detection
- **[StockPriceChart_BarReception.md](StockPriceChart_BarReception.md)** - Bar reception and processing flow
  - Updated: Reflects qcustomplot implementation with addLiveBar() and index-based system
- **[BidirectionalIndexSystem_Implementation.md](BidirectionalIndexSystem_Implementation.md)** - Index system details
  - Bidirectional index system for efficient historical data loading

### Migration Documentation (Historical)
- **[qcustomplot-migration.md](qcustomplot-migration.md)** - ✅ Migration from Qt Charts to qcustomplot (COMPLETED)
- **[MIGRATION_SUMMARY.md](MIGRATION_SUMMARY.md)** - ✅ Summary of qcustomplot migration (COMPLETED)

### Feature Documentation
- **[RecorderTab_CSV_Input_Widget.md](RecorderTab_CSV_Input_Widget.md)** - CSV file input widget in Recorder tab
- **[Thread_Relations_TSClient.md](Thread_Relations_TSClient.md)** - Thread relationships for TSClient, async requests, and BarCache

## Using the Documentation

### For General Understanding
Start with:
1. Application_Architecture_Diagram.md - Get the big picture of current architecture
2. GUI_Architecture.md - Understand the UI structure (all widgets included)
3. ProgramSequence.md - See how the app starts up with OAuth and threads

### For StockPriceChart Development
- **Architecture**: Start with StockPriceChart_Architecture.md for comprehensive overview
- **Class Structure**: See StockPriceChart_Diagram.md for class diagrams (qcustomplot-based)
- **Bar Processing**: See StockPriceChart_BarReception.md for data flow
- **Index System**: See BidirectionalIndexSystem_Implementation.md for performance details

### For Debugging
- **Chart Issues**: See StockPriceChart_Architecture.md (needs update for qcustomplot specifics)
- **Authentication Issues**: See OAuth_Authentication_Process.md
- **Thread Issues**: See Thread_Relations_TSClient.md

### For Development
- Review the architecture diagram relevant to your component
- All chart components now use qcustomplot library (not Qt Charts)
- Check migration docs for historical context on Qt Charts → qcustomplot transition

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
5. **Important**: When documenting chart components, note that the application uses qcustomplot, not Qt Charts

## Recent Updates (December 2024)

- ✅ Updated all architecture diagrams to reflect current codebase
- ✅ Removed obsolete component references (FMPClient, StockScreener, BreakingNewsFetcher)
- ✅ Added missing components (OrdersReceiver, BalanceWindow, OrderWindow, OrderEntryWidget, RecorderTab)
- ✅ Updated all StockPriceChart documentation for qcustomplot (removed Qt Charts references)
- ✅ Marked qcustomplot migration as complete and operational
- ✅ Updated ProgramSequence with actual startup flow including OAuth and threading details
