#include <QApplication>
#include <QScreen>
#include <QWindow>

#include "WindowManager.h"
#include "ChartWindow/ChartWindow.h"
#include "ChartWindow/ChartPanel.h"
#include "MainAlgo.h"
#include "Logging.h"
#include "Assume.h"
#include "Settings.h"

#define LOGGING_CATEGORY WindowManagerLog

Q_LOGGING_CATEGORY(WindowManagerLog, "l2trader.gui.windowmanager")

WindowManager::WindowManager(MainAlgo* p_mainAlgo, QObject* parent) : QObject(parent), m_mainAlgo(p_mainAlgo)
{
    Q_CHECK_PTR(m_mainAlgo);
    setObjectName("WindowManager");
    DEBUG << "WindowManager created";
}

WindowManager::~WindowManager()
{
    closeAllChartWindows();
    DEBUG << "WindowManager destroyed";
}

ChartPanel* WindowManager::openNewChart(const QString& p_symbol)
{
    if (m_chartWindows.isEmpty())
    {
        // No secondary window exists — create one with one panel
        ChartWindow* window = createChartWindow();
        ChartPanel* panel = window->addChartPanel(p_symbol);
        connect(panel, &ChartPanel::symbolChanged, this, &WindowManager::saveWindowState);
        saveWindowState();
        return panel;
    }

    // Add a panel to the last (most recently created) window
    ChartWindow* lastWindow = m_chartWindows.last();
    ChartPanel* panel = lastWindow->addChartPanel(p_symbol);
    connect(panel, &ChartPanel::symbolChanged, this, &WindowManager::saveWindowState);
    lastWindow->updateWindowTitle();
    lastWindow->raise();
    lastWindow->activateWindow();

    saveWindowState();

    DEBUG << "Added panel to existing ChartWindow" << lastWindow->windowId()
          << "total panels:" << lastWindow->panelCount();
    return panel;
}

ChartWindow* WindowManager::createChartWindow(const QString& p_symbol)
{
    int id = m_nextWindowId++;
    auto* window = new ChartWindow(m_mainAlgo, id);

    connect(window, &ChartWindow::windowClosed, this, &WindowManager::onChartWindowClosed);
    m_chartWindows.append(window);

    // Place on the best available screen with standard window decorations (title bar, close/minimize).
    QScreen* targetScreen = findBestScreenForNewWindow();
    window->show(); // creates the native window handle

    if (targetScreen && window->windowHandle())
    {
        window->windowHandle()->setScreen(targetScreen);

        // Size to 80% of screen, centered — gives a normal decorated window
        QRect available = targetScreen->availableGeometry();
        int w = available.width() * 4 / 5;
        int h = available.height() * 4 / 5;
        int x = available.x() + (available.width() - w) / 2;
        int y = available.y() + (available.height() - h) / 2;
        window->setGeometry(x, y, w, h);
    }

    // Set split orientation based on the target screen aspect ratio
    window->updateSplitOrientation();

    window->raise();
    window->activateWindow();

    // If a symbol was provided, create the first panel with that symbol
    if (!p_symbol.isEmpty())
        window->addChartPanel(p_symbol);

    DEBUG << "Created ChartWindow" << id << "on screen" << (targetScreen ? targetScreen->name() : "unknown")
          << "symbol:" << (p_symbol.isEmpty() ? "(none)" : p_symbol);

    return window;
}

void WindowManager::closeChartWindow(ChartWindow* p_window)
{
    OBJ_ASSUME_DIFF(p_window, nullptr);

    int idx = m_chartWindows.indexOf(p_window);
    if (idx < 0)
    {
        WARNING << "Attempted to close unknown ChartWindow";
        return;
    }

    m_chartWindows.removeAt(idx);
    p_window->deleteLater();

    DEBUG << "Closed ChartWindow" << p_window->windowId();
}

void WindowManager::closeAllChartWindows()
{
    while (!m_chartWindows.isEmpty())
    {
        ChartWindow* window = m_chartWindows.takeLast();
        window->disconnect(this);
        window->deleteLater();
    }
    DEBUG << "All chart windows closed";
}

void WindowManager::saveWindowState()
{
    Q_CHECK_PTR(appStateSettings);

    qInfo() << "WindowManager::saveWindowState() called with" << m_chartWindows.size() << "chart windows";

    // Clear previous chart window entries
    appStateSettings->remove("ChartWindows");

    appStateSettings->beginGroup("ChartWindows");
    appStateSettings->setValue("windowCount", m_chartWindows.size());

    for (int i = 0; i < m_chartWindows.size(); ++i)
    {
        ChartWindow* cw = m_chartWindows[i];
        QString winGroup = QStringLiteral("Window_%1").arg(i);
        appStateSettings->beginGroup(winGroup);

        appStateSettings->setValue("geometry", cw->saveGeometry());
        appStateSettings->setValue("panelCount", cw->panelCount());

        // Save which screen this window is on (by name for cross-session matching)
        if (cw->windowHandle() && cw->windowHandle()->screen())
            appStateSettings->setValue("screenName", cw->windowHandle()->screen()->name());

        // Save per-panel state
        const auto& panels = cw->panels();
        for (int j = 0; j < panels.size(); ++j)
        {
            ChartPanel* panel = panels[j];
            QString panelGroup = QStringLiteral("Panel_%1").arg(j);
            appStateSettings->beginGroup(panelGroup);

            appStateSettings->setValue("symbol", panel->getSymbol());
            appStateSettings->setValue("timeFrame", static_cast<int>(panel->currentTimeFrame()));

            qInfo() << "  Saved panel" << j << "symbol:" << panel->getSymbol()
                    << "timeFrame:" << static_cast<int>(panel->currentTimeFrame());

            appStateSettings->endGroup(); // Panel_N
        }

        appStateSettings->endGroup(); // Window_N
    }

    appStateSettings->endGroup(); // ChartWindows
    appStateSettings->sync();

    DEBUG << "Saved state for" << m_chartWindows.size() << "chart windows";
}

