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
#include "MarketDepthQuote.h"

class GUIFrontend;

class OrderEntryWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit OrderEntryWidget(QWidget* p_parent = nullptr);
    ~OrderEntryWidget();

    void setGUIFrontend(GUIFrontend* guiFrontend);

    bool isResultPopupEnabled() const
    {
        return m_resultPopupEnabled;
    }
    bool isCancelAllConfirmationEnabled() const
    {
        return m_cancelAllConfirmationEnabled;
    }

  public slots:
    void setAccounts(const QList<Account>& accounts);
    void setSymbol(const QString& symbol);
    void executeBuyOrder();
    void executeSellOrder();
    void executeBuyToCoverOrder();
    void executeSellToCoverOrder();
    void onMarketDepthUpdate(const QString& symbol, const MarketDepthQuote& quote);

  signals:
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
    void onConfirmationCheckBoxToggled(bool checked);
    void onResultPopupCheckBoxToggled(bool checked);
    void onCancelAllConfirmationCheckBoxToggled(bool checked);
    void onStickyCheckBoxToggled(bool checked);
    void saveStickyPriceSetting(bool checked);
    void onStickyModeChanged(int id);
    void saveStickyModeSetting(int id);
    void onStickyOffsetChanged(double value);
    void saveStickyOffsetSetting(double value);

  private:
    void setupUI();
    void setupStyles();
    void updatePriceFieldsVisibility();
    void loadSavedSettings();
    [[nodiscard]] bool validateInputs();
    [[nodiscard]] PlaceOrderRequest buildOrderRequest();
    void updateStickyPrice();
    void flashLimitPriceInput();

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
    QPushButton* m_submitButton;

    // Labels for price fields
    QLabel* m_limitPriceLabel;
    QLabel* m_stopPriceLabel;

    // Sticky price components
    QLabel* m_stickyLabel;
    QCheckBox* m_stickyCheckBox;
    QRadioButton* m_aggressiveRadio;
    QRadioButton* m_passiveRadio;
    QButtonGroup* m_stickyModeGroup;
    QDoubleSpinBox* m_stickyOffsetInput;

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
    double m_lastBestBid;
    double m_lastBestAsk;

    // Reference to GUIFrontend for account selection
    GUIFrontend* m_guiFrontend;

    // Account storage
    QVector<Account> m_accounts;

    // Current symbol
    QString m_currentSymbol;
};
