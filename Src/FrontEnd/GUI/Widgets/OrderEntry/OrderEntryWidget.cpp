#include "OrderEntryWidget.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QGroupBox>
#include <QWidgetAction>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include "../../GUIFrontend.h"
#include "Misc/Settings.h"
#include "Assume.h"
#include "TSClient.h"
#include "MainAlgo.h"
#include "Core/Replay/ReplayEngine.h"

OrderEntryWidget::OrderEntryWidget(QWidget* p_parent)
    : QWidget(p_parent)
    , m_headerLabel(new QLabel("ORDER ENTRY", this))
    , m_buyRadio(new QRadioButton("Buy", this))
    , m_buyToCoverRadio(new QRadioButton("Buy to Cover", this))
    , m_sellRadio(new QRadioButton("Sell", this))
    , m_sellToCoverRadio(new QRadioButton("Sell Short", this))
    , m_tradeActionGroup(new QButtonGroup(this))
    , m_orderTypeCombo(new QComboBox(this))
    , m_quantityInput(new QSpinBox(this))
    , m_limitPriceInput(new QDoubleSpinBox(this))
    , m_stopPriceInput(new QDoubleSpinBox(this))
    , m_durationCombo(new QComboBox(this))
    , m_submitButton(new QPushButton("Submit Order", this))
    , m_limitPriceLabel(new QLabel("Limit Price:", this))
    , m_stopPriceLabel(new QLabel("Stop Price:", this))
    , m_stickyLabel(new QLabel("Sticky:", this))
    , m_stickyCheckBox(new QCheckBox(this))
    , m_aggressiveRadio(new QRadioButton("Aggressive", this))
    , m_passiveRadio(new QRadioButton("Passive", this))
    , m_stickyModeGroup(new QButtonGroup(this))
    , m_stickyOffsetInput(new QDoubleSpinBox(this))
    , m_settingsButton(new QToolButton(this))
    , m_settingsMenu(new QMenu(this))
    , m_confirmationCheckBox(new QCheckBox("Enable Order Confirmation", this))
    , m_resultPopupCheckBox(new QCheckBox("Enable Result Popup", this))
    , m_cancelAllConfirmationCheckBox(new QCheckBox("Enable Cancel All Confirmation", this))
    , m_confirmationEnabled(true)          // Default to enabled
    , m_resultPopupEnabled(true)           // Default to enabled
    , m_cancelAllConfirmationEnabled(true) // Default to enabled
    , m_stickyEnabled(false)               // Default to disabled
    , m_stickyAggressiveMode(true)         // Default to aggressive
    , m_lastBestBid(0.0)
    , m_lastBestAsk(0.0)
    , m_guiFrontend(nullptr)
{
    setupUI();
    setupStyles();
}

OrderEntryWidget::~OrderEntryWidget()
{
    // Qt will handle deletion of child widgets
}

void OrderEntryWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Setup header with settings button
    QWidget* headerWidget = new QWidget(this);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);

    m_headerLabel->setFixedHeight(24);
    m_headerLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(m_headerLabel);

    // Setup settings button with cog icon
    m_settingsButton->setText("⚙"); // Unicode cog icon
    m_settingsButton->setToolTip("Order Entry Settings");
    m_settingsButton->setPopupMode(QToolButton::InstantPopup);
    m_settingsButton->setMenu(m_settingsMenu);
    m_settingsButton->setFixedSize(24, 24);
    headerLayout->addWidget(m_settingsButton);

    mainLayout->addWidget(headerWidget);

    // Setup settings menu
    m_confirmationCheckBox->setChecked(m_confirmationEnabled);
    QWidgetAction* confirmationAction = new QWidgetAction(m_settingsMenu);
    confirmationAction->setDefaultWidget(m_confirmationCheckBox);
    m_settingsMenu->addAction(confirmationAction);

    m_resultPopupCheckBox->setChecked(m_resultPopupEnabled);
    QWidgetAction* resultPopupAction = new QWidgetAction(m_settingsMenu);
    resultPopupAction->setDefaultWidget(m_resultPopupCheckBox);
    m_settingsMenu->addAction(resultPopupAction);

    m_cancelAllConfirmationCheckBox->setChecked(m_cancelAllConfirmationEnabled);
    QWidgetAction* cancelAllConfirmationAction = new QWidgetAction(m_settingsMenu);
    cancelAllConfirmationAction->setDefaultWidget(m_cancelAllConfirmationCheckBox);
    m_settingsMenu->addAction(cancelAllConfirmationAction);

    // Create form layout for inputs
    QWidget* formWidget = new QWidget(this);
    QFormLayout* formLayout = new QFormLayout(formWidget);
    formLayout->setSpacing(8);
    formLayout->setContentsMargins(8, 8, 8, 8);

    // Trade Action (Buy/Sell) - Radio buttons
    QGroupBox* tradeActionGroup = new QGroupBox("", this);
    QGridLayout* tradeActionLayout = new QGridLayout(tradeActionGroup);
    tradeActionLayout->setContentsMargins(8, 8, 8, 8);
    tradeActionLayout->setSpacing(4);

    m_tradeActionGroup->addButton(m_buyRadio, static_cast<int>(TradeAction::Buy));
    m_tradeActionGroup->addButton(m_sellRadio, static_cast<int>(TradeAction::Sell));
    m_tradeActionGroup->addButton(m_buyToCoverRadio, static_cast<int>(TradeAction::BuyToCover));
    m_tradeActionGroup->addButton(m_sellToCoverRadio, static_cast<int>(TradeAction::SellShort));

    // Arrange in 2x2 grid: Buy | Sell
    //                      Buy to Cover | Sell Short
    tradeActionLayout->addWidget(m_buyRadio, 0, 0);
    tradeActionLayout->addWidget(m_sellRadio, 0, 1);
    tradeActionLayout->addWidget(m_buyToCoverRadio, 1, 0);
    tradeActionLayout->addWidget(m_sellToCoverRadio, 1, 1);

    // Set Buy as default
    m_buyRadio->setChecked(true);

    formLayout->addRow(tradeActionGroup);

    // Order Type and Quantity on same row
    m_orderTypeCombo->addItem("Market", static_cast<int>(OrderType::Type::Market));
    m_orderTypeCombo->addItem("Limit", static_cast<int>(OrderType::Type::Limit));
    m_orderTypeCombo->addItem("Stop Market", static_cast<int>(OrderType::Type::StopMarket));
    m_orderTypeCombo->addItem("Stop Limit", static_cast<int>(OrderType::Type::StopLimit));
    m_orderTypeCombo->setToolTip("Select order type");

    m_quantityInput->setMinimum(1);
    m_quantityInput->setMaximum(999999);
    m_quantityInput->setValue(100);
    m_quantityInput->setToolTip("Number of shares");

    // Create horizontal layout for order type and quantity
    QWidget* orderTypeQuantityWidget = new QWidget(this);
    QHBoxLayout* orderTypeQuantityLayout = new QHBoxLayout(orderTypeQuantityWidget);
    orderTypeQuantityLayout->setContentsMargins(0, 0, 0, 0);
    orderTypeQuantityLayout->setSpacing(16);

    orderTypeQuantityLayout->addWidget(new QLabel("Order Type:"));
    orderTypeQuantityLayout->addWidget(m_orderTypeCombo);
    orderTypeQuantityLayout->addWidget(new QLabel("Quantity:"));
    orderTypeQuantityLayout->addWidget(m_quantityInput);
    orderTypeQuantityLayout->addStretch();

    formLayout->addRow(orderTypeQuantityWidget);

    // Limit Price with Sticky checkbox on same line
    m_limitPriceInput->setMinimum(0.01);
    m_limitPriceInput->setMaximum(999999.99);
    m_limitPriceInput->setDecimals(2);
    m_limitPriceInput->setValue(0.00);
    m_limitPriceInput->setPrefix("$ ");
    m_limitPriceInput->setToolTip("Limit price for order");

    m_stickyCheckBox->setToolTip("Enable auto-update limit price from market depth");

    // Create horizontal layout for limit price and sticky checkbox with label
    QWidget* limitPriceWidget = new QWidget(this);
    QHBoxLayout* limitPriceLayout = new QHBoxLayout(limitPriceWidget);
    limitPriceLayout->setContentsMargins(0, 0, 0, 0);
    limitPriceLayout->setSpacing(8);
    limitPriceLayout->addWidget(m_limitPriceInput);
    limitPriceLayout->addWidget(new QLabel("Sticky:"));
    limitPriceLayout->addWidget(m_stickyCheckBox);
    limitPriceLayout->addStretch(); // Push everything to the left

    formLayout->addRow(m_limitPriceLabel, limitPriceWidget);

    // Aggressive/Passive and offset on next line
    // Configure sticky mode radio buttons
    m_stickyModeGroup->addButton(m_aggressiveRadio, 0);
    m_stickyModeGroup->addButton(m_passiveRadio, 1);
    m_aggressiveRadio->setChecked(true); // Default to aggressive
    m_aggressiveRadio->setToolTip("Cross spread for guaranteed fills (Buy=Ask+offset, Sell=Bid-offset)");
    m_passiveRadio->setToolTip("Enter on bid/ask for better price (Buy=Bid+offset, Sell=Ask-offset)");

    // Configure sticky offset spinbox
    m_stickyOffsetInput->setMinimum(0.00);
    m_stickyOffsetInput->setMaximum(10.00);
    m_stickyOffsetInput->setDecimals(2);
    m_stickyOffsetInput->setSingleStep(0.01);
    m_stickyOffsetInput->setValue(0.00);
    m_stickyOffsetInput->setPrefix("$ ");
    m_stickyOffsetInput->setToolTip("Price offset from best bid/ask");
    m_stickyOffsetInput->setFixedWidth(65);

    // Create horizontal layout for sticky controls
    m_stickyControlsWidget = new QWidget(this);
    QHBoxLayout* stickyLayout = new QHBoxLayout(m_stickyControlsWidget);
    stickyLayout->setContentsMargins(0, 0, 0, 0);
    stickyLayout->setSpacing(4);
    stickyLayout->addWidget(m_aggressiveRadio);
    stickyLayout->addWidget(m_passiveRadio);
    stickyLayout->addWidget(m_stickyOffsetInput);
    stickyLayout->addStretch(); // Push everything to the left

    formLayout->addRow("Mode/Offset:", m_stickyControlsWidget);

    // Initially disable sticky controls since sticky is off by default
    m_stickyControlsWidget->setEnabled(false);

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

    // Set fixed width (increased to accommodate sticky controls)
    setFixedWidth(320);

    // Load saved settings
    loadSavedSettings();

    // Connect signals
    auto c1 = connect(m_orderTypeCombo,
                      QOverload<int>::of(&QComboBox::currentIndexChanged),
                      this,
                      &OrderEntryWidget::onOrderTypeChanged,
                      Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c1);

    auto c2 = connect(m_tradeActionGroup,
                      QOverload<int>::of(&QButtonGroup::idClicked),
                      this,
                      &OrderEntryWidget::onTradeActionChanged,
                      Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c2);

    auto c3 =
        connect(m_submitButton, &QPushButton::clicked, this, &OrderEntryWidget::onSubmitClicked, Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c3);

    // Connect settings save signals
    auto c4 = connect(m_orderTypeCombo,
                      QOverload<int>::of(&QComboBox::currentIndexChanged),
                      this,
                      &OrderEntryWidget::saveOrderTypeSetting,
                      Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c4);

    auto c5 = connect(m_durationCombo,
                      QOverload<int>::of(&QComboBox::currentIndexChanged),
                      this,
                      &OrderEntryWidget::saveDurationSetting,
                      Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c5);

    auto c6 = connect(m_quantityInput,
                      QOverload<int>::of(&QSpinBox::valueChanged),
                      this,
                      &OrderEntryWidget::saveQuantitySetting,
                      Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c6);

    auto c7 = connect(m_limitPriceInput,
                      QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                      this,
                      &OrderEntryWidget::saveLimitPriceSetting,
                      Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c7);

    auto c8 = connect(m_stopPriceInput,
                      QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                      this,
                      &OrderEntryWidget::saveStopPriceSetting,
                      Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c8);

    auto c9 = connect(m_tradeActionGroup,
                      QOverload<int>::of(&QButtonGroup::idClicked),
                      this,
                      &OrderEntryWidget::saveTradeActionSetting,
                      Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c9);

    // Connect confirmation checkbox
    auto c10 = connect(m_confirmationCheckBox,
                       &QCheckBox::toggled,
                       this,
                       &OrderEntryWidget::onConfirmationCheckBoxToggled,
                       Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c10);

    // Connect result popup checkbox
    auto c11 = connect(m_resultPopupCheckBox,
                       &QCheckBox::toggled,
                       this,
                       &OrderEntryWidget::onResultPopupCheckBoxToggled,
                       Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c11);

    // Connect cancel all confirmation checkbox
    auto c12 = connect(m_cancelAllConfirmationCheckBox,
                       &QCheckBox::toggled,
                       this,
                       &OrderEntryWidget::onCancelAllConfirmationCheckBoxToggled,
                       Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c12);

    // Connect sticky checkbox
    auto c13 = connect(m_stickyCheckBox,
                       &QCheckBox::toggled,
                       this,
                       &OrderEntryWidget::onStickyCheckBoxToggled,
                       Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c13);

    auto c14 = connect(m_stickyCheckBox,
                       &QCheckBox::toggled,
                       this,
                       &OrderEntryWidget::saveStickyPriceSetting,
                       Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c14);

    // Connect sticky offset spinbox
    auto c15 = connect(m_stickyOffsetInput,
                       QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                       this,
                       &OrderEntryWidget::onStickyOffsetChanged,
                       Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c15);

    auto c16 = connect(m_stickyOffsetInput,
                       QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                       this,
                       &OrderEntryWidget::saveStickyOffsetSetting,
                       Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c16);

    // Connect sticky mode radio buttons
    auto c17 = connect(m_stickyModeGroup,
                       QOverload<int>::of(&QButtonGroup::idClicked),
                       this,
                       &OrderEntryWidget::onStickyModeChanged,
                       Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c17);

    auto c18 = connect(m_stickyModeGroup,
                       QOverload<int>::of(&QButtonGroup::idClicked),
                       this,
                       &OrderEntryWidget::saveStickyModeSetting,
                       Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c18);

    // Initialize visibility based on default order type
    updatePriceFieldsVisibility();

    // Button styling is initialized in loadSavedSettings() based on saved trade action
}

