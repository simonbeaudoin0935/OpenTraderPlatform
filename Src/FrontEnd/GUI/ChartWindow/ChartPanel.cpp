#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QPointer>
#include <optional>

#include "ChartPanel.h"
#include "StockPriceChart/StockPriceChart.h"
#include "MainAlgo.h"
#include "Misc/Logging/Logging.h"
#include "Logging.h"
#include "Assume.h"
#include "CONSTANTS.h"
#include "LTTng/LTTngTracepoints.h"
#include "OrdersDatabase.h" // For StrategyLogEntry
#include "Order.h"
#include "Position.h"

#define LOGGING_CATEGORY ChartPanelLog

Q_LOGGING_CATEGORY(ChartPanelLog, "opentraderplatform.gui.chartpanel")

ChartPanel::ChartPanel(MainAlgo* p_mainAlgo, int p_panelId, QWidget* parent)
    : QWidget(parent), m_mainAlgo(p_mainAlgo), m_panelId(p_panelId)
{
    Q_CHECK_PTR(m_mainAlgo);
    setObjectName(QStringLiteral("ChartPanel_%1").arg(p_panelId));

    setupUi();
    // Shortcuts are NOT created here — they are per-panel and only the
    // focused/active panel should respond. ChartWindow manages focus routing.
    // However, 'i' shortcut for symbol input focus is panel-local.

    DEBUG << "Created ChartPanel" << p_panelId;
}

ChartPanel::~ChartPanel()
{
    releaseCurrentSymbolContext();
    DEBUG << "Destroyed ChartPanel" << m_panelId;
}

void ChartPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top bar: symbol input
    auto* topBar = new QWidget(this);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(4, 2, 4, 2);

    m_symbolLabel = new QLabel("Symbol:", topBar);
    topLayout->addWidget(m_symbolLabel);

    m_symbolInput = new QLineEdit(topBar);
    m_symbolInput->setPlaceholderText("Enter symbol...");
    m_symbolInput->setMaximumWidth(120);
    connect(m_symbolInput, &QLineEdit::returnPressed, this, &ChartPanel::onSymbolInputReturnPressed);
    topLayout->addWidget(m_symbolInput);
    topLayout->addStretch();

    mainLayout->addWidget(topBar);

    // Chart
    m_chart = new StockPriceChart(this);
    mainLayout->addWidget(m_chart, 1);

    // Connect chart's requestMissingBars signal
    connect(m_chart, &StockPriceChart::requestMissingBars, this, &ChartPanel::requestMissingBarsFromCache);
    connect(m_chart,
            &StockPriceChart::adjustManagedBracketRequested,
            this,
            [this](const QString& p_symbol,
                   const bool p_adjustStop,
                   const double p_stopPrice,
                   const bool p_adjustTake,
                   const double p_takePrice)
            {
                if (p_symbol.isEmpty())
                {
                    return;
                }

                const std::optional<double> stop = p_adjustStop ? std::optional<double>(p_stopPrice) : std::nullopt;
                const std::optional<double> take = p_adjustTake ? std::optional<double>(p_takePrice) : std::nullopt;
                if (!stop.has_value() && !take.has_value())
                {
                    return;
                }

                const QString symbol = p_symbol.trimmed().toUpper();
                QMetaObject::invokeMethod(
                    m_mainAlgo,
                    [mainAlgo = m_mainAlgo, symbol, stop, take]()
                    {
                        ASSUME_DIFF(mainAlgo, nullptr);
                        const QString accountID = mainAlgo->getActiveAccountId().trimmed().toUpper();
                        if (accountID.isEmpty())
                        {
                            return;
                        }
                        mainAlgo->processAdjustManagedBracketLevels(accountID,
                                                                    symbol,
                                                                    stop,
                                                                    take,
                                                                    QStringLiteral("chart-drag-adjust"));
                    },
                    Qt::QueuedConnection);
            });

    // Connect toolbar timeframe changes
    connect(m_chart->toolbar(), &ChartToolbar::timeFrameChanged, this, &ChartPanel::onTimeFrameChanged);

    setLayout(mainLayout);
}

