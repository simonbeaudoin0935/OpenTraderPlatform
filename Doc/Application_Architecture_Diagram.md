# Trading Algorithm Application Architecture

```mermaid
graph TD
    subgraph "Main Application"
        MA[MainApp] -->|connects| TS[TSClient]
        MA -->|connects| FMP[FMPClient]
        MA -->|connects| MAL[MainAlgo]
        MA -->|connects| AF[AppFrontend]
        MA -->|connects| MM[MemoryMonitor]
    end

    subgraph "External Data Sources"
        TS -->|REST API| TS_API[TradeStation API]
        FMP -->|REST API| FMP_API[FMP API]
    end

    subgraph "Algorithm Core"
        MAL --> SI[StockInstruments]
        SI --> BC[BarCache]
        SI --> RUD[RunUpDetector]
        SI --> MDQR[MarketDepthQuoteReceiver]
        MAL --> SS[StockScreener]
        MAL --> BNF[BreakingNewsFetcher]
        MAL --> PR[PositionsReceiver]
    end

    subgraph "GUI Frontend"
        AF --> GFW[GUIFrontend]
        GFW --> SPC[StockPriceChart]
        GFW --> MDT[MarketDepthTable]
        GFW --> CT[CacheTab]
        GFW --> LT[LoggingTab]
        GFW --> PW[PositionWindow]
    end

    %% Signal/Slot Connections
    TS -.->|authStateChanged| AF
    TS -.->|authStateChanged| MAL
    TS -.->|getAccountsAsyncReceived| AF
    TS -.->|totalDataReceivedBytesIncreased| AF
    FMP -.->|totalDataReceivedBytesIncreased| AF
    MM -.->|memoryUsageUpdated| AF

    MAL -.->|displayedStockReceivedNewBar| AF
    MAL -.->|displayedStockReceivedNewMarketDepthQuote| AF
    MAL -.->|receivedNewPosition| AF
    MAL -.->|requestedMissingBarsDisplayedStockReceived| AF

    AF -.->|requestMissingBars| MAL

    %% Internal Algorithm Connections
    SS -.->|finished| MAL
    BNF -.->|newNewsFound| MAL
    PR -.->|receivedNewPosition| MAL
    BC -.->|newBar| RUD
    BC -.->|newBar| MDQR

    %% GUI Component Interactions
    SPC -.->|requestMissingBars| AF
    MDT -.->|data updates| GFW
    CT -.->|cache info| GFW
    LT -.->|logs| GFW
    PW -.->|position data| GFW

    %% Styling
    classDef mainApp fill:#e1f5fe,stroke:#01579b,stroke-width:2px
    classDef clients fill:#f3e5f5,stroke:#4a148c,stroke-width:2px
    classDef algo fill:#e8f5e8,stroke:#1b5e20,stroke-width:2px
    classDef gui fill:#fff3e0,stroke:#e65100,stroke-width:2px
    classDef external fill:#fce4ec,stroke:#880e4f,stroke-width:2px

    class MA mainApp
    class TS,TS_API,FMP,FMP_API clients
    class MAL,SI,BC,RUD,MDQR,SS,BNF,PR algo
    class AF,GFW,SPC,MDT,CT,LT,PW gui
    class MM external
```

## Key Signal/Slot Interactions

### Authentication & Data Flow
- **TSClient** ↔ **MainAlgo**: Authentication state changes trigger algorithm initialization
- **TSClient** ↔ **AppFrontend**: Account data and authentication status updates UI
- **FMPClient** ↔ **AppFrontend**: Financial data usage monitoring

### Market Data Pipeline
- **MainAlgo** → **AppFrontend**: New bars and market depth quotes for display
- **AppFrontend** → **MainAlgo**: Requests for missing historical data
- **StockPriceChart** → **AppFrontend**: Zoom/pan requests trigger data fetching

### Internal Algorithm Communication
- **StockScreener** → **MainAlgo**: Stock selection results
- **BreakingNewsFetcher** → **MainAlgo**: News events for analysis
- **PositionsReceiver** → **MainAlgo**: Real-time position updates
- **BarCache** → **RunUpDetector/MarketDepthQuoteReceiver**: Bar data for analysis

### GUI Updates
- **MemoryMonitor** → **AppFrontend**: System resource monitoring
- **MainAlgo** → **GUIFrontend**: Position updates and market data
- **GUIFrontend** → **UI Components**: Data distribution to charts and tables

## Architecture Overview

The application follows a **Model-View-Controller** pattern with Qt's signal/slot mechanism:

1. **Data Sources** (TSClient, FMPClient): Handle external API communications
2. **Business Logic** (MainAlgo): Processes data and executes trading algorithms
3. **Presentation** (AppFrontend/GUIFrontend): Manages user interface and data visualization
4. **Monitoring** (MemoryMonitor): System resource tracking

All components communicate asynchronously through Qt's signal/slot system, ensuring thread-safe data flow and loose coupling between modules.