void OrderEntryWidget::setupStyles()
{
    // Style the header label
    m_headerLabel->setStyleSheet("QLabel {"
                                 "   background-color: #2D2D2D;"
                                 "   color: #FFFFFF;"
                                 "   padding: 4px;"
                                 "   border-bottom: 1px solid #3D3D3D;"
                                 "}");

    // Submit button styling is now handled dynamically in onTradeActionChanged
}

void OrderEntryWidget::setGUIFrontend(GUIFrontend* guiFrontend)
{
    m_guiFrontend = guiFrontend;
}

void OrderEntryWidget::setAccounts(const QList<Account>& accounts)
{
    m_accounts = accounts.toVector();

    // Enable/disable submit button based on account availability
    bool hasAccounts = !accounts.isEmpty();
    m_submitButton->setEnabled(hasAccounts);
}

void OrderEntryWidget::setSymbol(const QString& symbol)
{
    m_currentSymbol = symbol.toUpper();
}

void OrderEntryWidget::executeBuyOrder()
{
    // Set trade action to Buy
    m_buyRadio->setChecked(true);
    // Explicitly trigger the trade action change to update button appearance
    onTradeActionChanged(static_cast<int>(TradeAction::Buy));
    // Submit the order
    onSubmitClicked();
}

void OrderEntryWidget::executeSellOrder()
{
    // Set trade action to Sell
    m_sellRadio->setChecked(true);
    // Explicitly trigger the trade action change to update button appearance
    onTradeActionChanged(static_cast<int>(TradeAction::Sell));
    // Submit the order
    onSubmitClicked();
}