void ChartPanel::setupShortcuts()
{
    // Timeframe shortcuts (scoped to the parent widget = ChartWindow)
    // Only active when this panel's parent window has focus.
    // NOTE: When multiple panels share a window, ALL panels get the shortcut.
    // This is acceptable — the timeframe change applies to whichever panel
    // the shortcut is routed to (via ChartWindow focus logic).
    auto makeTfShortcut = [this](Qt::Key key, TimeFrame tf) -> QShortcut*
    {
        auto* sc = new QShortcut(QKeySequence(key), this);
        connect(sc, &QShortcut::activated, this, [this, tf]() { m_chart->toolbar()->setCurrentTimeFrame(tf); });
        return sc;
    };

    m_tf10sShortcut = makeTfShortcut(Qt::Key_0, TimeFrame::TEN_SECONDS);
    m_tf1mShortcut = makeTfShortcut(Qt::Key_1, TimeFrame::ONE_MINUTE);
    m_tf5mShortcut = makeTfShortcut(Qt::Key_2, TimeFrame::FIVE_MINUTES);
    m_tf15mShortcut = makeTfShortcut(Qt::Key_3, TimeFrame::FIFTEEN_MINUTES);
    m_tf30mShortcut = makeTfShortcut(Qt::Key_4, TimeFrame::THIRTY_MINUTES);
    m_tf1hShortcut = makeTfShortcut(Qt::Key_5, TimeFrame::ONE_HOUR);
    m_tf4hShortcut = makeTfShortcut(Qt::Key_6, TimeFrame::FOUR_HOURS);
    m_tf1dShortcut = makeTfShortcut(Qt::Key_7, TimeFrame::ONE_DAY);
    m_tf1wShortcut = makeTfShortcut(Qt::Key_8, TimeFrame::ONE_WEEK);
    m_tf1MShortcut = makeTfShortcut(Qt::Key_9, TimeFrame::ONE_MONTH);

    // 'i' focuses this panel's symbol input
    m_focusShortcut = new QShortcut(QKeySequence(Qt::Key_I), this);
    connect(m_focusShortcut,
            &QShortcut::activated,
            this,
            [this]()
            {
                m_symbolInput->clear();
                m_symbolInput->setFocus();
            });
}

