#include "OrderEntryWidget.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QGroupBox>

#include "Misc/Logging.h"

OrderEntryWidget::OrderEntryWidget(QWidget* p_parent)
    : QWidget(p_parent)
    , m_headerLabel(new QLabel("ORDER ENTRY", this))
    , m_accountCombo(new QComboBox(this))
    , m_symbolInput(new QLineEdit(this))
    , m_tradeActionCombo(new QComboBox(this))
    , m_orderTypeCombo(new QComboBox(this))
    , m_quantityInput(new QSpinBox(this))
    , m_limitPriceInput(new QDoubleSpinBox(this))
    , m_stopPriceInput(new QDoubleSpinBox(this))
    , m_durationCombo(new QComboBox(this))
    , m_submitButton(new QPushButton("Submit Order", this))
    , m_limitPriceLabel(new QLabel("Limit Price:", this))
    , m_stopPriceLabel(new QLabel("Stop Price:", this))
{
    setupUI();
    setupStyles();
}

OrderEntryWidget::~OrderEntryWidget() {
    // Qt will handle deletion of child widgets
}

void OrderEntryWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Setup header
    m_headerLabel->setFixedHeight(24);
    m_headerLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_headerLabel);

    // Create form layout for inputs
    QWidget* formWidget = new QWidget(this);
    QFormLayout* formLayout = new QFormLayout(formWidget);
    formLayout->setSpacing(8);
    formLayout->setContentsMargins(8, 8, 8, 8);

    // Account selection
    m_accountCombo->setToolTip("Select trading account");
    formLayout->addRow("Account:", m_accountCombo);

    // Symbol input
    m_symbolInput->setPlaceholderText("e.g., AAPL");
    m_symbolInput->setMaxLength(10);
    m_symbolInput->setToolTip("Stock symbol to trade");
    formLayout->addRow("Symbol:", m_symbolInput);

    // Trade Action (Buy/Sell)
    m_tradeActionCombo->addItem("Buy", static_cast<int>(TradeAction::Buy));
    m_tradeActionCombo->addItem("Sell", static_cast<int>(TradeAction::Sell));
    m_tradeActionCombo->addItem("Buy To Cover", static_cast<int>(TradeAction::BuyToCover));
    m_tradeActionCombo->addItem("Sell Short", static_cast<int>(TradeAction::SellShort));
    m_tradeActionCombo->setToolTip("Select trade action");
    formLayout->addRow("Action:", m_tradeActionCombo);

    // Order Type
    m_orderTypeCombo->addItem("Market", static_cast<int>(OrderType::Type::Market));
    m_orderTypeCombo->addItem("Limit", static_cast<int>(OrderType::Type::Limit));
    m_orderTypeCombo->addItem("Stop Market", static_cast<int>(OrderType::Type::StopMarket));
    m_orderTypeCombo->addItem("Stop Limit", static_cast<int>(OrderType::Type::StopLimit));
    m_orderTypeCombo->setToolTip("Select order type");
    formLayout->addRow("Order Type:", m_orderTypeCombo);

    // Quantity
    m_quantityInput->setMinimum(1);
    m_quantityInput->setMaximum(999999);
    m_quantityInput->setValue(100);
    m_quantityInput->setToolTip("Number of shares");
    formLayout->addRow("Quantity:", m_quantityInput);

    // Limit Price
    m_limitPriceInput->setMinimum(0.01);
    m_limitPriceInput->setMaximum(999999.99);
    m_limitPriceInput->setDecimals(2);
    m_limitPriceInput->setValue(0.00);
    m_limitPriceInput->setPrefix("$ ");
    m_limitPriceInput->setToolTip("Limit price for order");
    formLayout->addRow(m_limitPriceLabel, m_limitPriceInput);

    // Stop Price
    m_stopPriceInput->setMinimum(0.01);
    m_stopPriceInput->setMaximum(999999.99);
    m_stopPriceInput->setDecimals(2);
    m_stopPriceInput->setValue(0.00);
    m_stopPriceInput->setPrefix("$ ");
    m_stopPriceInput->setToolTip("Stop price for order");
    formLayout->addRow(m_stopPriceLabel, m_stopPriceInput);

    // Duration
    m_durationCombo->addItem("Day", static_cast<int>(OrderDuration::Day));
    m_durationCombo->addItem("Day+", static_cast<int>(OrderDuration::DayPlus));
    m_durationCombo->addItem("GTC", static_cast<int>(OrderDuration::GTC));
    m_durationCombo->addItem("GTC+", static_cast<int>(OrderDuration::GTCPlus));
    m_durationCombo->addItem("IOC", static_cast<int>(OrderDuration::IOC));
    m_durationCombo->addItem("FOK", static_cast<int>(OrderDuration::FOK));
    m_durationCombo->setToolTip("Order duration");
    formLayout->addRow("Duration:", m_durationCombo);

    mainLayout->addWidget(formWidget);

    // Submit button
    m_submitButton->setMinimumHeight(32);
    m_submitButton->setToolTip("Submit order to TradeStation");
    mainLayout->addWidget(m_submitButton);

    // Add spacer at bottom
    mainLayout->addStretch();

    // Set fixed width
    setFixedWidth(280);

    // Connect signals
    auto c1 = connect(m_orderTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                      this, &OrderEntryWidget::onOrderTypeChanged, Qt::UniqueConnection);
    Q_ASSERT(c1);

    auto c2 = connect(m_submitButton, &QPushButton::clicked,
                      this, &OrderEntryWidget::onSubmitClicked, Qt::UniqueConnection);
    Q_ASSERT(c2);

    // Initialize visibility based on default order type
    updatePriceFieldsVisibility();
}