void OrderEntryWidget::executeBuyToCoverOrder()
{
    // Set trade action to Buy to Cover
    m_buyToCoverRadio->setChecked(true);
    // Explicitly trigger the trade action change to update button appearance
    onTradeActionChanged(static_cast<int>(TradeAction::BuyToCover));
    // Submit the order
    onSubmitClicked();
}

void OrderEntryWidget::executeSellToCoverOrder()
{
    // Set trade action to Sell Short
    m_sellToCoverRadio->setChecked(true);
    // Explicitly trigger the trade action change to update button appearance
    onTradeActionChanged(static_cast<int>(TradeAction::SellShort));
    // Submit the order
    onSubmitClicked();
}


void OrderEntryWidget::onOrderTypeChanged(int index)
{
    Q_UNUSED(index);
    updatePriceFieldsVisibility();
}

void OrderEntryWidget::onTradeActionChanged(int id)
{
    TradeAction action = static_cast<TradeAction>(id);

    QString buttonText;
    QString buttonStyle;

    switch (action)
    {
    case TradeAction::Buy:
        buttonText = "Buy";
        buttonStyle = "QPushButton {"
                      "   background-color: #28A745;" // Green
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

    case TradeAction::BuyToCover:
        buttonText = "Buy to Cover";
        buttonStyle = "QPushButton {"
                      "   background-color: #28A745;" // Green
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
        buttonText = "Sell";
        buttonStyle = "QPushButton {"
                      "   background-color: #DC3545;" // Red
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

    case TradeAction::SellShort:
        buttonText = "Sell Short";
        buttonStyle = "QPushButton {"
                      "   background-color: #DC3545;" // Red
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
        buttonStyle = "QPushButton {"
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

    // Update sticky price if enabled (trade action affects price calculation)
    if (m_stickyEnabled)
    {
        updateStickyPrice();
    }
}

void OrderEntryWidget::updatePriceFieldsVisibility()
{
    OrderType::Type orderType = static_cast<OrderType::Type>(m_orderTypeCombo->currentData().toInt());

    // Show/hide limit price based on order type
    bool needsLimitPrice = (orderType == OrderType::Type::Limit || orderType == OrderType::Type::StopLimit);
    m_limitPriceLabel->setVisible(needsLimitPrice);
    m_limitPriceInput->setVisible(needsLimitPrice);
    m_stickyLabel->setVisible(needsLimitPrice);
    m_stickyCheckBox->setVisible(needsLimitPrice);
    m_aggressiveRadio->setVisible(needsLimitPrice);
    m_passiveRadio->setVisible(needsLimitPrice);
    m_stickyOffsetInput->setVisible(needsLimitPrice);

    // Show/hide stop price based on order type
    bool needsStopPrice = (orderType == OrderType::Type::StopMarket || orderType == OrderType::Type::StopLimit);
    m_stopPriceLabel->setVisible(needsStopPrice);
    m_stopPriceInput->setVisible(needsStopPrice);
}

bool OrderEntryWidget::validateInputs()
{
    // Check GUIFrontend reference
    OBJ_ASSUME_TRUE(m_guiFrontend);

    // Check if in replay mode and replay is not running
    if (TSClient::getInstance()->getMode() == TSClient::Mode::Replay)
    {
        MainAlgo* mainAlgo = MainAlgo::getInstance();
        if (mainAlgo->getReplayState() != ReplayEngine::PlaybackState::Playing)
        {
            QMessageBox::warning(this,
                                 "Replay Not Running",
                                 "Cannot place orders when replay is paused or stopped.\n\n"
                                 "Please start replay playback by pressing the Play button.");
            return false;
        }
    }

    // Check account selected
    QString accountID = m_guiFrontend->getSelectedAccountId();
    if (accountID.isEmpty())
    {
        QMessageBox::warning(this, "Invalid Input", "Please select an account.");
        return false;
    }

    // Check quantity
    if (m_quantityInput->value() < 1)
    {
        QMessageBox::warning(this, "Invalid Input", "Quantity must be at least 1.");
        m_quantityInput->setFocus();
        return false;
    }

    // Check limit price if needed
    OrderType::Type orderType = static_cast<OrderType::Type>(m_orderTypeCombo->currentData().toInt());

    if (orderType == OrderType::Type::Limit || orderType == OrderType::Type::StopLimit)
    {
        if (m_limitPriceInput->value() <= 0.0)
        {
            QMessageBox::warning(this, "Invalid Input", "Limit price must be greater than 0.");
            m_limitPriceInput->setFocus();
            return false;
        }
    }

    // Check stop price if needed
    if (orderType == OrderType::Type::StopMarket || orderType == OrderType::Type::StopLimit)
    {
        if (m_stopPriceInput->value() <= 0.0)
        {
            QMessageBox::warning(this, "Invalid Input", "Stop price must be greater than 0.");
            m_stopPriceInput->setFocus();
            return false;
        }
    }

    return true;
}

PlaceOrderRequest OrderEntryWidget::buildOrderRequest()
{
    PlaceOrderRequest request;

    // Check GUIFrontend reference
    OBJ_ASSUME_TRUE(m_guiFrontend);

    // Get account from GUIFrontend
    QString accountID = m_guiFrontend->getSelectedAccountId();
    request.setAccountID(accountID);

    QString symbol = m_currentSymbol;
    request.setSymbol(symbol);

    TradeAction tradeAction = static_cast<TradeAction>(m_tradeActionGroup->checkedId());
    request.setTradeAction(tradeAction);

    OrderType::Type orderType = static_cast<OrderType::Type>(m_orderTypeCombo->currentData().toInt());
    request.setOrderType(orderType);

    request.setQuantity(m_quantityInput->value());

    OrderDuration duration = static_cast<OrderDuration>(m_durationCombo->currentData().toInt());
    TimeInForce timeInForce(duration);
    request.setTimeInForce(timeInForce);

    // Set optional fields based on order type
    if (orderType == OrderType::Type::Limit || orderType == OrderType::Type::StopLimit)
    {
        request.setLimitPrice(m_limitPriceInput->value());
    }

    if (orderType == OrderType::Type::StopMarket || orderType == OrderType::Type::StopLimit)
    {
        request.setStopPrice(m_stopPriceInput->value());
    }

    return request;
}

void OrderEntryWidget::onSubmitClicked()
{
    if (!validateInputs())
    {
        return;
    }

    PlaceOrderRequest order = buildOrderRequest();

    // Check if confirmation is enabled
    if (m_confirmationEnabled)
    {
        // Confirm order with user
        QString actionText;
        QRadioButton* checkedButton = qobject_cast<QRadioButton*>(m_tradeActionGroup->checkedButton());
        if (checkedButton)
        {
            actionText = checkedButton->text();
        }
        else
        {
            actionText = "Unknown";
        }

        QString confirmMessage = QString("Submit order:\n\n"
                                         "Symbol: %1\n"
                                         "Action: %2\n"
                                         "Type: %3\n"
                                         "Quantity: %4\n")
                                     .arg(order.getSymbol(),
                                          actionText,
                                          m_orderTypeCombo->currentText(),
                                          QString::number(order.getQuantity()));

        if (order.getLimitPrice().has_value())
        {
            confirmMessage += QString("Limit Price: $%1\n").arg(order.getLimitPrice().value(), 0, 'f', 2);
        }

        if (order.getStopPrice().has_value())
        {
            confirmMessage += QString("Stop Price: $%1\n").arg(order.getStopPrice().value(), 0, 'f', 2);
        }

        QMessageBox::StandardButton reply =
            QMessageBox::question(this, "Confirm Order", confirmMessage, QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes)
        {
            return; // User cancelled
        }
    }

    // Submit the order
    qInfo() << "Order submitted:" << order.toJsonString();
    emit orderPlaced(order);
}

void OrderEntryWidget::loadSavedSettings()
{
    Q_CHECK_PTR(appStateSettings);

    // Load saved order type
    int savedOrderType = appStateSettings->value("OrderEntry/OrderType", 0).toInt();
    if (savedOrderType >= 0 && savedOrderType < m_orderTypeCombo->count())
    {
        m_orderTypeCombo->setCurrentIndex(savedOrderType);
    }

    // Load saved duration
    int savedDuration = appStateSettings->value("OrderEntry/Duration", 0).toInt();
    if (savedDuration >= 0 && savedDuration < m_durationCombo->count())
    {
        m_durationCombo->setCurrentIndex(savedDuration);
    }

    // Load saved quantity
    int savedQuantity = appStateSettings->value("OrderEntry/Quantity", 100).toInt();
    m_quantityInput->setValue(savedQuantity);

    // Load saved limit price
    double savedLimitPrice = appStateSettings->value("OrderEntry/LimitPrice", 0.0).toDouble();
    m_limitPriceInput->setValue(savedLimitPrice);

    // Load saved stop price
    double savedStopPrice = appStateSettings->value("OrderEntry/StopPrice", 0.0).toDouble();
    m_stopPriceInput->setValue(savedStopPrice);

    // Load saved trade action
    int savedTradeAction =
        appStateSettings->value("OrderEntry/TradeAction", static_cast<int>(TradeAction::Buy)).toInt();
    QAbstractButton* button = m_tradeActionGroup->button(savedTradeAction);
    if (button)
    {
        button->setChecked(true);
        // Synchronize the submit button appearance with the loaded trade action
        onTradeActionChanged(savedTradeAction);
    }

    // Load confirmation enabled setting
    m_confirmationEnabled = appStateSettings->value("OrderEntry/ConfirmationEnabled", true).toBool();
    m_confirmationCheckBox->setChecked(m_confirmationEnabled);

    // Load result popup enabled setting
    m_resultPopupEnabled = appStateSettings->value("OrderEntry/ResultPopupEnabled", true).toBool();
    m_resultPopupCheckBox->setChecked(m_resultPopupEnabled);

    // Load cancel all confirmation enabled setting
    m_cancelAllConfirmationEnabled = appStateSettings->value("OrderEntry/CancelAllConfirmationEnabled", true).toBool();
    m_cancelAllConfirmationCheckBox->setChecked(m_cancelAllConfirmationEnabled);

    // Load sticky price settings
    m_stickyEnabled = appStateSettings->value("OrderEntry/StickyPrice", false).toBool();
    m_stickyCheckBox->setChecked(m_stickyEnabled);

    // Load sticky mode (0 = aggressive, 1 = passive)
    int savedStickyMode = appStateSettings->value("OrderEntry/StickyMode", 0).toInt();
    QAbstractButton* modeButton = m_stickyModeGroup->button(savedStickyMode);
    if (modeButton)
    {
        modeButton->setChecked(true);
        m_stickyAggressiveMode = (savedStickyMode == 0);
    }

    double savedStickyOffset = appStateSettings->value("OrderEntry/StickyOffset", 0.00).toDouble();
    m_stickyOffsetInput->setValue(savedStickyOffset);

    // Apply initial enablement state for mode/offset controls
    m_stickyControlsWidget->setEnabled(m_stickyEnabled);
}

void OrderEntryWidget::saveOrderTypeSetting(int index)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/OrderType", index);
    appStateSettings->sync();
}

