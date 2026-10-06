#include "PositionWidget.h"
#include <QAction>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <optional>
#include <QJsonDocument>
#include "Assume.h"
#include "CONSTANTS.h"
#include "Core/MainApp.h"
#include "LTTng/LTTngTracepoints.h"
#include "Misc/Logging/Logging.h"

namespace
{
    [[nodiscard]] bool isOpenPosition(const Position& p_position)
    {
        return !qFuzzyCompare(1.0 + qAbs(p_position.getQuantity().toDouble()), 1.0);
    }

    [[nodiscard]] QDateTime positionOpenedDateTimeForFiltering(const Position& p_position)
    {
        QDateTime openedDateTime = p_position.getOpenedDateTime();
        if (!openedDateTime.isValid())
        {
            openedDateTime = p_position.getTimestamp();
        }
        return openedDateTime;
    }

    [[nodiscard]] QDateTime positionOrderingDateTime(const Position& p_position)
    {
        const QDateTime openedDateTime = p_position.getOpenedDateTime();
        if (openedDateTime.isValid())
        {
            return openedDateTime;
        }

        // For closed positions that still don't carry OpenedDateTime (legacy/replay edge cases),
        // use the close/update timestamp as a deterministic fallback.
        if (!isOpenPosition(p_position) && p_position.getTimestamp().isValid())
        {
            return p_position.getTimestamp();
        }

        return QDateTime();
    }

    [[nodiscard]] Position
    withLifecycleDateTimeField(const Position& p_position, const char* p_fieldName, const QDateTime& p_dateTime)
    {
        if (!p_dateTime.isValid())
        {
            return p_position;
        }

        const QJsonDocument jsonDocument = QJsonDocument::fromJson(p_position.toJsonString().toUtf8());
        if (!jsonDocument.isObject())
        {
            return p_position;
        }

        QJsonObject jsonObject = jsonDocument.object();
        jsonObject[QString::fromUtf8(p_fieldName)] = p_dateTime.toString(Qt::ISODate);
        return Position(jsonObject, p_position.isPositionUpdate());
    }

    [[nodiscard]] Position ensureOpenedDateTimeFromFirstSeenTimestamp(const Position& p_position)
    {
        if (p_position.getOpenedDateTime().isValid() || !p_position.getTimestamp().isValid())
        {
            return p_position;
        }

        return withLifecycleDateTimeField(p_position, "OpenedDateTime", p_position.getTimestamp());
    }

    [[nodiscard]] Position inheritLifecycleDateTimes(const Position& p_position, const Position& p_existing)
    {
        const bool needsOpenedDateTime =
            !p_position.getOpenedDateTime().isValid() && p_existing.getOpenedDateTime().isValid();
        const bool needsClosedDateTime =
            !p_position.getClosedDateTime().isValid() && p_existing.getClosedDateTime().isValid();
        if (!needsOpenedDateTime && !needsClosedDateTime)
        {
            return p_position;
        }

        const QJsonDocument jsonDocument = QJsonDocument::fromJson(p_position.toJsonString().toUtf8());
        if (!jsonDocument.isObject())
        {
            return p_position;
        }

        QJsonObject jsonObject = jsonDocument.object();
        if (needsOpenedDateTime)
        {
            jsonObject["OpenedDateTime"] = p_existing.getOpenedDateTime().toString(Qt::ISODate);
        }
        if (needsClosedDateTime)
        {
            jsonObject["ClosedDateTime"] = p_existing.getClosedDateTime().toString(Qt::ISODate);
        }

        return Position(jsonObject, p_position.isPositionUpdate());
    }

    [[nodiscard]] std::optional<Position> buildClosedPosition(const Position& p_position)
    {
        const QJsonDocument jsonDocument = QJsonDocument::fromJson(p_position.toJsonString().toUtf8());
        if (!jsonDocument.isObject())
        {
            return std::nullopt;
        }

        QJsonObject jsonObject = jsonDocument.object();
        jsonObject["Deleted"] = true;
        jsonObject["Quantity"] = QStringLiteral("0");
        jsonObject["MarketValue"] = QStringLiteral("0");
        jsonObject["UnrealizedProfitLoss"] = QStringLiteral("0");
        jsonObject["UnrealizedProfitLossPercent"] = QStringLiteral("0");
        jsonObject["UnrealizedProfitLossQty"] = QStringLiteral("0");
        jsonObject["Timestamp"] = MainApp::getCurrentAppTime().toString(Qt::ISODate);
        if (!p_position.getClosedDateTime().isValid())
        {
            jsonObject["ClosedDateTime"] = MainApp::getCurrentAppTime().toString(Qt::ISODate);
        }
        return Position(jsonObject, true);
    }
} // namespace

