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

#include "Account.h"
#include "PlaceOrder.h"

class GUIFrontend;

class OrderEntryWidget : public QWidget {
    Q_OBJECT
public:
    explicit OrderEntryWidget(QWidget* p_parent = nullptr);
    ~OrderEntryWidget();

    void setGUIFrontend(GUIFrontend* guiFrontend);

public slots:
    void setAccounts(const QList<Account>& accounts);
    void setSymbol(const QString& symbol);

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

private:
    void setupUI();
    void setupStyles();
    void updatePriceFieldsVisibility();
    void loadSavedSettings();
    [[nodiscard]] bool validateInputs();
    [[nodiscard]] PlaceOrderRequest buildOrderRequest();

    // UI Components
    QLabel* m_headerLabel;
    QLineEdit* m_symbolInput;
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

    // Reference to GUIFrontend for account selection
    GUIFrontend* m_guiFrontend;

    // Account storage
    QVector<Account> m_accounts;
};