void OrderEntryWidget::saveDurationSetting(int index)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/Duration", index);
    appStateSettings->sync();
}

void OrderEntryWidget::saveQuantitySetting(int value)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/Quantity", value);
    appStateSettings->sync();
}

void OrderEntryWidget::saveLimitPriceSetting(double value)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/LimitPrice", value);
    appStateSettings->sync();
}

void OrderEntryWidget::saveStopPriceSetting(double value)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/StopPrice", value);
    appStateSettings->sync();
}

void OrderEntryWidget::saveTradeActionSetting(int id)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/TradeAction", id);
    appStateSettings->sync();
}

void OrderEntryWidget::onConfirmationCheckBoxToggled(bool checked)
{
    m_confirmationEnabled = checked;
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/ConfirmationEnabled", checked);
    appStateSettings->sync();
    qInfo() << "Order confirmation" << (checked ? "enabled" : "disabled");
}

void OrderEntryWidget::onResultPopupCheckBoxToggled(bool checked)
{
    m_resultPopupEnabled = checked;
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/ResultPopupEnabled", checked);
    appStateSettings->sync();
    qInfo() << "Order result popup" << (checked ? "enabled" : "disabled");
}

void OrderEntryWidget::onCancelAllConfirmationCheckBoxToggled(bool checked)
{
    m_cancelAllConfirmationEnabled = checked;
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/CancelAllConfirmationEnabled", checked);
    appStateSettings->sync();
    qInfo() << "Cancel all orders confirmation" << (checked ? "enabled" : "disabled");
}

