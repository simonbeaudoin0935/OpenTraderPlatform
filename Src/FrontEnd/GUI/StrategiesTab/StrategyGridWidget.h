#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QMap>

class StrategyCard;
class StrategyManager;

/**
 * @brief StrategyGridWidget - Horizontal container for strategy cards
 *
 * Displays all active strategies as self-contained cards in horizontal layout.
 * Each card is a unified widget with tile header + details/logs panel.
 * Fixed-width cards (400px) for consistent horizontal scrolling.
 */
class StrategyGridWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit StrategyGridWidget(StrategyManager* p_strategyManager, QWidget* parent = nullptr);
    ~StrategyGridWidget() override = default;

    // Add a strategy card
    void
    addStrategyCard(const QString& strategyID, const QString& name, const QVector<QString>& symbols, bool isRunning);

    // Update status of a specific card
    void updateStrategyCardStatus(const QString& strategyID, bool isRunning, const QString& errorMessage);

    // Remove a specific card
    void removeStrategyCard(const QString& strategyID);

    // Set which card is selected
    void setSelectedCard(const QString& strategyID);

    // Clear all cards
    void clearCards();

  signals:
    void strategyCardClicked(const QString& strategyID);

  private:
    void setupUI();

    QHBoxLayout* m_mainLayout;
    QMap<QString, StrategyCard*> m_cards; // strategyID -> card widget
    QString m_selectedStrategyID;
    StrategyManager* m_strategyManager;
};
