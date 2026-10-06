#include "OrderEntryWidget.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QGroupBox>
#include <QWidgetAction>
#include <QSignalBlocker>
#include "../../GUIFrontend.h"
#include "Misc/Settings.h"
#include "Misc/Logging/Logging.h"
#include "Assume.h"
#include "TSClient.h"
#include "MainAlgo.h"
#include "Core/MainApp.h"
#include "CONSTANTS.h"
#include <cmath>

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
    , m_routeCombo(new QComboBox(this))
    , m_submitButton(new QPushButton("Submit Order", this))
    , m_stickyCheckBox(new QCheckBox(this))
    , m_aggressiveRadio(new QRadioButton("Aggressive", this))
    , m_passiveRadio(new QRadioButton("Passive", this))
    , m_stickyModeGroup(new QButtonGroup(this))
    , m_stickyOffsetInput(new QDoubleSpinBox(this))
    , m_manualBracketStopLossPercentInput(new QDoubleSpinBox(this))
    , m_manualBracketTakeProfitPercentInput(new QDoubleSpinBox(this))
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
    , m_reviewModeEnabled(false)
    , m_hasValidBestBid(false)
    , m_hasValidBestAsk(false)
    , m_lastBestBid(0.0)
    , m_lastBestAsk(0.0)
    , m_guiFrontend(nullptr)
    , m_routesLoaded(false)
    , m_routesRequestInFlight(false)
    , m_lastReplayMode(MainApp::isInReplayMode())
{
    setupUI();
    setupStyles();
    updateInteractivity();
}

OrderEntryWidget::~OrderEntryWidget()
{
    // Qt will handle deletion of child widgets
}

void OrderEntryWidget::setReviewModeEnabled(const bool p_enabled)
{
    m_reviewModeEnabled = p_enabled;
    updateInteractivity();
}

void OrderEntryWidget::updateInteractivity()
{
    const bool isReplayMode = MainApp::isInReplayMode() || TSClient::getInstance()->getMode() == TSClient::Mode::Replay;
    if (isReplayMode != m_lastReplayMode)
    {
        m_lastReplayMode = isReplayMode;
        m_routesLoaded = false;
        m_routesRequestInFlight = false;
    }

    if (isReplayMode)
    {
        const bool hasReplayOption =
            m_routeCombo->count() == 1 && m_routeCombo->itemData(0).toString() == QStringLiteral("replay");
        if (!hasReplayOption)
        {
            QSignalBlocker blocker(m_routeCombo);
            m_routeCombo->clear();
            m_routeCombo->addItem(QStringLiteral("replay"), QStringLiteral("replay"));
            m_routeCombo->setCurrentIndex(0);
        }
    }
    else if (!m_routesLoaded && !m_routesRequestInFlight)
    {
        refreshOrderRoutes();
    }

    const bool hasSymbol = !m_currentSymbol.isEmpty();
    const bool controlsInteractive = !m_reviewModeEnabled && hasSymbol;
    const bool hasAccounts = !m_accounts.isEmpty();
    const bool routeInteractive = controlsInteractive && !isReplayMode && !m_routesRequestInFlight &&
                                  m_routeCombo->count() > 0 && !m_routeCombo->itemData(0).toString().isEmpty();

    m_buyRadio->setEnabled(controlsInteractive);
    m_buyToCoverRadio->setEnabled(controlsInteractive);
    m_sellRadio->setEnabled(controlsInteractive);
    m_sellToCoverRadio->setEnabled(controlsInteractive);
    m_orderTypeCombo->setEnabled(controlsInteractive);
    m_quantityInput->setEnabled(controlsInteractive);
    m_durationCombo->setEnabled(controlsInteractive);
    m_routeCombo->setEnabled(routeInteractive);
    if (m_limitControlsWidget)
    {
        m_limitControlsWidget->setEnabled(controlsInteractive);
    }
    if (m_stopControlsWidget)
    {
        m_stopControlsWidget->setEnabled(controlsInteractive);
    }
    m_limitPriceInput->setEnabled(controlsInteractive);
    m_stopPriceInput->setEnabled(controlsInteractive);
    m_submitButton->setEnabled(controlsInteractive && hasAccounts);
    m_stickyCheckBox->setEnabled(controlsInteractive);
    m_stickyControlsWidget->setEnabled(controlsInteractive && m_stickyEnabled);
    m_settingsButton->setEnabled(controlsInteractive);

    if (m_reviewModeEnabled)
    {
        m_headerLabel->setText("ORDER ENTRY (READ-ONLY)");
        m_submitButton->setText("Review Mode");
        m_submitButton->setToolTip("Order entry is disabled while Review mode is active.");
        return;
    }

    onTradeActionChanged(m_tradeActionGroup->checkedId());

    if (!hasSymbol)
    {
        m_headerLabel->setText("ORDER ENTRY (NO SYMBOL)");
        m_submitButton->setText("Select Symbol");
        m_submitButton->setToolTip("Select a symbol before placing an order.");
        return;
    }

    m_headerLabel->setText("ORDER ENTRY");
    m_submitButton->setToolTip(hasAccounts ? QString() : "Select an account before placing an order.");
}

