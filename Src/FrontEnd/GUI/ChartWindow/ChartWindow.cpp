#include <QVBoxLayout>
#include <QSplitter>
#include <QCloseEvent>
#include <QApplication>
#include <QScreen>
#include <QWindow>

#include "ChartWindow.h"
#include "ChartPanel.h"
#include "MainAlgo.h"
#include "Logging.h"
#include "Assume.h"
#include "Misc/ShortcutSettings.h"

#define LOGGING_CATEGORY ChartWindowLog

Q_LOGGING_CATEGORY(ChartWindowLog, "l2trader.gui.chartwindow")

ChartWindow::ChartWindow(MainAlgo* p_mainAlgo, int p_windowId, QWidget* parent)
    : QWidget(parent, Qt::Window), m_mainAlgo(p_mainAlgo), m_windowId(p_windowId)
{
    Q_CHECK_PTR(m_mainAlgo);
    setObjectName(QStringLiteral("ChartWindow_%1").arg(p_windowId));
    setMinimumSize(600, 400);

    // Inherit application palette (dark theme)
    setPalette(QApplication::palette());

    setupUi();
    setupShortcuts();

    DEBUG << "Created ChartWindow" << p_windowId;
}

ChartWindow::~ChartWindow()
{
    DEBUG << "Destroyed ChartWindow" << m_windowId;
}

void ChartWindow::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Auto-detect orientation from screen: landscape → horizontal split, portrait → vertical
    m_splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(m_splitter, 1);

    setLayout(mainLayout);
}

void ChartWindow::setupShortcuts()
{
    // Ctrl+W closes this window
    ShortcutSettings& shortcutSettings = ShortcutSettings::getInstance();
    m_closeShortcut = new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::CloseChartWindow), this);
    connect(m_closeShortcut, &QShortcut::activated, this, &QWidget::close);
}

ChartPanel* ChartWindow::addChartPanel(const QString& p_symbol)
{
    int panelId = m_nextPanelId++;
    auto* panel = new ChartPanel(m_mainAlgo, panelId, m_splitter);
    m_splitter->addWidget(panel);
    m_panels.append(panel);

    // Distribute splitter sizes equally across all panels
    QList<int> sizes;
    int totalSize = (m_splitter->orientation() == Qt::Horizontal) ? m_splitter->width() : m_splitter->height();
    int sizePerPanel = qMax(1, totalSize / m_panels.size());
    for (int i = 0; i < m_panels.size(); ++i)
        sizes.append(sizePerPanel);
    m_splitter->setSizes(sizes);

    if (!p_symbol.isEmpty())
        panel->setSymbol(p_symbol);

    updateWindowTitle();

    DEBUG << "Added ChartPanel" << panelId << "to ChartWindow" << m_windowId
          << "symbol:" << (p_symbol.isEmpty() ? "(none)" : p_symbol) << "total panels:" << m_panels.size();

    return panel;
}

void ChartWindow::refreshAllPanels()
{
    for (ChartPanel* panel: m_panels)
        panel->refreshFromSnapshot();
}

void ChartWindow::onOrderReceived(const Order& p_order)
{
    for (ChartPanel* panel: m_panels)
        panel->onOrderReceived(p_order);
}

void ChartWindow::onPositionReceived(const Position& p_position)
{
    for (ChartPanel* panel: m_panels)
        panel->onPositionReceived(p_position);
}

void ChartWindow::onPositionClosed(const Position& p_position)
{
    for (ChartPanel* panel: m_panels)
        panel->onPositionClosed(p_position);
}

void ChartWindow::onStrategyLogReceived(const StrategyLogEntry& p_entry)
{
    for (ChartPanel* panel: m_panels)
        panel->onStrategyLogReceived(p_entry);
}

void ChartWindow::enterReplayMode(const QString& p_replaySymbol)
{
    for (ChartPanel* panel: m_panels)
        panel->enterReplayMode(p_replaySymbol);
    DEBUG << "ChartWindow" << m_windowId << "entered replay mode with" << m_panels.size() << "panels";
}

void ChartWindow::exitReplayMode()
{
    for (ChartPanel* panel: m_panels)
        panel->exitReplayMode();
    DEBUG << "ChartWindow" << m_windowId << "exited replay mode";
}

void ChartWindow::updateSplitOrientation()
{
    QScreen* screen = nullptr;
    if (windowHandle())
        screen = windowHandle()->screen();
    if (!screen)
        screen = QApplication::primaryScreen();

    if (screen)
    {
        QRect geo = screen->availableGeometry();
        Qt::Orientation orient = (geo.width() >= geo.height()) ? Qt::Horizontal : Qt::Vertical;
        m_splitter->setOrientation(orient);
    }
}

void ChartWindow::updateWindowTitle()
{
    QString title = "L2Trader";

    if (!m_panels.isEmpty())
    {
        QStringList symbols;
        for (const ChartPanel* panel: m_panels)
        {
            QString sym = panel->getSymbol();
            if (!sym.isEmpty())
                symbols.append(sym);
        }

        if (!symbols.isEmpty())
            title += " — " + symbols.join(" | ");
    }

    setWindowTitle(title);
}

void ChartWindow::closeEvent(QCloseEvent* event)
{
    emit windowClosed(this);
    event->accept();
}