void WindowManager::restoreWindowState()
{
    Q_CHECK_PTR(appStateSettings);

    appStateSettings->beginGroup("ChartWindows");
    int windowCount = appStateSettings->value("windowCount", 0).toInt();

    // Backwards compatibility: also check old "count" key
    if (windowCount == 0)
        windowCount = appStateSettings->value("count", 0).toInt();

    qInfo() << "WindowManager::restoreWindowState() - windowCount:" << windowCount;

    for (int i = 0; i < windowCount; ++i)
    {
        QString winGroup = QStringLiteral("Window_%1").arg(i);
        appStateSettings->beginGroup(winGroup);

        QByteArray geometry = appStateSettings->value("geometry").toByteArray();
        QString screenName = appStateSettings->value("screenName").toString();
        int panelCount = appStateSettings->value("panelCount", 0).toInt();

        // Create the window (without a default panel)
        int id = m_nextWindowId++;
        auto* window = new ChartWindow(m_mainAlgo, id);
        connect(window, &ChartWindow::windowClosed, this, &WindowManager::onChartWindowClosed);
        m_chartWindows.append(window);

        // Restore geometry (includes position and size)
        if (!geometry.isEmpty())
        {
            window->restoreGeometry(geometry);
            window->show(); // creates native window handle
        }
        else
        {
            window->show(); // creates native window handle
        }

        // Move to the saved screen if it's still connected
        if (!screenName.isEmpty() && window->windowHandle())
        {
            for (QScreen* screen: QApplication::screens())
            {
                if (screen->name() == screenName)
                {
                    window->windowHandle()->setScreen(screen);
                    if (!geometry.isEmpty())
                        window->restoreGeometry(geometry);
                    break;
                }
            }
        }

        // Set split orientation based on the restored screen
        window->updateSplitOrientation();

        // Restore panels
        for (int j = 0; j < panelCount; ++j)
        {
            QString panelGroup = QStringLiteral("Panel_%1").arg(j);
            appStateSettings->beginGroup(panelGroup);

            QString symbol = appStateSettings->value("symbol").toString();
            int tfInt = appStateSettings->value("timeFrame", static_cast<int>(TimeFrame::ONE_MINUTE)).toInt();

            appStateSettings->endGroup(); // Panel_N

            ChartPanel* panel = window->addChartPanel();
            auto tf = static_cast<TimeFrame>(tfInt);
            panel->setTimeFrame(tf);
            if (!symbol.isEmpty())
                panel->setSymbol(symbol);
            connect(panel, &ChartPanel::symbolChanged, this, &WindowManager::saveWindowState);
        }

        // Backwards compatibility: if no panel sub-groups were saved, check old single-panel keys
        if (panelCount == 0)
        {
            QString symbol = appStateSettings->value("symbol").toString();
            int tfInt = appStateSettings->value("timeFrame", static_cast<int>(TimeFrame::ONE_MINUTE)).toInt();

            if (!symbol.isEmpty())
            {
                ChartPanel* panel = window->addChartPanel();
                panel->setTimeFrame(static_cast<TimeFrame>(tfInt));
                panel->setSymbol(symbol);
                connect(panel, &ChartPanel::symbolChanged, this, &WindowManager::saveWindowState);
            }
        }

        appStateSettings->endGroup(); // Window_N

        DEBUG << "Restored ChartWindow" << id << "with" << window->panelCount() << "panels, screen:" << screenName;
    }

    appStateSettings->endGroup(); // ChartWindows

    if (windowCount > 0)
        DEBUG << "Restored" << windowCount << "chart windows from previous session";
}

QScreen* WindowManager::findBestScreenForNewWindow() const
{
    QList<QScreen*> screens = QApplication::screens();
    if (screens.isEmpty())
        return nullptr;
    if (screens.size() == 1)
        return screens.first();

    // Count windows on each screen: main window + existing chart windows
    QMap<QScreen*, int> windowCounts;
    for (QScreen* s: screens)
        windowCounts[s] = 0;

    // Count the main application window
    for (QWidget* topLevel: QApplication::topLevelWidgets())
    {
        if (topLevel->windowHandle() && topLevel->windowHandle()->screen())
            windowCounts[topLevel->windowHandle()->screen()]++;
    }

    // Find the screen with fewest windows
    QScreen* bestScreen = screens.first();
    int minCount = windowCounts.value(bestScreen, 0);

    for (auto it = windowCounts.begin(); it != windowCounts.end(); ++it)
    {
        if (it.value() < minCount)
        {
            minCount = it.value();
            bestScreen = it.key();
        }
    }

    return bestScreen;
}

void WindowManager::onChartWindowClosed(ChartWindow* p_window)
{
    closeChartWindow(p_window);
    if (!m_shuttingDown)
        saveWindowState();
}