void OrderEntryWidget::setAccounts(const QList<Account>& accounts)
{
    m_accounts = accounts.toVector();

    updateInteractivity();
}

void OrderEntryWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(4);
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

    // Create compact vertical stack for inputs
    QWidget* formWidget = new QWidget(this);
    QVBoxLayout* formLayout = new QVBoxLayout(formWidget);
    formLayout->setSpacing(6);
    formLayout->setContentsMargins(8, 6, 8, 6);

    // Trade Action (Buy/Sell) - Radio buttons
    QGroupBox* tradeActionGroup = new QGroupBox(this);
    tradeActionGroup->setFlat(true);
    QGridLayout* tradeActionLayout = new QGridLayout(tradeActionGroup);
    tradeActionLayout->setContentsMargins(0, 0, 0, 0);
    tradeActionLayout->setHorizontalSpacing(12);
    tradeActionLayout->setVerticalSpacing(4);

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

    formLayout->addWidget(tradeActionGroup);

    // Type/Qty/Mode in one compact horizontal row
    m_orderTypeCombo->addItem("Market", static_cast<int>(OrderType::Type::Market));
    m_orderTypeCombo->addItem("Limit", static_cast<int>(OrderType::Type::Limit));
    m_orderTypeCombo->addItem("Stop Market", static_cast<int>(OrderType::Type::StopMarket));
    m_orderTypeCombo->addItem("Stop Limit", static_cast<int>(OrderType::Type::StopLimit));
    m_orderTypeCombo->setToolTip("Select order type");
    m_orderTypeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_quantityInput->setMinimum(1);
    m_quantityInput->setMaximum(999999);
    m_quantityInput->setValue(100);
    m_quantityInput->setToolTip("Number of shares");
    m_quantityInput->setFixedWidth(82);

    m_durationCombo->addItem("Day", static_cast<int>(OrderDuration::Day));
    m_durationCombo->addItem("Day+", static_cast<int>(OrderDuration::DayPlus));
    m_durationCombo->addItem("GTC", static_cast<int>(OrderDuration::GTC));
    m_durationCombo->addItem("GTC+", static_cast<int>(OrderDuration::GTCPlus));
    m_durationCombo->addItem("IOC", static_cast<int>(OrderDuration::IOC));
    m_durationCombo->addItem("FOK", static_cast<int>(OrderDuration::FOK));
    m_durationCombo->setToolTip("Order duration mode");
    m_durationCombo->setMinimumWidth(78);

    QWidget* compactRowWidget = new QWidget(this);
    QHBoxLayout* compactRowLayout = new QHBoxLayout(compactRowWidget);
    compactRowLayout->setContentsMargins(0, 0, 0, 0);
    compactRowLayout->setSpacing(6);

    compactRowLayout->addWidget(new QLabel("Type", this));
    compactRowLayout->addWidget(m_orderTypeCombo, 1);
    compactRowLayout->addWidget(new QLabel("Qty", this));
    compactRowLayout->addWidget(m_quantityInput);
    compactRowLayout->addWidget(new QLabel("Mode", this));
    compactRowLayout->addWidget(m_durationCombo);

    formLayout->addWidget(compactRowWidget);

    m_routeCombo->setToolTip("Select order route");
    m_routeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_routeCombo->addItem("Loading...", QString());

    QWidget* routeRowWidget = new QWidget(this);
    QHBoxLayout* routeRowLayout = new QHBoxLayout(routeRowWidget);
    routeRowLayout->setContentsMargins(0, 0, 0, 0);
    routeRowLayout->setSpacing(6);
    routeRowLayout->addWidget(new QLabel("Route", routeRowWidget));
    routeRowLayout->addWidget(m_routeCombo, 1);
    formLayout->addWidget(routeRowWidget);

    // Limit controls (shown only for limit-capable orders)
    m_limitPriceInput->setMinimum(0.01);
    m_limitPriceInput->setMaximum(999999.99);
    m_limitPriceInput->setDecimals(2);
    m_limitPriceInput->setValue(0.00);
    m_limitPriceInput->setPrefix("$ ");
    m_limitPriceInput->setToolTip("Limit price for order");
    m_limitPriceInput->setFixedWidth(96);

    m_stickyCheckBox->setToolTip("Enable auto-update limit price from market depth");

    // Configure sticky mode controls
    m_stickyModeGroup->addButton(m_aggressiveRadio, 0);
    m_stickyModeGroup->addButton(m_passiveRadio, 1);
    m_aggressiveRadio->setText("Agg");
    m_passiveRadio->setText("Pass");
    m_aggressiveRadio->setChecked(true);
    m_aggressiveRadio->setToolTip("Cross spread for guaranteed fills (Buy=Ask+offset, Sell=Bid-offset)");
    m_passiveRadio->setToolTip("Enter on bid/ask for better price (Buy=Bid+offset, Sell=Ask-offset)");

    m_stickyOffsetInput->setMinimum(0.00);
    m_stickyOffsetInput->setMaximum(10.00);
    m_stickyOffsetInput->setDecimals(2);
    m_stickyOffsetInput->setSingleStep(0.01);
    m_stickyOffsetInput->setValue(0.00);
    m_stickyOffsetInput->setPrefix("$ ");
    m_stickyOffsetInput->setToolTip("Price offset from best bid/ask");
    m_stickyOffsetInput->setFixedWidth(76);

    m_limitControlsWidget = new QWidget(formWidget);
    QVBoxLayout* limitEntryLayout = new QVBoxLayout(m_limitControlsWidget);
    limitEntryLayout->setContentsMargins(0, 0, 0, 0);
    limitEntryLayout->setSpacing(4);

    QWidget* limitTopRowWidget = new QWidget(m_limitControlsWidget);
    QHBoxLayout* limitTopRowLayout = new QHBoxLayout(limitTopRowWidget);
    limitTopRowLayout->setContentsMargins(0, 0, 0, 0);
    limitTopRowLayout->setSpacing(6);
    limitTopRowLayout->addWidget(new QLabel("Limit", limitTopRowWidget));
    limitTopRowLayout->addWidget(m_limitPriceInput);
    limitTopRowLayout->addWidget(new QLabel("Sticky", limitTopRowWidget));
    limitTopRowLayout->addWidget(m_stickyCheckBox);
    limitTopRowLayout->addStretch();

    m_stickyControlsWidget = new QWidget(m_limitControlsWidget);
    QHBoxLayout* stickyLayout = new QHBoxLayout(m_stickyControlsWidget);
    stickyLayout->setContentsMargins(0, 0, 0, 0);
    stickyLayout->setSpacing(4);
    stickyLayout->addWidget(new QLabel("Mode", m_stickyControlsWidget));
    stickyLayout->addWidget(m_aggressiveRadio);
    stickyLayout->addWidget(m_passiveRadio);
    stickyLayout->addWidget(new QLabel("Offset", m_stickyControlsWidget));
    stickyLayout->addWidget(m_stickyOffsetInput);
    stickyLayout->addStretch();

    limitEntryLayout->addWidget(limitTopRowWidget);
    limitEntryLayout->addWidget(m_stickyControlsWidget);
    formLayout->addWidget(m_limitControlsWidget);

    // Stop controls (shown only for stop-capable orders)
    m_stickyControlsWidget->setEnabled(false);
    m_stopPriceInput->setMinimum(0.01);
    m_stopPriceInput->setMaximum(999999.99);
    m_stopPriceInput->setDecimals(2);
    m_stopPriceInput->setValue(0.00);
    m_stopPriceInput->setPrefix("$ ");
    m_stopPriceInput->setToolTip("Stop price for order");
    m_stopPriceInput->setFixedWidth(110);

    m_stopControlsWidget = new QWidget(formWidget);
    QHBoxLayout* stopEntryLayout = new QHBoxLayout(m_stopControlsWidget);
    stopEntryLayout->setContentsMargins(0, 0, 0, 0);
    stopEntryLayout->setSpacing(6);
    stopEntryLayout->addWidget(new QLabel("Stop", m_stopControlsWidget));
    stopEntryLayout->addWidget(m_stopPriceInput);
    stopEntryLayout->addStretch();
    formLayout->addWidget(m_stopControlsWidget);

    m_manualBracketStopLossPercentInput->setMinimum(0.00);
    m_manualBracketStopLossPercentInput->setMaximum(99.99);
    m_manualBracketStopLossPercentInput->setDecimals(2);
    m_manualBracketStopLossPercentInput->setSingleStep(0.25);
    m_manualBracketStopLossPercentInput->setSuffix("%");
    m_manualBracketStopLossPercentInput->setToolTip(
        "Default stop-loss percent used when pressing B/S to arm a chart bracket.");

    m_manualBracketTakeProfitPercentInput->setMinimum(0.00);
    m_manualBracketTakeProfitPercentInput->setMaximum(999.99);
    m_manualBracketTakeProfitPercentInput->setDecimals(2);
    m_manualBracketTakeProfitPercentInput->setSingleStep(0.25);
    m_manualBracketTakeProfitPercentInput->setSuffix("%");
    m_manualBracketTakeProfitPercentInput->setToolTip(
        "Default take-profit percent used when pressing B/S to arm a chart bracket.");

    QWidget* manualBracketRowWidget = new QWidget(formWidget);
    QHBoxLayout* manualBracketRowLayout = new QHBoxLayout(manualBracketRowWidget);
    manualBracketRowLayout->setContentsMargins(0, 0, 0, 0);
    manualBracketRowLayout->setSpacing(6);
    manualBracketRowLayout->addWidget(new QLabel("Bracket Stop/Take", manualBracketRowWidget));
    manualBracketRowLayout->addWidget(m_manualBracketStopLossPercentInput);
    manualBracketRowLayout->addWidget(m_manualBracketTakeProfitPercentInput);
    formLayout->addWidget(manualBracketRowWidget);

    mainLayout->addWidget(formWidget);

    // Submit button
    m_submitButton->setMinimumHeight(28);
    m_submitButton->setToolTip("Submit order to TradeStation");
    mainLayout->addWidget(m_submitButton);

    // Preserve previous horizontal footprint while compressing vertical footprint.
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

    auto c19 = connect(m_routeCombo,
                       QOverload<int>::of(&QComboBox::currentIndexChanged),
                       this,
                       &OrderEntryWidget::saveRouteSetting,
                       Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c19);

    auto c20 = connect(m_manualBracketStopLossPercentInput,
                       QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                       this,
                       &OrderEntryWidget::saveManualBracketStopLossPercentSetting,
                       Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c20);

    auto c21 = connect(m_manualBracketTakeProfitPercentInput,
                       QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                       this,
                       &OrderEntryWidget::saveManualBracketTakeProfitPercentSetting,
                       Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(c21);

    // Initialize visibility based on default order type
    updatePriceFieldsVisibility();

    // Button styling is initialized in loadSavedSettings() based on saved trade action
}