void ChartPanel::setSymbol(const QString& p_symbol)
{
    const QString symbol = p_symbol.trimmed().toUpper();
    if (symbol == m_symbol)
        return;

    releaseCurrentSymbolContext();

    m_symbol = symbol;
    m_symbolBindRequestToken += 1;
    const quint64 requestToken = m_symbolBindRequestToken;
    m_symbolInput->setText(m_symbol);

    if (m_symbol.isEmpty())
    {
        m_chart->clearSymbol();
        emit symbolChanged(m_symbol);
        return;
    }

    m_chart->clearSymbol();
    QPointer<ChartPanel> panel(this);
    const bool invoked = QMetaObject::invokeMethod(
        m_mainAlgo,
        [mainAlgo = m_mainAlgo, panel, symbol, requestToken]()
        {
            ASSUME_DIFF(mainAlgo, nullptr);
            if (panel.isNull())
            {
                return;
            }

            QPointer<SymbolContext> sc = mainAlgo->acquireSymbolContext(symbol);
            const std::optional<StrategyStatusEntry> currentStatus = mainAlgo->getLatestStrategyStatusForSymbol(symbol);

            QMetaObject::invokeMethod(
                panel.data(),
                [panel, mainAlgo, symbol, requestToken, sc, currentStatus]()
                {
                    if (panel.isNull())
                    {
                        return;
                    }

                    if (panel->m_symbolBindRequestToken != requestToken || panel->m_symbol != symbol)
                    {
                        if (!sc.isNull())
                        {
                            QMetaObject::invokeMethod(
                                mainAlgo,
                                [mainAlgo, symbol]() { mainAlgo->releaseSymbolContextRef(symbol); },
                                Qt::QueuedConnection);
                        }
                        return;
                    }

                    panel->m_symbolContext = sc;
                    panel->m_chart->setSymbol(symbol);

                    if (currentStatus.has_value())
                    {
                        panel->m_chart->onStrategyStatusEmitted(currentStatus.value());
                    }
                    else
                    {
                        StrategyStatusEntry clearEntry;
                        clearEntry.symbol = symbol;
                        clearEntry.action = StrategyStatusEntry::Action::Clear;
                        panel->m_chart->onStrategyStatusEmitted(clearEntry);
                    }

                    qCDebug(ChartPanelLog) << "ChartPanel" << panel->m_panelId << "now displaying" << symbol;
                    emit panel->symbolChanged(symbol);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
    ASSUME_TRUE(invoked);
}

void ChartPanel::setTimeFrame(TimeFrame p_tf)
{
    if (p_tf == m_currentTimeFrame)
        return;

    m_currentTimeFrame = p_tf;
    m_chart->preserveCurrentRanges();
    m_chart->clearChart();
    m_chart->setDisplayTimeFrame(p_tf);
}

void ChartPanel::refreshFromSnapshot()
{
    QPointer<SymbolContext> sc = m_symbolContext;
    if (!sc)
        return;

    DisplaySnapshot& snap = sc->m_displaySnapshot;
    QReadLocker lock(&snap.lock);

    if (snap.latestLevel2.has_value())
    {
        Level2 l2 = *snap.latestLevel2;
        lock.unlock();
        m_chart->onLevel2Update(sc->symbol, l2);
        lock.relock();
    }

    if (m_currentTimeFrame == TimeFrame::ONE_MINUTE)
    {
        if (snap.latestBar.has_value() && snap.latestBar->isValid())
        {
            Bar bar = *snap.latestBar;
            lock.unlock();
            m_chart->addLiveBar(sc->symbol, bar);
        }
    }
    else
    {
        auto it = snap.aggregatorBars.find(m_currentTimeFrame);
        if (it != snap.aggregatorBars.end() && it.value().isValid())
        {
            Bar bar = it.value();
            lock.unlock();
            m_chart->addLiveBar(sc->symbol, bar);
        }
    }
}

void ChartPanel::onOrderReceived(const Order& p_order)
{
    if (p_order.getSymbol() != m_symbol)
        return;

    Order::Status status = p_order.getOrderStatus();
    const bool isCancelLikeStatus = status == Order::Status::CAN || status == Order::Status::UCN ||
                                    status == Order::Status::TSC || status == Order::Status::REJ ||
                                    status == Order::Status::OUT || status == Order::Status::EXP;

    if (status == Order::Status::OPN || status == Order::Status::ACK)
        m_chart->onOrderPlaced(p_order);
    else if (status == Order::Status::FLL || status == Order::Status::FLP || status == Order::Status::FPR)
        m_chart->onOrderFilled(p_order);
    else if (isCancelLikeStatus)
        m_chart->onOrderCancelled(p_order);
    else if (status == Order::Status::UCH || status == Order::Status::RSN)
        m_chart->onOrderAmended(p_order);
}

void ChartPanel::onPositionReceived(const Position& p_position)
{
    if (p_position.getSymbol() != m_symbol)
        return;

    int quantity = p_position.getQuantity().toInt();
    if (quantity == 0)
        m_chart->onPositionClosed(p_position);
    else
        m_chart->onPositionUpdated(p_position);
}

void ChartPanel::onPositionClosed(const Position& p_position)
{
    if (p_position.getSymbol() != m_symbol)
        return;
    m_chart->onPositionClosed(p_position);
}

void ChartPanel::onStrategyLogReceived(const StrategyLogEntry& p_entry)
{
    if (p_entry.symbol != m_symbol)
        return;
    m_chart->onStrategyLogEmitted(p_entry);
}

void ChartPanel::onStrategyStatusReceived(const StrategyStatusEntry& p_entry)
{
    if (p_entry.symbol != m_symbol)
    {
        return;
    }

    m_chart->onStrategyStatusEmitted(p_entry);
}

void ChartPanel::onStrategyBracketOverlayReceived(const StrategyBracketOverlayEntry& p_entry)
{
    if (p_entry.symbol != m_symbol)
    {
        return;
    }
    m_chart->onStrategyBracketOverlayEmitted(p_entry);
}

void ChartPanel::enterReplayMode(const QString& p_replaySymbol)
{
    m_preReplaySymbol = m_symbol;
    m_chart->clearChart();
    m_chart->setReplayModeActive(true);
    setSymbol(p_replaySymbol);
    DEBUG << "ChartPanel" << m_panelId << "entered replay mode, saved symbol:" << m_preReplaySymbol;
}

void ChartPanel::exitReplayMode()
{
    m_chart->clearChart();
    m_chart->setReplayModeActive(false);

    if (!m_preReplaySymbol.isEmpty())
    {
        setSymbol(m_preReplaySymbol);
        m_preReplaySymbol.clear();
    }
    DEBUG << "ChartPanel" << m_panelId << "exited replay mode";
}

void ChartPanel::onSymbolInputReturnPressed()
{
    QString symbol = m_symbolInput->text().toUpper().trimmed();
    if (symbol.isEmpty())
        return;

    logInputEvent(u"ChartPanel", u"set-symbol", {inputDetail(u"panelId", m_panelId), inputDetail(u"symbol", symbol)});
    setSymbol(symbol);
}

void ChartPanel::onTimeFrameChanged(TimeFrame p_tf)
{
    logInputEvent(u"ChartPanel",
                  u"set-timeframe",
                  {inputDetail(u"panelId", m_panelId), inputDetail(u"timeframe", timeFrameToString(p_tf))});
    setTimeFrame(p_tf);
}

void ChartPanel::releaseCurrentSymbolContext()
{
    if (!m_symbolContext || m_symbol.isEmpty())
        return;

    QString sym = m_symbol;
    QMetaObject::invokeMethod(
        m_mainAlgo,
        [this, sym]() { m_mainAlgo->releaseSymbolContextRef(sym); },
        Qt::QueuedConnection);

    m_symbolContext = nullptr;
}

void ChartPanel::requestMissingBarsFromCache(const QString& p_symbol,
                                             const QDateTime& p_from,
                                             const QDateTime& p_to,
                                             uint64_t p_requestToken)
{
    if (!m_chart->isExpectedMissingBarsRequest(p_symbol, p_requestToken))
    {
        DEBUG << "ChartPanel" << m_panelId << "ignoring stale missing bars request for" << p_symbol << "token"
              << p_requestToken;
        return;
    }

    if (!m_symbolContext)
        return;

    DEBUG << "ChartPanel" << m_panelId << "requesting missing bars from" << p_from << "to" << p_to;

    BarCache::GetBarsResult_t result =
        m_symbolContext->barCache.getBars(m_currentTimeFrame, p_from.date(), p_from.time(), p_to.time());

    if (std::holds_alternative<std::shared_ptr<QVector<Bar>>>(result))
    {
        auto bars = std::get<std::shared_ptr<QVector<Bar>>>(result);
        if (bars->isEmpty())
            m_chart->onRequestedMissingBarsFailed(p_symbol, p_requestToken);
        else
            m_chart->onRequestedMissingBarsReceived(p_symbol, p_requestToken, bars);
    }
    else if (std::holds_alternative<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result))
    {
        auto future = std::get<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result);
        future.then(
            [panel = QPointer<ChartPanel>(this), p_symbol, p_requestToken](
                std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>&& bars) mutable
            {
                if (panel.isNull())
                {
                    return;
                }

                QMetaObject::invokeMethod(
                    panel,
                    [panel, p_symbol, p_requestToken, bars = std::move(bars)]() mutable
                    {
                        if (panel.isNull())
                        {
                            return;
                        }

                        if (!panel->m_chart->isExpectedMissingBarsRequest(p_symbol, p_requestToken))
                        {
                            qCDebug(ChartPanelLog)
                                << "Ignoring stale missing bars callback for" << p_symbol << "token" << p_requestToken;
                            return;
                        }

                        if (bars.has_value() && !bars.value()->isEmpty())
                        {
                            panel->m_chart->onRequestedMissingBarsReceived(p_symbol, p_requestToken, bars.value());
                        }
                        else if (!bars.has_value())
                        {
                            const bool terminal = bars.error() == TSClient::Error::RejectedByValidator;
                            panel->m_chart->onRequestedMissingBarsError(
                                p_symbol,
                                p_requestToken,
                                terminal,
                                terminal ? QString("Invalid symbol: %1 - historical requests stopped").arg(p_symbol)
                                         : QString("Historical data unavailable for %1 - retrying").arg(p_symbol));
                        }
                        else
                        {
                            panel->m_chart->onRequestedMissingBarsFailed(p_symbol, p_requestToken);
                        }
                    },
                    Qt::QueuedConnection);
            });
    }
}
