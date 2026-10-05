#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>
#include <QString>

class MainAlgo;

/**
 * @brief Panel widget that streams live logs for a single selected strategy.
 *
 * Sits in the right half of the bottom logger horizontal splitter.
 * Hidden by default; shown when the user right-clicks a strategy in
 * StrategyQuickView and chooses "Display Logs".
 *
 * Layout:
 *  ┌──────────────────────────────────────────────┐
 *  │  [● Strategy Log: MyStrat] [Level ▾] [✕]   │  ← header bar
 *  ├──────────────────────────────────────────────┤
 *  │  QTextEdit (read-only, monospace, dark bg)   │  ← log body
 *  └──────────────────────────────────────────────┘
 *
 * Polling: QTimer fires every 1 000 ms and appends only new messages.
 */
class StrategyLogWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit StrategyLogWidget(MainAlgo* p_mainAlgo, QWidget* parent = nullptr);
    ~StrategyLogWidget() override = default;

    /// @brief Start streaming logs for the given strategy.
    void setStrategy(const QString& p_strategyID, const QString& p_strategyName);

    /// @brief Stop streaming and clear the display.
    void clearStrategy();

  signals:
    /**
     * @brief Emitted when the user presses the close (✕) button.
     * Thread context: Emitted from Main/GUI thread.
     * The parent (GUIFrontend) collapses the right panel in the splitter.
     */
    void closeRequested();

  private slots:
    void onRefreshTimer();
    void onLevelFilterChanged(int index);
    void onScrolled();

  private:
    void setupUI();
    void setupStyles();

    MainAlgo* m_mainAlgo;

    QString m_strategyID;
    int m_selectedLogLevel{-1}; ///< -1 = All, otherwise QtMsgType
    int m_lastLogCount{0};
    bool m_autoScroll{true};
    bool m_refreshInFlight{false};
    quint64 m_refreshRequestToken{0};

    QLabel* m_titleLabel;
    QComboBox* m_levelFilter;
    QPushButton* m_closeButton;
    QTextEdit* m_logDisplay;
    QTimer* m_timer;

    static QString levelToString(QtMsgType level);
    static QString levelToHtmlColor(QtMsgType level);
};
