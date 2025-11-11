```mermaid
classDiagram
    class AppFrontend {
        <<interface>>
        +onFMPClientDataUsageUpdate(qsizetype)*
        +onTSClientDataUsageUpdate(qsizetype)*
        +onTradeStationAccountsReceived(QVector<Account>)*
        +onMemoryUsageUpdate(qsizetype)*
        +onCurrentHighlightedStockBarReceived(QString, Bar)*
        +onCurrentHighlightedReceivedNewMarketDepthQuote(QString, MarketDepthQuote, double, double, double)*
        +onNewPositionReceived(QString, Position)*
        +onRequestedMissingBarsDisplayedStockReceived(QVector<Bar>)*
    }

    class GuiFrontend {
        -MainAlgo* mainAlgo
        -Ui::GuiFrontend* ui
        -QPushButton* tradeStationLoginButton
        +explicit GuiFrontend(MainAlgo*, QObject*)
        +~GuiFrontend()
        +onFMPClientDataUsageUpdate(qsizetype) override
        +onTSClientDataUsageUpdate(qsizetype) override
        +onTradeStationAccountsReceived(QVector<Account>) override
        +onMemoryUsageUpdate(qsizetype) override
        +onCurrentHighlightedStockBarReceived(QString, Bar) override
        +onCurrentHighlightedReceivedNewMarketDepthQuote(QString, MarketDepthQuote, double, double, double) override
        +onNewPositionReceived(QString, Position) override
        +onRequestedMissingBarsDisplayedStockReceived(QVector<Bar>) override
        -onTradeStationLoginClicked()
        -onTradeStationAuthStateChanged(bool, QString)
        -onNewDisplayedStockSelection()
        -setupDarkTheme(QMainWindow*)
    }

    class StockPriceChart {
        -QChart* chart
        -QCandlestickSeries* candlestickSeries
        -QLineSeries* lastPriceLine
        -QScatterSeries* voidBarSeries
        -QMap<QDateTime, Bar> completedBars
        +explicit StockPriceChart(QWidget*)
        +~StockPriceChart()
        +setSymbol(QString)
        +clearSymbol()
        +addBar(Bar)
        +onRequestedMissingBarsReceived(QVector<Bar>)
        +requestMissingBars(QDateTime, QDateTime)*
        -updateChart()
        -handleClosedBar(Bar)
        -handleOpenBar(Bar)
        -updateLastPriceLine(double, bool)
        -maintainBarLimit()
        -checkForMissingBars(QDateTime, QDateTime)
    }

    class MarketDepthTable {
        -MarketDepthTableView* tableView
        -QStandardItemModel* model
        -QLabel* bidLabel
        -QLabel* askLabel
        -QLabel* spreadLabel
        -QLabel* dwpLabel
        +explicit MarketDepthTable(QWidget*)
        +~MarketDepthTable()
        +updateData(QVector<MarketDepthLevel>, QVector<MarketDepthLevel>)
        +updateDWP(double, double)
        -setupUI()
        -setupStyles()
    }

    class MarketDepthTableView {
        +explicit MarketDepthTableView(QWidget*)
        +setTopMargin(int)
    }

    class PositionWindow {
        -QTableView* tableView
        -QStandardItemModel* model
        -QMap<QString, int> positionRowMap
        +explicit PositionWindow(QWidget*)
        +~PositionWindow()
        +updatePosition(QString, Position)
        +symbolClicked(QString)*
        -setupUI()
        -setupStyles()
        -updatePositionRow(QString, Position)
        -onSymbolClicked(QModelIndex)
    }

    class Gauge {
        -double m_value
        -double m_minValue
        -double m_maxValue
        -QString m_label
        +explicit Gauge(QWidget*)
        +explicit Gauge(QString, QWidget*)
        +setRange(double, double)
        +setValue(double)
        +setLabel(QString)
        +label() const
        +valueChanged(double)*
        -paintEvent(QPaintEvent*) override
        -drawBackground(QPainter&)
        -drawBar(QPainter&)
        -drawIndicator(QPainter&)
        -drawTicks(QPainter&)
        -drawCenterLogo(QPainter&)
    }

    class CacheTab {
        -QTableWidget* cacheTable
        -QPushButton* refreshButton
        -QPushButton* clearSelectedButton
        -QPushButton* clearAllButton
        -QTimer* refreshTimer
        +explicit CacheTab(QWidget*)
        +~CacheTab() = default
        -refreshCacheInfo()
        -clearSelectedCache()
        -clearAllCache()
        -setupUI()
        -populateCacheTable()
        -formatFileSize(qint64) const
    }

    class LoggingTab {
        -QVBoxLayout* categoryCheckBoxLayout
        -QTextEdit* liveLogDisplay
        -QMap<QString, QCheckBox*> categoryCheckBoxes
        +explicit LoggingTab(QWidget*)
        +~LoggingTab() = default
        +updateLiveLogDisplay(QString)
        -onCategoryCheckBoxToggled(bool)
        -setupUI()
        -populateCategoryCheckboxes()
    }

    %% Relationships
    AppFrontend <|-- GuiFrontend : implements
    GuiFrontend --> StockPriceChart : contains
    GuiFrontend --> MarketDepthTable : contains
    GuiFrontend --> PositionWindow : contains
    GuiFrontend --> Gauge : contains (4 instances)
    GuiFrontend --> CacheTab : manages
    GuiFrontend --> LoggingTab : manages

    MarketDepthTable --> MarketDepthTableView : uses
    MarketDepthTableView --> QTableView : extends

    StockPriceChart --> QChartView : uses
    StockPriceChart --> QCandlestickSeries : uses
    StockPriceChart --> QLineSeries : uses
    StockPriceChart --> QScatterSeries : uses

    PositionWindow --> QTableView : uses
    PositionWindow --> QStandardItemModel : uses

    CacheTab --> QTableWidget : uses
    CacheTab --> QTimer : uses

    LoggingTab --> QTextEdit : uses
    LoggingTab --> QCheckBox : uses

    %% Qt Base Classes
    class QTableView {
        <<Qt Widget>>
    }

    class QChartView {
        <<Qt Widget>>
    }

    class QCandlestickSeries {
        <<Qt Charts>>
    }

    class QLineSeries {
        <<Qt Charts>>
    }

    class QScatterSeries {
        <<Qt Charts>>
    }

    class QStandardItemModel {
        <<Qt Model>>
    }

    class QTableWidget {
        <<Qt Widget>>
    }

    class QTimer {
        <<Qt Core>>
    }

    class QTextEdit {
        <<Qt Widget>>
    }

    class QCheckBox {
        <<Qt Widget>>
    }
```

