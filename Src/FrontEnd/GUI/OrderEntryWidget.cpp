#include "OrderEntryWidget.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QGroupBox>
#include "GUIFrontend.h"

OrderEntryWidget::OrderEntryWidget(QWidget* p_parent)
    : QWidget(p_parent)
    , m_guiFrontend(nullptr)
    , m_headerLabel(new QLabel("ORDER ENTRY", this))
    , m_symbolInput(new QLineEdit(this))
    , m_buyRadio(new QRadioButton("Buy", this))
    , m_buyToCoverRadio(new QRadioButton("Buy to Cover", this))
    , m_sellRadio(new QRadioButton("Sell", this))
    , m_sellToCoverRadio(new QRadioButton("Sell to Cover", this))
    , m_tradeActionGroup(new QButtonGroup(this))
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

    // Symbol input
    m_symbolInput->setPlaceholderText("e.g., AAPL");
    m_symbolInput->setMaxLength(10);
    m_symbolInput->setToolTip("Stock symbol to trade");
    formLayout->addRow("Symbol:", m_symbolInput);

    // Trade Action (Buy/Sell) - Radio buttons
    QGroupBox* tradeActionGroup = new QGroupBox("Action:", this);
    QGridLayout* tradeActionLayout = new QGridLayout(tradeActionGroup);
    tradeActionLayout->setContentsMargins(8, 8, 8, 8);
    tradeActionLayout->setSpacing(4);
    
    m_tradeActionGroup->addButton(m_buyRadio, static_cast<int>(TradeAction::Buy));
    m_tradeActionGroup->addButton(m_sellRadio, static_cast<int>(TradeAction::Sell));
    m_tradeActionGroup->addButton(m_buyToCoverRadio, static_cast<int>(TradeAction::BuyToCover));
    m_tradeActionGroup->addButton(m_sellToCoverRadio, static_cast<int>(TradeAction::SellToClose));
    
    // Arrange in 2x2 grid: Buy | Sell
    //                      Buy to Cover | Sell to Cover
    tradeActionLayout->addWidget(m_buyRadio, 0, 0);
    tradeActionLayout->addWidget(m_sellRadio, 0, 1);
    tradeActionLayout->addWidget(m_buyToCoverRadio, 1, 0);
    tradeActionLayout->addWidget(m_sellToCoverRadio, 1, 1);
    
    // Set Buy as default
    m_buyRadio->setChecked(true);
    
    formLayout->addRow(tradeActionGroup);

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

    auto c2 = connect(m_tradeActionGroup, QOverload<int>::of(&QButtonGroup::idClicked),
                      this, &OrderEntryWidget::onTradeActionChanged, Qt::UniqueConnection);
    Q_ASSERT(c2);

    auto c3 = connect(m_submitButton, &QPushButton::clicked,
                      this, &OrderEntryWidget::onSubmitClicked, Qt::UniqueConnection);
    Q_ASSERT(c3);

    // Initialize visibility based on default order type
    updatePriceFieldsVisibility();
    
    // Initialize button styling based on default trade action (Buy)
    onTradeActionChanged(static_cast<int>(TradeAction::Buy));
}

void OrderEntryWidget::setupStyles() {
    // Style the header label
    m_headerLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #2D2D2D;"
        "   color: #FFFFFF;"
        "   padding: 4px;"
        "   border-bottom: 1px solid #3D3D2D;"
        "}"
    );

    // Submit button styling is now handled dynamically in onTradeActionChanged
}

void OrderEntryWidget::setGUIFrontend(GUIFrontend* guiFrontend) {
    m_guiFrontend = guiFrontend;
}

void OrderEntryWidget::setAccounts(const QList<Account>& accounts) {
    m_accounts = accounts.toVector();
    
    // Enable/disable submit button based on account availability
    bool hasAccounts = !accounts.isEmpty();
    m_submitButton->setEnabled(hasAccounts);
}

void OrderEntryWidget::setSymbol(const QString& symbol) {
    m_symbolInput->setText(symbol.toUpper());
}

void OrderEntryWidget::onOrderTypeChanged(int index) {
    Q_UNUSED(index);
    updatePriceFieldsVisibility();
}

void OrderEntryWidget::onTradeActionChanged(int id) {
    TradeAction action = static_cast<TradeAction>(id);
    
    QString buttonText;
    QString buttonStyle;
    
    switch (action) {
        case TradeAction::Buy:
        case TradeAction::BuyToCover:
            buttonText = "Buy";
            buttonStyle = 
                "QPushButton {"
                "   background-color: #28A745;"  // Green
                "   color: #FFFFFF;"
                "   border: none;"
                "   border-radius: 4px;"
                "   padding: 8px;"
                "   font-weight: bold;"
                "}"
                "QPushButton:hover {"
                "   background-color: #218838;"
                "}"
                "QPushButton:pressed {"
                "   background-color: #1E7E34;"
                "}"
                "QPushButton:disabled {"
                "   background-color: #505050;"
                "   color: #888888;"
                "}";
            break;
            
        case TradeAction::Sell:
        case TradeAction::SellToClose:
            buttonText = "Sell";
            buttonStyle = 
                "QPushButton {"
                "   background-color: #DC3545;"  // Red
                "   color: #FFFFFF;"
                "   border: none;"
                "   border-radius: 4px;"
                "   padding: 8px;"
                "   font-weight: bold;"
                "}"
                "QPushButton:hover {"
                "   background-color: #C82333;"
                "}"
                "QPushButton:pressed {"
                "   background-color: #BD2130;"
                "}"
                "QPushButton:disabled {"
                "   background-color: #505050;"
                "   color: #888888;"
                "}";
            break;
            
        default:
            buttonText = "Submit Order";
            buttonStyle = 
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
                "}";
            break;
    }
    
    m_submitButton->setText(buttonText);
    m_submitButton->setStyleSheet(buttonStyle);
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
    // Check GUIFrontend reference
    Q_ASSERT(m_guiFrontend);
    
    // Check account selected
    QString accountID = m_guiFrontend->getSelectedAccountId();
    if (accountID.isEmpty()) {
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

    // Check GUIFrontend reference
    Q_ASSERT(m_guiFrontend);
    
    // Get account from GUIFrontend
    QString accountID = m_guiFrontend->getSelectedAccountId();
    request.setAccountID(accountID);

    QString symbol = m_symbolInput->text().trimmed().toUpper();
    request.setSymbol(symbol);

    TradeAction tradeAction = static_cast<TradeAction>(
        m_tradeActionGroup->checkedId()
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
    QString actionText;
    QRadioButton* checkedButton = qobject_cast<QRadioButton*>(m_tradeActionGroup->checkedButton());
    if (checkedButton) {
        actionText = checkedButton->text();
    } else {
        actionText = "Unknown";
    }
    
    QString confirmMessage = QString(
        "Submit order:\n\n"
        "Symbol: %1\n"
        "Action: %2\n"
        "Type: %3\n"
        "Quantity: %4\n"
    ).arg(
        order.getSymbol(),
        actionText,
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
        qInfo() << "Order submitted:" << order.toJsonString();
        emit orderPlaced(order);
    }
}