void OrderEntryWidget::setupStyles()
{
    m_headerLabel->setStyleSheet(QStringLiteral("QLabel {"
                                                "   background-color: %1;"
                                                "   color: %2;"
                                                "   padding: 4px;"
                                                "   border-bottom: 1px solid %3;"
                                                "}")
                                     .arg(QString::fromLatin1(GUIThemeConstants::SIDEBAR_BACKGROUND))
                                     .arg(QString::fromLatin1(GUIThemeConstants::TEXT_PRIMARY))
                                     .arg(QString::fromLatin1(GUIThemeConstants::BORDER)));

    // Submit button styling is now handled dynamically in onTradeActionChanged
}

void OrderEntryWidget::setGUIFrontend(GUIFrontend* guiFrontend)
{
    m_guiFrontend = guiFrontend;
}

void OrderEntryWidget::setSymbol(const QString& symbol)
{
    const QString nextSymbol = symbol.toUpper();
    if (m_currentSymbol != nextSymbol)
    {
        m_hasValidBestBid = false;
        m_hasValidBestAsk = false;
        m_lastBestBid = 0.0;
        m_lastBestAsk = 0.0;
    }
    m_currentSymbol = nextSymbol;
    updateInteractivity();
}

void OrderEntryWidget::executeBuyOrder()
{
    logInputEvent(u"OrderEntryWidget", u"quick-buy");
    // Set trade action to Buy
    m_buyRadio->setChecked(true);
    // Explicitly trigger the trade action change to update button appearance
    onTradeActionChanged(static_cast<int>(TradeAction::Buy));
    // Submit the order
    onSubmitClicked();
}