void OrderEntryWidget::onStickyCheckBoxToggled(bool checked)
{
    m_stickyEnabled = checked;

    // Enable/disable the mode and offset controls based on sticky state
    m_stickyControlsWidget->setEnabled(checked);

    if (checked)
    {
        updateStickyPrice();
    }
}

void OrderEntryWidget::saveStickyPriceSetting(bool checked)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/StickyPrice", checked);
    appStateSettings->sync();
}

void OrderEntryWidget::onStickyOffsetChanged(double value)
{
    Q_UNUSED(value);
    if (m_stickyEnabled)
    {
        updateStickyPrice();
    }
}

void OrderEntryWidget::saveStickyOffsetSetting(double value)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/StickyOffset", value);
    appStateSettings->sync();
}

void OrderEntryWidget::onStickyModeChanged(int id)
{
    m_stickyAggressiveMode = (id == 0); // 0 = aggressive, 1 = passive
    if (m_stickyEnabled)
    {
        updateStickyPrice();
    }
}

void OrderEntryWidget::saveStickyModeSetting(int id)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/StickyMode", id);
    appStateSettings->sync();
}

void OrderEntryWidget::onMarketDepthUpdate(const QString& symbol, const MarketDepthQuote& quote)
{
    // Only update if this is for the current symbol
    if (symbol != m_currentSymbol)
    {
        return;
    }

    // Extract best bid and ask prices
    const QVector<MarketDepthLevel>& bids = quote.getBids();
    const QVector<MarketDepthLevel>& asks = quote.getAsks();

    // TODO: Add check for account having "Matrix" label (Level 2 data permission)

    if (!bids.isEmpty())
    {
        bool ok = false;
        double bestBid = bids[0].getPrice().toDouble(&ok);
        if (ok)
        {
            m_lastBestBid = bestBid;
        }
    }

    if (!asks.isEmpty())
    {
        bool ok = false;
        double bestAsk = asks[0].getPrice().toDouble(&ok);
        if (ok)
        {
            m_lastBestAsk = bestAsk;
        }
    }

    // Update sticky price if enabled
    if (m_stickyEnabled)
    {
        updateStickyPrice();
    }
}

