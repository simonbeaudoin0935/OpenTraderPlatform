#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QVector>

#include "Account.h"
#include "PlaceOrder.h"

class OrderEntryWidget : public QWidget {
    Q_OBJECT
public:
    explicit OrderEntryWidget(QWidget* p_parent = nullptr);
    ~OrderEntryWidget();

public slots:
    void setAccounts(const QVector<Account>& accounts);
    void setSymbol(const QString& symbol);

signals:
    void orderPlaced(const PlaceOrderRequest& order);

private slots:
    void onOrderTypeChanged(int index);
    void onSubmitClicked();

private:
    void setupUI();
    void setupStyles();
    void updatePriceFieldsVisibility();
    [[nodiscard]] bool validateInputs();
    [[nodiscard]] PlaceOrderRequest buildOrderRequest();

    // UI Components
    QLabel* m_headerLabel;
    QComboBox* m_accountCombo;
    QLineEdit* m_symbolInput;
    QComboBox* m_tradeActionCombo;
    QComboBox* m_orderTypeCombo;
    QSpinBox* m_quantityInput;
    QDoubleSpinBox* m_limitPriceInput;
    QDoubleSpinBox* m_stopPriceInput;
    QComboBox* m_durationCombo;
    QPushButton* m_submitButton;

    // Labels for price fields
    QLabel* m_limitPriceLabel;
    QLabel* m_stopPriceLabel;

    // Account storage
    QVector<Account> m_accounts;
};