void OrderEntryWidget::executeSellOrder()
{
    logInputEvent(u"OrderEntryWidget", u"quick-sell");
    // Set trade action to Sell
    m_sellRadio->setChecked(true);
    // Explicitly trigger the trade action change to update button appearance
    onTradeActionChanged(static_cast<int>(TradeAction::Sell));
    // Submit the order
    onSubmitClicked();
}

void OrderEntryWidget::executeBuyToCoverOrder()
{
    logInputEvent(u"OrderEntryWidget", u"quick-buy-to-cover");
    // Set trade action to Buy to Cover
    m_buyToCoverRadio->setChecked(true);
    // Explicitly trigger the trade action change to update button appearance
    onTradeActionChanged(static_cast<int>(TradeAction::BuyToCover));
    // Submit the order
    onSubmitClicked();
}

void OrderEntryWidget::executeSellToCoverOrder()
{
    logInputEvent(u"OrderEntryWidget", u"quick-sell-short");
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

    // Show/hide limit controls based on order type
    const bool needsLimitPrice = (orderType == OrderType::Type::Limit || orderType == OrderType::Type::StopLimit);
    if (m_limitControlsWidget)
    {
        m_limitControlsWidget->setVisible(needsLimitPrice);
    }
    if (m_stickyControlsWidget)
    {
        m_stickyControlsWidget->setEnabled(needsLimitPrice && !m_reviewModeEnabled && !m_currentSymbol.isEmpty() &&
                                           m_stickyEnabled);
    }

    // Show/hide stop controls based on order type
    const bool needsStopPrice = (orderType == OrderType::Type::StopMarket || orderType == OrderType::Type::StopLimit);
    if (m_stopControlsWidget)
    {
        m_stopControlsWidget->setVisible(needsStopPrice);
    }
}