PositionWidget::PositionWidget(QWidget* parent)
    : QWidget(parent)
    , tableView(new QTableView(this))
    , model(new QStandardItemModel(this))
    , m_headerWidget(new QWidget(this))
    , headerLabel(new QLabel("POSITIONS", this))
    , m_closeAllPositionsButton(new QPushButton("Close All", this))
    , m_closeAllPositionsPassiveButton(new QPushButton("Close All Passive", this))
    , m_settingsButton(new QToolButton(this))
    , m_settingsMenu(new QMenu(this))
    , m_currentDayOnlyAction(new QAction("Current Day Only", this))
{
    setupUI();
    setupStyles();
}

PositionWidget::~PositionWidget()
{
    // Qt will handle deletion of child widgets
}

void PositionWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Setup header
    m_headerWidget->setFixedHeight(24);
    headerLabel->setAlignment(Qt::AlignCenter);
    m_closeAllPositionsButton->setFixedHeight(20);
    m_closeAllPositionsPassiveButton->setFixedHeight(20);
    m_closeAllPositionsButton->setCursor(Qt::PointingHandCursor);
    m_closeAllPositionsPassiveButton->setCursor(Qt::PointingHandCursor);
    m_closeAllPositionsButton->setToolTip("Close all open positions for the selected account");
    m_closeAllPositionsPassiveButton->setToolTip(
        "Close all open positions with passive resting prices in extended-hours sessions");
    m_settingsButton->setText("⚙");
    m_settingsButton->setFixedSize(20, 20);
    m_settingsButton->setToolTip("Positions view settings");
    m_settingsButton->setPopupMode(QToolButton::InstantPopup);

    m_currentDayOnlyAction->setCheckable(true);
    m_currentDayOnlyAction->setChecked(false);
    m_settingsMenu->addAction(m_currentDayOnlyAction);
    m_settingsButton->setMenu(m_settingsMenu);

    QHBoxLayout* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(6, 0, 6, 0);
    headerLayout->setSpacing(6);
    headerLayout->addStretch();
    headerLayout->addWidget(headerLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_closeAllPositionsPassiveButton);
    headerLayout->addWidget(m_closeAllPositionsButton);
    headerLayout->addWidget(m_settingsButton);

    auto closeAllConnection = connect(m_closeAllPositionsButton,
                                      &QPushButton::clicked,
                                      this,
                                      &PositionWidget::closeAllPositionsRequested,
                                      Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(closeAllConnection);
    auto closeAllPassiveConnection = connect(m_closeAllPositionsPassiveButton,
                                             &QPushButton::clicked,
                                             this,
                                             &PositionWidget::closeAllPositionsPassiveRequested,
                                             Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(closeAllPassiveConnection);
    auto currentDayOnlyConnection = connect(m_currentDayOnlyAction,
                                            &QAction::toggled,
                                            this,
                                            &PositionWidget::onCurrentDayOnlyToggled,
                                            Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(currentDayOnlyConnection);

    // Setup model columns (Position ID at END like OrderWidget)
    QStringList headers;
    headers << "Symbol" << "Quantity" << "Avg Price" << "Last" << "Unrealized P/L" << "Realized P/L" << "Market Value"
            << "Position ID";
    model->setHorizontalHeaderLabels(headers);

    // Configure table view
    tableView->setModel(model);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableView->verticalHeader()->setVisible(false);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setAlternatingRowColors(true);
    tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    // Connect click signal
    auto symbolPressConnection =
        connect(tableView, &QTableView::pressed, this, &PositionWidget::onSymbolClicked, Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(symbolPressConnection);
    auto contextMenuConnection = connect(tableView,
                                         &QTableView::customContextMenuRequested,
                                         this,
                                         &PositionWidget::onCustomContextMenuRequested,
                                         Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(contextMenuConnection);

    // Set column widths
    tableView->setColumnWidth(0, 70); // Symbol
    tableView->setColumnWidth(1, 70); // Quantity
    tableView->setColumnWidth(2, 70); // Avg Price
    tableView->setColumnWidth(3, 70); // Last
    tableView->setColumnWidth(4, 90); // Unrealized P/L
    tableView->setColumnWidth(5, 90); // Realized P/L
    tableView->setColumnWidth(6, 90); // Market Value
    tableView->setColumnWidth(7, 90); // Position ID

    // Add widgets to layout
    mainLayout->addWidget(m_headerWidget);
    mainLayout->addWidget(tableView);

    // Set fixed width based on total column widths
    int totalWidth = 0;
    for (int i = 0; i < headers.size(); ++i)
    {
        totalWidth += tableView->columnWidth(i);
    }
    setFixedWidth(totalWidth);
}

void PositionWidget::setupStyles()
{
    m_headerWidget->setStyleSheet(QStringLiteral("QWidget {"
                                                 "   background-color: %1;"
                                                 "   border-bottom: 1px solid %2;"
                                                 "}")
                                      .arg(QString::fromLatin1(GUIThemeConstants::SIDEBAR_BACKGROUND))
                                      .arg(QString::fromLatin1(GUIThemeConstants::BORDER)));
    headerLabel->setStyleSheet("QLabel { color: #FFFFFF; background: transparent; }");
    m_closeAllPositionsButton->setStyleSheet("QPushButton {"
                                             "   background-color: #7A1F1F;"
                                             "   color: #FFFFFF;"
                                             "   border: 1px solid #A63A3A;"
                                             "   border-radius: 3px;"
                                             "   padding: 0 8px;"
                                             "}"
                                             "QPushButton:hover {"
                                             "   background-color: #9B2C2C;"
                                             "}"
                                             "QPushButton:pressed {"
                                             "   background-color: #5F1919;"
                                             "}");
    m_closeAllPositionsPassiveButton->setStyleSheet("QPushButton {"
                                                    "   background-color: #1F477A;"
                                                    "   color: #FFFFFF;"
                                                    "   border: 1px solid #3E6AA3;"
                                                    "   border-radius: 3px;"
                                                    "   padding: 0 8px;"
                                                    "}"
                                                    "QPushButton:hover {"
                                                    "   background-color: #2A5E9E;"
                                                    "}"
                                                    "QPushButton:pressed {"
                                                    "   background-color: #1A3A63;"
                                                    "}");
    m_settingsButton->setStyleSheet("QToolButton {"
                                    "   background-color: #3A3A3A;"
                                    "   color: #FFFFFF;"
                                    "   border: 1px solid #595959;"
                                    "   border-radius: 3px;"
                                    "}"
                                    "QToolButton:hover {"
                                    "   background-color: #4A4A4A;"
                                    "}"
                                    "QToolButton:pressed {"
                                    "   background-color: #2A2A2A;"
                                    "}");
}

void PositionWidget::setReviewModeEnabled(const bool p_enabled)
{
    m_reviewModeEnabled = p_enabled;
    m_closeAllPositionsButton->setEnabled(!p_enabled);
    m_closeAllPositionsPassiveButton->setEnabled(!p_enabled);

    if (p_enabled)
    {
        m_closeAllPositionsButton->setText("Read-Only");
        m_closeAllPositionsPassiveButton->setText("Read-Only");
    }
    else
    {
        m_closeAllPositionsButton->setText("Close All");
        m_closeAllPositionsPassiveButton->setText("Close All Passive");
    }
}

void PositionWidget::updatePosition(const QString& account, const Position& position)
{
    LTTnG_TP(opentraderplatform, gui_position_widget_update);

    Q_UNUSED(account);
    Position positionToStore = position;
    const QString positionId = position.getPositionID();
    const auto existingPositionIt = m_positionsById.constFind(positionId);
    const bool hadExistingPosition = existingPositionIt != m_positionsById.cend();
    const Position previousPosition = hadExistingPosition ? existingPositionIt.value() : Position();
    if (existingPositionIt != m_positionsById.cend())
    {
        positionToStore = inheritLifecycleDateTimes(position, existingPositionIt.value());
    }

    positionToStore = ensureOpenedDateTimeFromFirstSeenTimestamp(positionToStore);

    m_positionsById[positionId] = positionToStore;

    if (!hadExistingPosition)
    {
        rebuildTable();
        return;
    }

    const bool wasVisible = shouldShowPosition(previousPosition);
    const bool isVisible = shouldShowPosition(positionToStore);
    const bool openStateChanged = isOpenPosition(previousPosition) != isOpenPosition(positionToStore);
    const bool orderingChanged =
        positionOrderingDateTime(previousPosition) != positionOrderingDateTime(positionToStore);

    if (!wasVisible && !isVisible)
    {
        return;
    }

    if (wasVisible != isVisible || openStateChanged || orderingChanged)
    {
        rebuildTable();
        return;
    }

    const auto rowIt = positionRowMap.constFind(positionId);
    if (rowIt == positionRowMap.cend())
    {
        rebuildTable();
        return;
    }

    const int row = rowIt.value();
    if (row < 0 || row >= model->rowCount())
    {
        rebuildTable();
        return;
    }

    QList<QStandardItem*> rowItems = createRowItems(positionToStore);
    for (int col = 0; col < rowItems.size(); ++col)
    {
        model->setItem(row, col, rowItems.at(col));
    }
}

void PositionWidget::onCurrentDayOnlyToggled(const bool p_checked)
{
    m_showCurrentDayOnly = p_checked;
    rebuildTable();
    logInputEvent(u"PositionWidget",
                  u"toggle-current-day-only",
                  {inputDetail(u"enabled", p_checked), inputDetail(u"visibleRows", model->rowCount())});
}

bool PositionWidget::shouldShowPosition(const Position& p_position) const
{
    if (!m_showCurrentDayOnly)
    {
        return true;
    }

    if (isOpenPosition(p_position))
    {
        return true;
    }

    const QDateTime positionTime =
        positionOpenedDateTimeForFiltering(p_position).toTimeZone(TradingHours::MARKET_TIMEZONE);
    if (!positionTime.isValid())
    {
        return false;
    }

    const QDate currentDate = MainApp::getCurrentAppTime().date();
    return positionTime.date() == currentDate;
}

void PositionWidget::rebuildTable()
{
    model->removeRows(0, model->rowCount());
    positionRowMap.clear();

    QVector<Position> orderedPositions = m_positionsById.values().toVector();
    std::sort(orderedPositions.begin(),
              orderedPositions.end(),
              [](const Position& p_left, const Position& p_right)
              {
                  const bool leftIsOpen = isOpenPosition(p_left);
                  const bool rightIsOpen = isOpenPosition(p_right);
                  if (leftIsOpen != rightIsOpen)
                  {
                      return leftIsOpen && !rightIsOpen;
                  }

                  const QDateTime leftOpenedDateTime = positionOrderingDateTime(p_left);
                  const QDateTime rightOpenedDateTime = positionOrderingDateTime(p_right);
                  if (leftOpenedDateTime.isValid() != rightOpenedDateTime.isValid())
                  {
                      return leftOpenedDateTime.isValid();
                  }
                  if (leftOpenedDateTime != rightOpenedDateTime)
                  {
                      return leftOpenedDateTime > rightOpenedDateTime;
                  }

                  return p_left.getPositionID() < p_right.getPositionID();
              });

    for (const Position& position: orderedPositions)
    {
        if (!shouldShowPosition(position))
        {
            continue;
        }

        QList<QStandardItem*> rowItems = createRowItems(position);
        const int row = model->rowCount();
        model->insertRow(row, rowItems);
        positionRowMap[position.getPositionID()] = row;
    }
}

QList<QStandardItem*> PositionWidget::createRowItems(const Position& position)
{
    QList<QStandardItem*> items;

    // Symbol
    auto symbolItem = new QStandardItem(position.getSymbol());
    symbolItem->setTextAlignment(Qt::AlignCenter);
    items << symbolItem;

    // Quantity
    auto quantityItem = new QStandardItem(position.getQuantity());
    quantityItem->setTextAlignment(Qt::AlignCenter);
    items << quantityItem;

    // Average Price
    auto avgPriceItem = new QStandardItem(QString::number(position.getAveragePrice().toDouble(), 'f', 2));
    avgPriceItem->setTextAlignment(Qt::AlignCenter);
    items << avgPriceItem;

    // Last Price
    auto lastItem = new QStandardItem(QString::number(position.getLast().toDouble(), 'f', 2));
    lastItem->setTextAlignment(Qt::AlignCenter);
    items << lastItem;

    // Unrealized P/L (only for open positions)
    int qty = position.getQuantity().toInt();
    double unrealizedPL = position.getUnrealizedProfitLoss().toDouble();
    auto unrealizedItem = new QStandardItem();
    unrealizedItem->setTextAlignment(Qt::AlignCenter);
    if (qty != 0)
    {
        unrealizedItem->setText(QString::number(unrealizedPL, 'f', 2));
        unrealizedItem->setForeground(unrealizedPL >= 0 ? QColor(Qt::green) : QColor(Qt::red));
    }
    // else: leave empty for closed positions
    items << unrealizedItem;

    // Realized P/L (only for closed positions, uses TodaysProfitLoss field)
    double realizedPL = position.getTodaysProfitLoss().toDouble();
    auto realizedItem = new QStandardItem();
    realizedItem->setTextAlignment(Qt::AlignCenter);
    if (qty == 0)
    {
        QString text = QString::number(realizedPL, 'f', 2);
        realizedItem->setText(text);
        realizedItem->setForeground(realizedPL >= 0 ? QColor(Qt::green) : QColor(Qt::red));
    }
    // else: leave empty for open positions
    items << realizedItem;

    // Market Value
    auto marketValueItem = new QStandardItem(QString::number(position.getMarketValue().toDouble(), 'f', 2));
    marketValueItem->setTextAlignment(Qt::AlignCenter);
    items << marketValueItem;

    // Position ID (at end, like OrderWidget)
    auto positionIDItem = new QStandardItem(position.getPositionID());
    positionIDItem->setTextAlignment(Qt::AlignCenter);
    items << positionIDItem;

    return items;
}

void PositionWidget::onPositionDeleted(const QString& account, const QString& positionID)
{
    Q_UNUSED(account);

    const auto positionIt = m_positionsById.constFind(positionID);
    if (positionIt != m_positionsById.cend())
    {
        const std::optional<Position> closedPosition = buildClosedPosition(positionIt.value());
        if (closedPosition.has_value())
        {
            m_positionsById[positionID] = closedPosition.value();
        }
    }

    rebuildTable();
}

void PositionWidget::onSymbolClicked(const QModelIndex& index)
{
    if (!index.isValid())
    {
        return;
    }

    if (index.column() == 0)
    { // Only handle clicks on the Symbol column
        QStandardItem* const symbolItem = model->item(index.row(), 0);
        if (symbolItem == nullptr)
        {
            return;
        }

        const QString symbol = symbolItem->text();
        logInputEvent(u"PositionWidget", u"select-symbol", {inputDetail(u"symbol", symbol)});
        emit symbolClicked(symbol);
    }
}

void PositionWidget::onCustomContextMenuRequested(const QPoint& p_pos)
{
    if (m_reviewModeEnabled)
    {
        return;
    }

    const QModelIndex index = tableView->indexAt(p_pos);
    if (!index.isValid())
    {
        return;
    }

    QStandardItem* const quantityItem = model->item(index.row(), 1);
    QStandardItem* const positionIDItem = model->item(index.row(), 7);
    if (quantityItem == nullptr || positionIDItem == nullptr)
    {
        return;
    }

    bool quantityOk = false;
    const double quantity = quantityItem->text().toDouble(&quantityOk);
    if (!quantityOk || qFuzzyCompare(1.0 + qAbs(quantity), 1.0))
    {
        return;
    }

    const QString positionID = positionIDItem->text().trimmed();
    if (positionID.isEmpty())
    {
        return;
    }

    QMenu menu(this);
    QAction* const closeAction = menu.addAction("Close Position");
    Q_CHECK_PTR(closeAction);

    if (menu.exec(tableView->viewport()->mapToGlobal(p_pos)) == closeAction)
    {
        logInputEvent(u"PositionWidget", u"context-close-position", {inputDetail(u"positionId", positionID)});
        emit closePositionRequested(positionID);
    }
}


void PositionWidget::clearAllPositions()
{
    model->removeRows(0, model->rowCount());
    positionRowMap.clear();
    m_positionsById.clear();
    qDebug() << "PositionWidget cleared all positions";
}