void OrderEntryWidget::onL1QuoteUpdate(const QString& symbol, double bid, double ask)
{
    if (symbol != m_currentSymbol)
    {
        return;
    }

    if (bid > 0.0)
    {
        m_lastBestBid = bid;
    }
    if (ask > 0.0)
    {
        m_lastBestAsk = ask;
    }

    if (m_stickyEnabled)
    {
        updateStickyPrice();
    }
}

void OrderEntryWidget::updateStickyPrice()
{
    if (!m_stickyEnabled)
    {
        return;
    }

    TradeAction action = static_cast<TradeAction>(m_tradeActionGroup->checkedId());
    double offset = m_stickyOffsetInput->value();
    double finalPrice = 0.0;

    // Determine base price based on mode and trade action
    switch (action)
    {
    case TradeAction::Buy:
    case TradeAction::BuyToCover:
    case TradeAction::BuyToOpen:
    case TradeAction::BuyToClose:
        // Buy orders
        if (m_stickyAggressiveMode)
        {
            // Aggressive: Cross spread (Ask + offset) for guaranteed fill
            if (m_lastBestAsk > 0.0)
            {
                finalPrice = m_lastBestAsk + offset;
            }
        }
        else
        {
            // Passive: Enter on bid (Bid + offset) for better price
            if (m_lastBestBid > 0.0)
            {
                finalPrice = m_lastBestBid + offset;
            }
        }
        break;

    case TradeAction::Sell:
    case TradeAction::SellShort:
    case TradeAction::SellToOpen:
    case TradeAction::SellToClose:
        // Sell orders
        if (m_stickyAggressiveMode)
        {
            // Aggressive: Cross spread (Bid - offset) for guaranteed fill
            if (m_lastBestBid > 0.0)
            {
                finalPrice = m_lastBestBid - offset;
            }
        }
        else
        {
            // Passive: Enter on ask (Ask - offset) for better price
            if (m_lastBestAsk > 0.0)
            {
                finalPrice = m_lastBestAsk - offset;
            }
        }

        // Ensure price doesn't go negative
        if (finalPrice < 0.01)
        {
            finalPrice = 0.01;
        }
        break;
    }

    if (finalPrice > 0.0)
    {
        m_limitPriceInput->setValue(finalPrice);
    }
}