```mermaid
graph TB
    subgraph "Main Window (GuiFrontend)"
        A[GuiFrontend<br/>QMainWindow]
        B[Stock Symbol Input<br/>QLineEdit]
        C[Main Splitter<br/>QSplitter]
    end

    subgraph "Trade Tab"
        D[StockPriceChart<br/>Candlestick Chart]
        E[Gauge Container<br/>QGridLayout]
        F[BAI Gauge]
        G[DWP Gauge]
        H[OBLR Gauge]
        I[QRR Gauge]
        J[MarketDepthTable<br/>Bid/Ask Table]
    end

    subgraph "Settings Tab"
        K[Settings Table<br/>QTableView]
        L[Settings Button<br/>QPushButton]
    end

    subgraph "Bottom Panel"
        M[Log Display<br/>QTextEdit]
        N[PositionWindow<br/>Positions Table]
    end

    subgraph "Status Bar"
        O[TradeStation Login Button<br/>QPushButton]
    end

    subgraph "Menu Bar"
        P[Menu Bar<br/>QMenuBar]
    end

    A --> B
    A --> C
    A --> O
    A --> P

    C --> D
    C --> E
    C --> J
    C --> M
    C --> N

    E --> F
    E --> G
    E --> H
    E --> I

    C --> K
    C --> L

    style A fill:#e1f5fe
    style D fill:#f3e5f5
    style J fill:#e8f5e8
    style N fill:#fff3e0
    style F fill:#fce4ec
    style G fill:#fce4ec
    style H fill:#fce4ec
    style I fill:#fce4ec
```

```mermaid
stateDiagram-v2
    [*] --> GuiFrontend: Application Start
    GuiFrontend --> StockPriceChart: User selects symbol
    GuiFrontend --> MarketDepthTable: Receives market depth data
    GuiFrontend --> PositionWindow: Receives position updates
    GuiFrontend --> Gauge: Receives metric updates (BAI, DWP, OBLR, QRR)

    StockPriceChart --> StockPriceChart: addBar() / onRequestedMissingBarsReceived()
    MarketDepthTable --> MarketDepthTable: updateData() / updateDWP()
    PositionWindow --> PositionWindow: updatePosition()
    Gauge --> Gauge: setValue()

    StockPriceChart --> GuiFrontend: requestMissingBars()
    GuiFrontend --> StockPriceChart: Bars received

    note right of GuiFrontend : Main coordinator receives data\nfrom MainAlgo and updates UI components

    GuiFrontend --> [*]: Application exit
```</content>
<parameter name="filePath">/home/simon/Documents/TradingAlgorithm/src/GUI/GUI_Architecture.md