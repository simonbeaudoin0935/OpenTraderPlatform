#include "RecordsInfoTab.h"
#include "DBClient.h"
#include "Logging.h"

#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QRegularExpression>
#include <QSet>

RecordsInfoTab::RecordsInfoTab(QWidget* p_parent)
    : QWidget(p_parent)
    , m_daysTable(nullptr)
    , m_refreshButton(nullptr)
    , m_symbolsTable(nullptr)
    , m_symbolLabel(nullptr)
    , m_mbp10GroupBox(nullptr)
    , m_mbp10StatusLabel(nullptr)
    , m_mbp10SizeLabel(nullptr)
    , m_mbp10PathLabel(nullptr)
    , m_tradesGroupBox(nullptr)
    , m_tradesStatusLabel(nullptr)
    , m_tradesSizeLabel(nullptr)
    , m_tradesPathLabel(nullptr)
{
    setupUI();
    scanRecordedDays();
}

void RecordsInfoTab::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Toolbar
    auto* toolbar = new QHBoxLayout();
    m_refreshButton = new QPushButton("Refresh");
    Q_CHECK_PTR(m_refreshButton);
    toolbar->addWidget(m_refreshButton);
    toolbar->addStretch();
    mainLayout->addLayout(toolbar);

    connect(m_refreshButton, &QPushButton::clicked, this, &RecordsInfoTab::onRefreshClicked, Qt::UniqueConnection);

    // Three-column splitter
    auto* splitter = new QSplitter(Qt::Horizontal);

    // Left: Days table
    auto* daysWidget = new QWidget();
    auto* daysLayout = new QVBoxLayout(daysWidget);
    daysLayout->setContentsMargins(0, 0, 0, 0);
    daysLayout->addWidget(new QLabel("<b>Recorded Days</b>"));

    m_daysTable = new QTableWidget();
    Q_CHECK_PTR(m_daysTable);
    m_daysTable->setColumnCount(3);
    m_daysTable->setHorizontalHeaderLabels({"Date", "Files", "Total Size"});
    m_daysTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_daysTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_daysTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_daysTable->horizontalHeader()->setStretchLastSection(true);
    m_daysTable->verticalHeader()->setVisible(false);
    daysLayout->addWidget(m_daysTable);
    splitter->addWidget(daysWidget);

    connect(m_daysTable,
            &QTableWidget::itemSelectionChanged,
            this,
            &RecordsInfoTab::onDaySelected,
            Qt::UniqueConnection);

    // Middle: Symbols table
    auto* symbolsWidget = new QWidget();
    auto* symbolsLayout = new QVBoxLayout(symbolsWidget);
    symbolsLayout->setContentsMargins(0, 0, 0, 0);
    symbolsLayout->addWidget(new QLabel("<b>Symbols</b>"));

    m_symbolsTable = new QTableWidget();
    Q_CHECK_PTR(m_symbolsTable);
    m_symbolsTable->setColumnCount(3);
    m_symbolsTable->setHorizontalHeaderLabels({"Symbol", "Mbp10", "Trades"});
    m_symbolsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_symbolsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_symbolsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_symbolsTable->horizontalHeader()->setStretchLastSection(true);
    m_symbolsTable->verticalHeader()->setVisible(false);
    symbolsLayout->addWidget(m_symbolsTable);
    splitter->addWidget(symbolsWidget);

    connect(m_symbolsTable,
            &QTableWidget::itemSelectionChanged,
            this,
            &RecordsInfoTab::onStockSelected,
            Qt::UniqueConnection);

    // Right: Details
    auto* detailsWidget = new QWidget();
    auto* detailsLayout = new QVBoxLayout(detailsWidget);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    detailsLayout->addWidget(new QLabel("<b>File Details</b>"));

    m_symbolLabel = new QLabel("No symbol selected");
    Q_CHECK_PTR(m_symbolLabel);
    m_symbolLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    detailsLayout->addWidget(m_symbolLabel);

    // Mbp10 group
    m_mbp10GroupBox = new QGroupBox("Mbp10 (Level 2)");
    auto* mbp10Layout = new QVBoxLayout(m_mbp10GroupBox);
    m_mbp10StatusLabel = new QLabel("Status: —");
    m_mbp10SizeLabel = new QLabel("Size: —");
    m_mbp10PathLabel = new QLabel("Path: —");
    m_mbp10PathLabel->setWordWrap(true);
    mbp10Layout->addWidget(m_mbp10StatusLabel);
    mbp10Layout->addWidget(m_mbp10SizeLabel);
    mbp10Layout->addWidget(m_mbp10PathLabel);
    detailsLayout->addWidget(m_mbp10GroupBox);

    // Trades group
    m_tradesGroupBox = new QGroupBox("Trades");
    auto* tradesLayout = new QVBoxLayout(m_tradesGroupBox);
    m_tradesStatusLabel = new QLabel("Status: —");
    m_tradesSizeLabel = new QLabel("Size: —");
    m_tradesPathLabel = new QLabel("Path: —");
    m_tradesPathLabel->setWordWrap(true);
    tradesLayout->addWidget(m_tradesStatusLabel);
    tradesLayout->addWidget(m_tradesSizeLabel);
    tradesLayout->addWidget(m_tradesPathLabel);
    detailsLayout->addWidget(m_tradesGroupBox);

    detailsLayout->addStretch();
    splitter->addWidget(detailsWidget);

    // Set proportions
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 3);

    mainLayout->addWidget(splitter);
}