void OrderEntryWidget::refreshOrderRoutes()
{
    if (MainApp::isInReplayMode() || TSClient::getInstance()->getMode() == TSClient::Mode::Replay)
    {
        QSignalBlocker blocker(m_routeCombo);
        m_routeCombo->clear();
        m_routeCombo->addItem(QStringLiteral("replay"), QStringLiteral("replay"));
        m_routeCombo->setCurrentIndex(0);
        m_routesRequestInFlight = false;
        return;
    }

    m_routesRequestInFlight = true;
    {
        QSignalBlocker blocker(m_routeCombo);
        m_routeCombo->clear();
        m_routeCombo->addItem("Loading...", QString());
        m_routeCombo->setCurrentIndex(0);
    }

    TSClient::getInstance()->getOrderRoutes().then(
        this,
        [this](std::expected<QVector<OrderRoute>, TSClient::Error> p_result)
        {
            if (MainApp::isInReplayMode() || TSClient::getInstance()->getMode() == TSClient::Mode::Replay)
            {
                m_routesRequestInFlight = false;
                updateInteractivity();
                return;
            }

            if (!p_result.has_value())
            {
                qWarning() << "Failed to fetch order routes, falling back to Intelligent:"
                           << static_cast<int>(p_result.error());
                populateFallbackLiveRoute();
            }
            else
            {
                qInfo() << "OrderEntryWidget received" << p_result->size() << "routes from getOrderRoutes()";
                for (const OrderRoute& route: p_result.value())
                {
                    qInfo() << "Order route candidate:" << "id=" << route.getId() << "name=" << route.getName()
                            << "assetTypes=" << route.getAssetTypes();
                }

                // TradeStation never returns "Intelligent" from GET_ORDER_ROUTES — it is implied by
                // omitting the Route field entirely. populateLiveRoutes() synthesizes that entry itself.
                QVector<OrderRoute> stockRoutes;
                stockRoutes.reserve(p_result->size());

                // Order entry currently supports STOCK routes only.
                // TODO: Expand this filter when options/futures order entry is implemented.
                for (const OrderRoute& route: p_result.value())
                {
                    if (route.supportsAssetType(QStringLiteral("STOCK")))
                    {
                        stockRoutes.push_back(route);
                    }
                }

                qInfo() << "OrderEntryWidget filtered to" << stockRoutes.size() << "stock routes";
                populateLiveRoutes(stockRoutes);
            }

            m_routesLoaded = true;
            m_routesRequestInFlight = false;
            updateInteractivity();
        });
}