void OrderEntryWidget::setupStyles() {
    // Style the header label
    m_headerLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #2D2D2D;"
        "   color: #FFFFFF;"
        "   padding: 4px;"
        "   border-bottom: 1px solid #3D3D3D;"
        "}"
    );

    // Style the submit button
    m_submitButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #00A0E9;"
        "   color: #FFFFFF;"
        "   border: none;"
        "   border-radius: 4px;"
        "   padding: 8px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #0080C0;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #006090;"
        "}"
        "QPushButton:disabled {"
        "   background-color: #505050;"
        "   color: #888888;"
        "}"
    );
}

void OrderEntryWidget::setAccounts(const QVector<Account>& accounts) {
    m_accounts = accounts;
    m_accountCombo->clear();
    
    for (const Account& account : accounts) {
        m_accountCombo->addItem(
            QString("%1 (%2)").arg(account.accountID, account.name),
            account.accountID
        );
    }
    
    // Enable/disable based on account availability
    bool hasAccounts = !accounts.isEmpty();
    m_accountCombo->setEnabled(hasAccounts);
    m_submitButton->setEnabled(hasAccounts);
}

void OrderEntryWidget::setSymbol(const QString& symbol) {
    m_symbolInput->setText(symbol.toUpper());
}

void OrderEntryWidget::onOrderTypeChanged(int index) {
    Q_UNUSED(index);
    updatePriceFieldsVisibility();
}

void OrderEntryWidget::updatePriceFieldsVisibility() {
    OrderType::Type orderType = static_cast<OrderType::Type>(
        m_orderTypeCombo->currentData().toInt()
    );

    // Show/hide limit price based on order type
    bool needsLimitPrice = (orderType == OrderType::Type::Limit || 
                            orderType == OrderType::Type::StopLimit);
    m_limitPriceLabel->setVisible(needsLimitPrice);
    m_limitPriceInput->setVisible(needsLimitPrice);

    // Show/hide stop price based on order type
    bool needsStopPrice = (orderType == OrderType::Type::StopMarket || 
                           orderType == OrderType::Type::StopLimit);
    m_stopPriceLabel->setVisible(needsStopPrice);
    m_stopPriceInput->setVisible(needsStopPrice);
}

