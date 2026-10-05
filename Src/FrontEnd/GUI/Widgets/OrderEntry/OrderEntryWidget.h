#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QVector>
#include <QToolButton>
#include <QMenu>
#include <QCheckBox>

#include "Account.h"
#include "PlaceOrder.h"
#include "Level2.h"
#include "OrderExecution/GetRoutes/OrderRoute.h"

class GUIFrontend;

class OrderEntryWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit OrderEntryWidget(QWidget* p_parent = nullptr);
    ~OrderEntryWidget();

    /// Set reference to GUIFrontend for accessing current account selection
    /// @param guiFrontend Pointer to parent GUIFrontend instance
    void setGUIFrontend(GUIFrontend* guiFrontend);

    /// Check if result popup notification is enabled
    /// @return True if popup should appear after order execution
    bool isResultPopupEnabled() const
    {
        return m_resultPopupEnabled;
    }

    /// Check if Cancel All confirmation dialog is enabled
    /// @return True if confirmation is required before canceling all orders
    bool isCancelAllConfirmationEnabled() const
    {
        return m_cancelAllConfirmationEnabled;
    }

    [[nodiscard]] double getManualBracketStopLossPercent() const
    {
        return m_manualBracketStopLossPercentInput->value();
    }

    [[nodiscard]] double getManualBracketTakeProfitPercent() const
    {
        return m_manualBracketTakeProfitPercentInput->value();
    }

    [[nodiscard]] int getConfiguredQuantity() const
    {
        return m_quantityInput ? m_quantityInput->value() : 0;
    }

    void setReviewModeEnabled(bool p_enabled);

  public slots:
    /// Update available accounts list
    /// @param accounts List of trading accounts to populate dropdown
    void setAccounts(const QList<Account>& accounts);

    /// Set the symbol for order entry
    /// @param symbol Stock symbol to trade (e.g., "AAPL")
    void setSymbol(const QString& symbol);

    /// Execute a market buy order (shortcut trigger)
    void executeBuyOrder();

    /// Execute a market sell order (shortcut trigger)
    void executeSellOrder();

    /// Execute a buy-to-cover order (close short position)
    void executeBuyToCoverOrder();

    /// Execute a sell short order (open short position)
    void executeSellToCoverOrder();

    /// Handle market depth updates for sticky price calculation
    /// Updates best bid/ask prices when sticky mode is enabled
    /// @param symbol Stock symbol of the update
    /// @param level2 Level 2 book snapshot with current bid/ask levels
    void onMarketDepthUpdate(const QString& symbol, const Level2& level2);

  signals:
    /// Emitted when user submits an order (after validation)
    /// @param order The order request details
    void orderPlaced(const PlaceOrderRequest& order);

  private slots:
    void onOrderTypeChanged(int index);
    void onTradeActionChanged(int id);
    void onSubmitClicked();
    void saveOrderTypeSetting(int index);
    void saveDurationSetting(int index);
    void saveQuantitySetting(int value);
    void saveLimitPriceSetting(double value);
    void saveStopPriceSetting(double value);
    void saveTradeActionSetting(int id);
    void saveManualBracketStopLossPercentSetting(double value);
    void saveManualBracketTakeProfitPercentSetting(double value);
    void onConfirmationCheckBoxToggled(bool checked);
    void onResultPopupCheckBoxToggled(bool checked);
    void onCancelAllConfirmationCheckBoxToggled(bool checked);
    void onStickyCheckBoxToggled(bool checked);
    void saveStickyPriceSetting(bool checked);
    void onStickyModeChanged(int id);
    void saveStickyModeSetting(int id);
    void onStickyOffsetChanged(double value);
    void saveStickyOffsetSetting(double value);
    void saveRouteSetting(int index);

  private:
    void setupUI();
    void setupStyles();
    void updatePriceFieldsVisibility();
    void updateInteractivity();
    void refreshOrderRoutes();
    void populateLiveRoutes(const QVector<OrderRoute>& p_routes);
    void populateFallbackLiveRoute();
    void loadSavedSettings();
    [[nodiscard]] bool validateInputs();
    [[nodiscard]] PlaceOrderRequest buildOrderRequest();
    bool updateStickyPrice();

    // UI Components
    QLabel* m_headerLabel;
    QRadioButton* m_buyRadio;
    QRadioButton* m_buyToCoverRadio;
    QRadioButton* m_sellRadio;
    QRadioButton* m_sellToCoverRadio;
    QButtonGroup* m_tradeActionGroup;
    QComboBox* m_orderTypeCombo;
    QSpinBox* m_quantityInput;
    QDoubleSpinBox* m_limitPriceInput;
    QDoubleSpinBox* m_stopPriceInput;
    QComboBox* m_durationCombo;
    QComboBox* m_routeCombo;
    QPushButton* m_submitButton;
    QWidget* m_limitControlsWidget = nullptr;
    QWidget* m_stopControlsWidget = nullptr;

    // Sticky price components
    QCheckBox* m_stickyCheckBox;
    QRadioButton* m_aggressiveRadio;
    QRadioButton* m_passiveRadio;
    QButtonGroup* m_stickyModeGroup;
    QDoubleSpinBox* m_stickyOffsetInput;
    QWidget* m_stickyControlsWidget = nullptr; // Container for mode/offset to enable/disable as a group
    QDoubleSpinBox* m_manualBracketStopLossPercentInput;
    QDoubleSpinBox* m_manualBracketTakeProfitPercentInput;

    // Settings menu
    QToolButton* m_settingsButton;
    QMenu* m_settingsMenu;
    QCheckBox* m_confirmationCheckBox;
    QCheckBox* m_resultPopupCheckBox;
    QCheckBox* m_cancelAllConfirmationCheckBox;

    bool m_confirmationEnabled;          // Whether to show confirmation dialog
    bool m_resultPopupEnabled;           // Whether to show result popup after order execution
    bool m_cancelAllConfirmationEnabled; // Whether to show confirmation dialog when cancelling all orders

    // Sticky price state
    bool m_stickyEnabled;
    bool m_stickyAggressiveMode; // true = aggressive, false = passive
    bool m_reviewModeEnabled;
    bool m_hasValidBestBid;
    bool m_hasValidBestAsk;
    double m_lastBestBid;
    double m_lastBestAsk;

    // Reference to GUIFrontend for account selection
    GUIFrontend* m_guiFrontend;

    // Account storage
    QVector<Account> m_accounts;

    // Current symbol
    QString m_currentSymbol;

    // Route selection state
    bool m_routesLoaded;
    bool m_routesRequestInFlight;
    bool m_lastReplayMode;
};