void OrderEntryWidget::populateLiveRoutes(const QVector<OrderRoute>& p_routes)
{
    QSignalBlocker blocker(m_routeCombo);
    m_routeCombo->clear();

    // "Intelligent" is never returned by GET_ORDER_ROUTES — TradeStation infers it from the
    // absence of a Route field on the order. It's also the only commission-free route, so it's
    // synthesized here as the first entry and the default selection.
    m_routeCombo->addItem(QStringLiteral("Intelligent"),
                          QString::fromLatin1(OrderRoutingConstants::INTELLIGENT_ROUTE_ID));

    for (const OrderRoute& route: p_routes)
    {
        const QString routeId = route.getId().trimmed();
        if (routeId.isEmpty())
        {
            continue;
        }

        const QString routeName = route.getName().trimmed();
        const QString displayName = routeName.isEmpty() ? routeId : routeName;
        m_routeCombo->addItem(displayName, routeId);
    }

    QString savedRoute;
    if (appStateSettings != nullptr)
    {
        savedRoute = appStateSettings->value("OrderEntry/Route").toString().trimmed();
    }

    int selectedIndex = 0; // Default to "Intelligent" (index 0) when there's no saved preference.
    if (!savedRoute.isEmpty())
    {
        for (int i = 0; i < m_routeCombo->count(); ++i)
        {
            if (m_routeCombo->itemData(i).toString().compare(savedRoute, Qt::CaseInsensitive) == 0)
            {
                selectedIndex = i;
                break;
            }
        }
    }

    m_routeCombo->setCurrentIndex(selectedIndex);
    blocker.unblock();
    saveRouteSetting(selectedIndex);

    QStringList routeEntries;
    routeEntries.reserve(m_routeCombo->count());
    for (int i = 0; i < m_routeCombo->count(); ++i)
    {
        routeEntries.push_back(QString("%1(%2)").arg(m_routeCombo->itemText(i), m_routeCombo->itemData(i).toString()));
    }
    qInfo() << "OrderEntryWidget dropdown routes:" << routeEntries
            << "selected=" << m_routeCombo->itemData(m_routeCombo->currentIndex()).toString();
}

void OrderEntryWidget::populateFallbackLiveRoute()
{
    QSignalBlocker blocker(m_routeCombo);
    m_routeCombo->clear();
    m_routeCombo->addItem(QStringLiteral("Intelligent"),
                          QString::fromLatin1(OrderRoutingConstants::INTELLIGENT_ROUTE_ID));
    m_routeCombo->setCurrentIndex(0);
    blocker.unblock();
    saveRouteSetting(0);
    qInfo() << "OrderEntryWidget using fallback route list: Intelligent";
}