void RecordsInfoTab::onRefreshClicked()
{
    scanRecordedDays();
}

void RecordsInfoTab::scanRecordedDays()
{
    m_daysTable->setRowCount(0);
    clearSymbolsList();
    clearDetailsDisplay();

    // Scan ReplayData directory for date folders
    // Use a known date to derive the base directory, then go up one level
    QString baseDir = DBClient::getReplayDataDir(QDate::currentDate());
    QDir base(baseDir);
    base.cdUp(); // Go from ReplayData/YYYY-MM-DD to ReplayData/

    if (!base.exists())
    {
        return;
    }

    QStringList dateDirs = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);

    for (const QString& dirName: dateDirs)
    {
        QDate date = QDate::fromString(dirName, Qt::ISODate);
        if (!date.isValid())
            continue;

        QDir dateDir(base.absoluteFilePath(dirName));
        QStringList dbnFiles = dateDir.entryList({"*.dbn.zst"}, QDir::Files);

        if (dbnFiles.isEmpty())
            continue;

        qint64 totalSize = 0;
        for (const QString& f: dbnFiles)
        {
            totalSize += QFileInfo(dateDir.absoluteFilePath(f)).size();
        }

        int row = m_daysTable->rowCount();
        m_daysTable->insertRow(row);

        auto* dateItem = new QTableWidgetItem(date.toString(Qt::ISODate));
        dateItem->setData(Qt::UserRole, date);
        m_daysTable->setItem(row, 0, dateItem);
        m_daysTable->setItem(row, 1, new QTableWidgetItem(QString::number(dbnFiles.size())));
        m_daysTable->setItem(row, 2, new QTableWidgetItem(formatFileSize(totalSize)));
    }

    m_daysTable->resizeColumnsToContents();
}

void RecordsInfoTab::onDaySelected()
{
    QList<QTableWidgetItem*> selected = m_daysTable->selectedItems();
    if (selected.isEmpty())
        return;

    int row = selected.first()->row();
    QTableWidgetItem* dateItem = m_daysTable->item(row, 0);
    if (dateItem == nullptr)
        return;

    QDate date = dateItem->data(Qt::UserRole).toDate();
    if (!date.isValid() || date == m_selectedDate)
        return;

    m_selectedDate = date;
    loadSymbolsForDay(date);
}