bool OrderEntryWidget::validateInputs() {
    // Check account selected
    if (m_accountCombo->currentIndex() < 0) {
        QMessageBox::warning(this, "Invalid Input", "Please select an account.");
        return false;
    }

    // Check symbol
    QString symbol = m_symbolInput->text().trimmed();
    if (symbol.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a stock symbol.");
        m_symbolInput->setFocus();
        return false;
    }

    // Check quantity
    if (m_quantityInput->value() < 1) {
        QMessageBox::warning(this, "Invalid Input", "Quantity must be at least 1.");
        m_quantityInput->setFocus();
        return false;
    }

    // Check limit price if needed
    OrderType::Type orderType = static_cast<OrderType::Type>(
        m_orderTypeCombo->currentData().toInt()
    );
    
    if (orderType == OrderType::Type::Limit || orderType == OrderType::Type::StopLimit) {
        if (m_limitPriceInput->value() <= 0.0) {
            QMessageBox::warning(this, "Invalid Input", "Limit price must be greater than 0.");
            m_limitPriceInput->setFocus();
            return false;
        }
    }

    // Check stop price if needed
    if (orderType == OrderType::Type::StopMarket || orderType == OrderType::Type::StopLimit) {
        if (m_stopPriceInput->value() <= 0.0) {
            QMessageBox::warning(this, "Invalid Input", "Stop price must be greater than 0.");
            m_stopPriceInput->setFocus();
            return false;
        }
    }

    return true;
}

PlaceOrderRequest OrderEntryWidget::buildOrderRequest() {
    PlaceOrderRequest request;

    // Set required fields
    QString accountID = m_accountCombo->currentData().toString();
    request.setAccountID(accountID);

    QString symbol = m_symbolInput->text().trimmed().toUpper();
    request.setSymbol(symbol);

    TradeAction tradeAction = static_cast<TradeAction>(
        m_tradeActionCombo->currentData().toInt()
    );
    request.setTradeAction(tradeAction);

    OrderType::Type orderType = static_cast<OrderType::Type>(
        m_orderTypeCombo->currentData().toInt()
    );
    request.setOrderType(orderType);

    request.setQuantity(m_quantityInput->value());

    OrderDuration duration = static_cast<OrderDuration>(
        m_durationCombo->currentData().toInt()
    );
    TimeInForce timeInForce(duration);
    request.setTimeInForce(timeInForce);

    // Set optional fields based on order type
    if (orderType == OrderType::Type::Limit || orderType == OrderType::Type::StopLimit) {
        request.setLimitPrice(m_limitPriceInput->value());
    }

    if (orderType == OrderType::Type::StopMarket || orderType == OrderType::Type::StopLimit) {
        request.setStopPrice(m_stopPriceInput->value());
    }

    return request;
}

void OrderEntryWidget::onSubmitClicked() {
    if (!validateInputs()) {
        return;
    }

    PlaceOrderRequest order = buildOrderRequest();

    // Confirm order with user
    QString confirmMessage = QString(
        "Submit order:\n\n"
        "Symbol: %1\n"
        "Action: %2\n"
        "Type: %3\n"
        "Quantity: %4\n"
    ).arg(
        order.getSymbol(),
        m_tradeActionCombo->currentText(),
        m_orderTypeCombo->currentText(),
        QString::number(order.getQuantity())
    );

    if (order.getLimitPrice().has_value()) {
        confirmMessage += QString("Limit Price: $%1\n")
            .arg(order.getLimitPrice().value(), 0, 'f', 2);
    }

    if (order.getStopPrice().has_value()) {
        confirmMessage += QString("Stop Price: $%1\n")
            .arg(order.getStopPrice().value(), 0, 'f', 2);
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm Order",
        confirmMessage,
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        qCInfo(frontend_log) << "Order submitted:" << order.toJsonString();
        emit orderPlaced(order);
    }
}