bool OrderEntryWidget::validateInputs()
{
    if (MainApp::isInReviewMode())
    {
        QMessageBox::information(this, "Review Mode", "Order entry is disabled while Review mode is active.");
        return false;
    }

    // Check GUIFrontend reference
    OBJ_ASSUME_TRUE(m_guiFrontend);

    // Check if in replay mode and replay is not running
    if (TSClient::getInstance()->getMode() == TSClient::Mode::Replay)
    {
        MainAlgo* mainAlgo = MainAlgo::getInstance();
        if (mainAlgo->getReplayState() != Playback::State::Playing)
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

    if (m_currentSymbol.isEmpty())
    {
        QMessageBox::warning(this, "Invalid Input", "Please select a symbol before placing an order.");
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

    if (MainApp::isInReplayMode() || TSClient::getInstance()->getMode() == TSClient::Mode::Replay)
    {
        request.setRoute(QStringLiteral("replay"));
    }
    else
    {
        const QString route = m_routeCombo->currentData().toString().trimmed();
        // "Intelligent" is a synthesized sentinel, not a real TradeStation route ID — the API
        // infers intelligent routing from the absence of the Route field, so leave it unset.
        if (!route.isEmpty() &&
            route.compare(QString::fromLatin1(OrderRoutingConstants::INTELLIGENT_ROUTE_ID), Qt::CaseInsensitive) != 0)
        {
            request.setRoute(route);
        }
    }

    return request;
}

void OrderEntryWidget::onSubmitClicked()
{
    const OrderType::Type orderType = static_cast<OrderType::Type>(m_orderTypeCombo->currentData().toInt());
    const bool needsLimitPrice = (orderType == OrderType::Type::Limit || orderType == OrderType::Type::StopLimit);
    if (m_stickyEnabled && needsLimitPrice)
    {
        if (!updateStickyPrice())
        {
            const TradeAction action = static_cast<TradeAction>(m_tradeActionGroup->checkedId());
            const bool isBuyAction = action == TradeAction::Buy || action == TradeAction::BuyToCover ||
                                     action == TradeAction::BuyToOpen || action == TradeAction::BuyToClose;
            const QString requiredSide =
                (isBuyAction == m_stickyAggressiveMode) ? QStringLiteral("ask") : QStringLiteral("bid");
            const QString symbol = m_currentSymbol.isEmpty() ? QStringLiteral("(no symbol)") : m_currentSymbol;

            QMessageBox::warning(this,
                                 "Sticky Price Unavailable",
                                 QString("Sticky pricing is enabled, but no valid %1 quote is available for %2.\n\n"
                                         "Wait for Level2 quotes or disable Sticky and enter a manual limit price.")
                                     .arg(requiredSide, symbol));
            logInputEvent(u"OrderEntryWidget",
                          u"submit-order-blocked",
                          {inputDetail(u"reason", QStringLiteral("sticky-no-quote")),
                           inputDetail(u"symbol", symbol),
                           inputDetail(u"requiredSide", requiredSide)});
            return;
        }
    }

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
                                         "Quantity: %4\n"
                                         "Route: %5\n")
                                     .arg(order.getSymbol(),
                                          actionText,
                                          m_orderTypeCombo->currentText(),
                                          QString::number(order.getQuantity()),
                                          order.getRoute().value_or(QStringLiteral("Intelligent")));

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
    QAbstractButton* checkedButton = m_tradeActionGroup->checkedButton();
    const QString actionText = checkedButton != nullptr ? checkedButton->text() : QString("Unknown");
    QStringList details = {inputDetail(u"symbol", order.getSymbol()),
                           inputDetail(u"quantity", order.getQuantity()),
                           inputDetail(u"orderType", m_orderTypeCombo->currentText()),
                           inputDetail(u"tradeAction", actionText),
                           inputDetail(u"accountId", order.getAccountID()),
                           inputDetail(u"route", order.getRoute().value_or(QStringLiteral("Intelligent")))};
    if (order.getLimitPrice().has_value())
    {
        details << inputDetail(u"limitPrice", QString::number(order.getLimitPrice().value(), 'f', 2));
    }
    if (order.getStopPrice().has_value())
    {
        details << inputDetail(u"stopPrice", QString::number(order.getStopPrice().value(), 'f', 2));
    }
    logInputEvent(u"OrderEntryWidget", u"submit-order", details);
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

    const double savedManualBracketStopLossPercent =
        appStateSettings->value("OrderEntry/ManualBracketStopLossPercent", 10.0).toDouble();
    m_manualBracketStopLossPercentInput->setValue(savedManualBracketStopLossPercent);

    const double savedManualBracketTakeProfitPercent =
        appStateSettings->value("OrderEntry/ManualBracketTakeProfitPercent", 10.0).toDouble();
    m_manualBracketTakeProfitPercentInput->setValue(savedManualBracketTakeProfitPercent);

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

void OrderEntryWidget::saveRouteSetting(int index)
{
    if (MainApp::isInReplayMode() || TSClient::getInstance()->getMode() == TSClient::Mode::Replay)
    {
        return;
    }

    if (index < 0 || index >= m_routeCombo->count())
    {
        return;
    }

    const QString route = m_routeCombo->itemData(index).toString().trimmed();
    if (route.isEmpty())
    {
        return;
    }

    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/Route", route);
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

void OrderEntryWidget::saveManualBracketStopLossPercentSetting(double value)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/ManualBracketStopLossPercent", value);
    appStateSettings->sync();
}

void OrderEntryWidget::saveManualBracketTakeProfitPercentSetting(double value)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/ManualBracketTakeProfitPercent", value);
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
    if (isVisible())
    {
        logInputEvent(u"OrderEntryWidget", u"toggle-order-confirmation", {inputDetail(u"enabled", checked)});
    }
    qInfo() << "Order confirmation" << (checked ? "enabled" : "disabled");
}

void OrderEntryWidget::onResultPopupCheckBoxToggled(bool checked)
{
    m_resultPopupEnabled = checked;
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/ResultPopupEnabled", checked);
    appStateSettings->sync();
    if (isVisible())
    {
        logInputEvent(u"OrderEntryWidget", u"toggle-result-popup", {inputDetail(u"enabled", checked)});
    }
    qInfo() << "Order result popup" << (checked ? "enabled" : "disabled");
}

void OrderEntryWidget::onCancelAllConfirmationCheckBoxToggled(bool checked)
{
    m_cancelAllConfirmationEnabled = checked;
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("OrderEntry/CancelAllConfirmationEnabled", checked);
    appStateSettings->sync();
    if (isVisible())
    {
        logInputEvent(u"OrderEntryWidget", u"toggle-cancel-all-confirmation", {inputDetail(u"enabled", checked)});
    }
    qInfo() << "Cancel all orders confirmation" << (checked ? "enabled" : "disabled");
}

void OrderEntryWidget::onStickyCheckBoxToggled(bool checked)
{
    m_stickyEnabled = checked;
    if (isVisible())
    {
        logInputEvent(u"OrderEntryWidget", u"toggle-sticky-price", {inputDetail(u"enabled", checked)});
    }

    // Enable/disable the mode and offset controls based on sticky state
    const bool hasSymbol = !m_currentSymbol.isEmpty();
    const OrderType::Type orderType = static_cast<OrderType::Type>(m_orderTypeCombo->currentData().toInt());
    const bool needsLimitPrice = (orderType == OrderType::Type::Limit || orderType == OrderType::Type::StopLimit);
    m_stickyControlsWidget->setEnabled(checked && needsLimitPrice && !m_reviewModeEnabled && hasSymbol);

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
    if (isVisible())
    {
        logInputEvent(
            u"OrderEntryWidget",
            u"set-sticky-offset",
            {inputDetail(u"value", QString::number(value, 'f', 2)), inputDetail(u"enabled", m_stickyEnabled)});
    }
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
    if (isVisible())
    {
        logInputEvent(u"OrderEntryWidget",
                      u"set-sticky-mode",
                      {inputDetail(u"mode", id == 0 ? "aggressive" : "passive")});
    }
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

void OrderEntryWidget::onMarketDepthUpdate(const QString& symbol, const Level2& level2)
{
    // Only update if this is for the current symbol
    if (symbol != m_currentSymbol)
    {
        return;
    }

    // Extract best bid and ask prices
    const auto& bids = level2.m_bids;
    const auto& asks = level2.m_asks;

    const double bestBid = bids[0].m_price;
    const double bestAsk = asks[0].m_price;
    m_hasValidBestBid = std::isfinite(bestBid) && bestBid > 0.0;
    m_hasValidBestAsk = std::isfinite(bestAsk) && bestAsk > 0.0;
    m_lastBestBid = m_hasValidBestBid ? bestBid : 0.0;
    m_lastBestAsk = m_hasValidBestAsk ? bestAsk : 0.0;

    // Update sticky price if enabled
    if (m_stickyEnabled)
    {
        updateStickyPrice();
    }
}

bool OrderEntryWidget::updateStickyPrice()
{
    if (!m_stickyEnabled)
    {
        return false;
    }

    TradeAction action = static_cast<TradeAction>(m_tradeActionGroup->checkedId());
    double offset = m_stickyOffsetInput->value();
    double finalPrice = 0.0;
    bool hasRequiredQuote = false;

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
            if (m_hasValidBestAsk)
            {
                finalPrice = m_lastBestAsk + offset;
                hasRequiredQuote = true;
            }
        }
        else
        {
            // Passive: Enter on bid (Bid + offset) for better price
            if (m_hasValidBestBid)
            {
                finalPrice = m_lastBestBid + offset;
                hasRequiredQuote = true;
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
            if (m_hasValidBestBid)
            {
                finalPrice = m_lastBestBid - offset;
                hasRequiredQuote = true;
            }
        }
        else
        {
            // Passive: Enter on ask (Ask - offset) for better price
            if (m_hasValidBestAsk)
            {
                finalPrice = m_lastBestAsk - offset;
                hasRequiredQuote = true;
            }
        }

        // Ensure price doesn't go negative
        if (finalPrice < 0.01)
        {
            finalPrice = 0.01;
        }
        break;
    }

    if (hasRequiredQuote && std::isfinite(finalPrice) && finalPrice > 0.0)
    {
        m_limitPriceInput->setValue(finalPrice);
        return true;
    }
    return false;
}