void RecordsInfoTab::loadSymbolsForDay(const QDate& p_date)
{
    clearSymbolsList();
    clearDetailsDisplay();

    QString dirPath = DBClient::getReplayDataDir(p_date);
    QDir dir(dirPath);
    if (!dir.exists())
        return;

    QStringList files = dir.entryList({"*.dbn.zst"}, QDir::Files);

    // Extract unique symbols from filenames: {symbol}_mbp10.dbn.zst, {symbol}_trades.dbn.zst
    static const QRegularExpression mbp10Re("^(.+)_mbp10\\.dbn\\.zst$");
    static const QRegularExpression tradesRe("^(.+)_trades\\.dbn\\.zst$");

    QMap<QString, SymbolFiles> symbolMap;

    for (const QString& f: files)
    {
        QRegularExpressionMatch m = mbp10Re.match(f);
        if (m.hasMatch())
        {
            QString sym = m.captured(1);
            symbolMap[sym].symbol = sym;
            symbolMap[sym].hasMbp10 = true;
            symbolMap[sym].mbp10Size = QFileInfo(dir.absoluteFilePath(f)).size();
            continue;
        }

        m = tradesRe.match(f);
        if (m.hasMatch())
        {
            QString sym = m.captured(1);
            symbolMap[sym].symbol = sym;
            symbolMap[sym].hasTrades = true;
            symbolMap[sym].tradesSize = QFileInfo(dir.absoluteFilePath(f)).size();
        }
    }

    for (auto it = symbolMap.constBegin(); it != symbolMap.constEnd(); ++it)
    {
        const SymbolFiles& sf = it.value();
        int row = m_symbolsTable->rowCount();
        m_symbolsTable->insertRow(row);

        m_symbolsTable->setItem(row, 0, new QTableWidgetItem(sf.symbol));
        m_symbolsTable->setItem(row, 1, new QTableWidgetItem(sf.hasMbp10 ? "✓" : "—"));
        m_symbolsTable->setItem(row, 2, new QTableWidgetItem(sf.hasTrades ? "✓" : "—"));
    }

    m_symbolsTable->resizeColumnsToContents();
}

void RecordsInfoTab::onStockSelected()
{
    QList<QTableWidgetItem*> selected = m_symbolsTable->selectedItems();
    if (selected.isEmpty())
        return;

    int row = selected.first()->row();
    QTableWidgetItem* symItem = m_symbolsTable->item(row, 0);
    if (symItem == nullptr)
        return;

    QString symbol = symItem->text();
    if (symbol == m_selectedSymbol)
        return;

    m_selectedSymbol = symbol;
    loadSymbolDetails(m_selectedDate, symbol);
}

void RecordsInfoTab::loadSymbolDetails(const QDate& p_date, const QString& p_symbol)
{
    m_symbolLabel->setText(p_symbol);

    // Mbp10
    QString mbp10Path = DBClient::getReplayFilePath(p_date, p_symbol, "mbp10");
    QFileInfo mbp10Info(mbp10Path);
    if (mbp10Info.exists())
    {
        m_mbp10StatusLabel->setText("Status: <span style='color:green;'>Available</span>");
        m_mbp10SizeLabel->setText("Size: " + formatFileSize(mbp10Info.size()));
        m_mbp10PathLabel->setText("Path: " + mbp10Path);
    }
    else
    {
        m_mbp10StatusLabel->setText("Status: <span style='color:red;'>Not found</span>");
        m_mbp10SizeLabel->setText("Size: —");
        m_mbp10PathLabel->setText("Path: " + mbp10Path);
    }

    // Trades
    QString tradesPath = DBClient::getReplayFilePath(p_date, p_symbol, "trades");
    QFileInfo tradesInfo(tradesPath);
    if (tradesInfo.exists())
    {
        m_tradesStatusLabel->setText("Status: <span style='color:green;'>Available</span>");
        m_tradesSizeLabel->setText("Size: " + formatFileSize(tradesInfo.size()));
        m_tradesPathLabel->setText("Path: " + tradesPath);
    }
    else
    {
        m_tradesStatusLabel->setText("Status: <span style='color:red;'>Not found</span>");
        m_tradesSizeLabel->setText("Size: —");
        m_tradesPathLabel->setText("Path: " + tradesPath);
    }
}

void RecordsInfoTab::clearSymbolsList()
{
    m_symbolsTable->setRowCount(0);
    m_selectedSymbol.clear();
    clearDetailsDisplay();
}

void RecordsInfoTab::clearDetailsDisplay()
{
    m_symbolLabel->setText("No symbol selected");
    m_mbp10StatusLabel->setText("Status: —");
    m_mbp10SizeLabel->setText("Size: —");
    m_mbp10PathLabel->setText("Path: —");
    m_tradesStatusLabel->setText("Status: —");
    m_tradesSizeLabel->setText("Size: —");
    m_tradesPathLabel->setText("Path: —");
}

QString RecordsInfoTab::formatFileSize(qint64 p_bytes) const
{
    if (p_bytes < 1024)
        return QString::number(p_bytes) + " B";
    if (p_bytes < 1024 * 1024)
        return QString::number(p_bytes / 1024.0, 'f', 1) + " KB";
    if (p_bytes < 1024 * 1024 * 1024)
        return QString::number(p_bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(p_bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}
