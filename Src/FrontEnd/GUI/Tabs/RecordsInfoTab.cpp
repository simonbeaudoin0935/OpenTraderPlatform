#include "RecordsInfoTab.h"
#include "DBClient.h"
#include "Logging.h"
#include "Settings.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

RecordsInfoTab::RecordsInfoTab(QWidget* p_parent)
    : QWidget(p_parent)
    , m_dateEdit(nullptr)
    , m_csvPathEdit(nullptr)
    , m_browseCsvButton(nullptr)
    , m_manualSymbolsEdit(nullptr)
    , m_downloadButton(nullptr)
    , m_downloadProgressBar(nullptr)
    , m_downloadStatusLabel(nullptr)
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

QStringList RecordsInfoTab::parseSymbolCsv(const QString& p_filePath)
{
    QStringList symbols;
    QFile file(p_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return symbols;

    QTextStream in(&file);
    bool firstLine = true;

    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        // Skip header row
        if (firstLine)
        {
            firstLine = false;
            continue;
        }

        // Extract first column — handle quoted fields
        QString ticker;
        if (line.startsWith('"'))
        {
            int endQuote = line.indexOf('"', 1);
            if (endQuote > 1)
                ticker = line.mid(1, endQuote - 1);
        }
        else
        {
            int comma = line.indexOf(',');
            ticker = (comma >= 0) ? line.left(comma) : line;
        }

        ticker = ticker.trimmed().toUpper();
        if (!ticker.isEmpty())
            symbols.append(ticker);
    }

    return symbols;
}

QStringList RecordsInfoTab::parseManualSymbols() const
{
    QStringList symbols;
    const QString text = m_manualSymbolsEdit->text().trimmed();
    if (text.isEmpty())
        return symbols;

    const QStringList parts = text.split(',', Qt::SkipEmptyParts);
    for (const QString& part: parts)
    {
        QString sym = part.trimmed().toUpper();
        if (!sym.isEmpty())
            symbols.append(sym);
    }
    return symbols;
}

QStringList RecordsInfoTab::buildDownloadQueue(const QDate& p_date) const
{
    QSet<QString> seen;
    QStringList queue;

    // CSV symbols first
    const QString csvPath = m_csvPathEdit->text().trimmed();
    if (!csvPath.isEmpty())
    {
        const QStringList csvSymbols = parseSymbolCsv(csvPath);
        for (const QString& sym: csvSymbols)
        {
            if (!seen.contains(sym))
            {
                seen.insert(sym);
                if (!DBClient::hasReplayData(p_date, sym))
                    queue.append(sym);
            }
        }
    }

    // Manual symbols
    const QStringList manualSymbols = parseManualSymbols();
    for (const QString& sym: manualSymbols)
    {
        if (!seen.contains(sym))
        {
            seen.insert(sym);
            if (!DBClient::hasReplayData(p_date, sym))
                queue.append(sym);
        }
    }

    return queue;
}

void RecordsInfoTab::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // ── Download section ──────────────────────────────────────────────────────
    auto* downloadGroup = new QGroupBox("Download Replay Data");
    auto* downloadLayout = new QVBoxLayout(downloadGroup);

    // Row 1: Date picker
    auto* dateRow = new QHBoxLayout();
    dateRow->addWidget(new QLabel("Date:"));
    m_dateEdit = new QDateEdit();
    Q_CHECK_PTR(m_dateEdit);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setMaximumDate(QDate::currentDate());
    m_dateEdit->setDisplayFormat("yyyy-MM-dd");

    // Default to last weekday
    QDate defaultDate = QDate::currentDate().addDays(-1);
    while (defaultDate.dayOfWeek() > 5)
        defaultDate = defaultDate.addDays(-1);
    m_dateEdit->setDate(defaultDate);

    dateRow->addWidget(m_dateEdit);
    dateRow->addStretch();
    downloadLayout->addLayout(dateRow);

    // Row 2: CSV file picker
    auto* csvRow = new QHBoxLayout();
    csvRow->addWidget(new QLabel("CSV File:"));
    m_csvPathEdit = new QLineEdit();
    Q_CHECK_PTR(m_csvPathEdit);
    m_csvPathEdit->setPlaceholderText("Path to stock list CSV (e.g., NBI.csv)");
    csvRow->addWidget(m_csvPathEdit, 1);

    m_browseCsvButton = new QPushButton("Browse…");
    Q_CHECK_PTR(m_browseCsvButton);
    csvRow->addWidget(m_browseCsvButton);
    downloadLayout->addLayout(csvRow);

    connect(m_browseCsvButton, &QPushButton::clicked, this, &RecordsInfoTab::onBrowseCsvClicked, Qt::UniqueConnection);
    connect(m_csvPathEdit, &QLineEdit::editingFinished, this, &RecordsInfoTab::saveCsvPath, Qt::UniqueConnection);

    // Row 3: Manual symbols
    auto* manualRow = new QHBoxLayout();
    manualRow->addWidget(new QLabel("Extra Symbols:"));
    m_manualSymbolsEdit = new QLineEdit();
    Q_CHECK_PTR(m_manualSymbolsEdit);
    m_manualSymbolsEdit->setPlaceholderText("Comma-separated (e.g., AAPL, NVDA, MSFT)");
    manualRow->addWidget(m_manualSymbolsEdit, 1);
    downloadLayout->addLayout(manualRow);

    connect(m_manualSymbolsEdit,
            &QLineEdit::editingFinished,
            this,
            &RecordsInfoTab::saveManualSymbols,
            Qt::UniqueConnection);

    // Row 4: Download button + status
    auto* actionRow = new QHBoxLayout();
    m_downloadButton = new QPushButton("Download");
    Q_CHECK_PTR(m_downloadButton);
    actionRow->addWidget(m_downloadButton);

    m_downloadStatusLabel = new QLabel("Idle");
    Q_CHECK_PTR(m_downloadStatusLabel);
    actionRow->addWidget(m_downloadStatusLabel, 1);
    downloadLayout->addLayout(actionRow);

    connect(m_downloadButton, &QPushButton::clicked, this, &RecordsInfoTab::onDownloadClicked, Qt::UniqueConnection);

    // Row 5: Progress bar (hidden until download starts)
    m_downloadProgressBar = new QProgressBar();
    Q_CHECK_PTR(m_downloadProgressBar);
    m_downloadProgressBar->setVisible(false);
    downloadLayout->addWidget(m_downloadProgressBar);

    mainLayout->addWidget(downloadGroup, 0);

    // Restore persisted values from AppState.ini
    if (appStateSettings != nullptr)
    {
        m_csvPathEdit->setText(appStateSettings->value("RecordsInfo/LastCsvPath").toString());
        m_manualSymbolsEdit->setText(appStateSettings->value("RecordsInfo/ManualSymbols").toString());
    }

    // ── Browse section (existing) ─────────────────────────────────────────────

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

    mainLayout->addWidget(splitter, 1);
}

void RecordsInfoTab::saveCsvPath()
{
    if (appStateSettings != nullptr)
        appStateSettings->setValue("RecordsInfo/LastCsvPath", m_csvPathEdit->text().trimmed());
}

void RecordsInfoTab::saveManualSymbols()
{
    if (appStateSettings != nullptr)
        appStateSettings->setValue("RecordsInfo/ManualSymbols", m_manualSymbolsEdit->text().trimmed());
}

void RecordsInfoTab::onBrowseCsvClicked()
{
    QString startDir = m_csvPathEdit->text().trimmed();
    if (startDir.isEmpty())
        startDir = QDir::currentPath();
    else
        startDir = QFileInfo(startDir).absolutePath();

    QString filePath =
        QFileDialog::getOpenFileName(this, "Select Stock List CSV", startDir, "CSV Files (*.csv);;All Files (*)");

    if (!filePath.isEmpty())
    {
        m_csvPathEdit->setText(filePath);
        saveCsvPath();
    }
}

void RecordsInfoTab::onDownloadClicked()
{
    // Validate Databento connection
    if (!DBClient::getInstance()->hasApiKey())
    {
        QMessageBox::warning(this,
                             "Not Connected",
                             "Databento API key is not configured.\n"
                             "Connect via the status bar button first.");
        return;
    }

    const QDate date = m_dateEdit->date();
    if (!date.isValid() || date > QDate::currentDate())
    {
        QMessageBox::warning(this, "Invalid Date", "Please select a valid past date.");
        return;
    }

    // Build the download queue (CSV + manual, deduplicated, skip already downloaded)
    m_downloadQueue = buildDownloadQueue(date);

    if (m_downloadQueue.isEmpty())
    {
        const QString csvPath = m_csvPathEdit->text().trimmed();
        const QStringList manual = parseManualSymbols();
        if (csvPath.isEmpty() && manual.isEmpty())
        {
            QMessageBox::warning(this, "No Symbols", "Please specify a CSV file or enter symbols manually.");
            return;
        }
        m_downloadStatusLabel->setText("All symbols already downloaded for " + date.toString(Qt::ISODate));
        return;
    }

    // Start sequential download
    m_downloadDate = date;
    m_nextDownloadIndex = 0;
    m_completedCount = 0;
    m_downloadSuccessCount = 0;
    m_downloadFailCount = 0;
    m_inFlightSymbols.clear();

    m_downloadButton->setEnabled(false);
    m_downloadProgressBar->setMaximum(m_downloadQueue.size());
    m_downloadProgressBar->setValue(0);
    m_downloadProgressBar->setVisible(true);

    // Connect to DBClient signal
    connect(DBClient::getInstance(),
            &DBClient::replayDownloadFinished,
            this,
            &RecordsInfoTab::onDownloadFinished,
            Qt::UniqueConnection);

    dispatchDownloads();
}

void RecordsInfoTab::dispatchDownloads()
{
    // Launch up to MAX_CONCURRENT_DOWNLOADS in parallel
    while (m_inFlightSymbols.size() < MAX_CONCURRENT_DOWNLOADS && m_nextDownloadIndex < m_downloadQueue.size())
    {
        const QString& symbol = m_downloadQueue.at(m_nextDownloadIndex);
        m_inFlightSymbols.insert(symbol);
        m_nextDownloadIndex++;
        DBClient::getInstance()->downloadReplayData(symbol, m_downloadDate);
    }

    if (!m_inFlightSymbols.isEmpty())
    {
        m_downloadStatusLabel->setText(QString("Downloading %1/%2 (%3 in parallel)…")
                                           .arg(m_completedCount)
                                           .arg(m_downloadQueue.size())
                                           .arg(m_inFlightSymbols.size()));
    }

    if (m_inFlightSymbols.isEmpty() && m_nextDownloadIndex >= m_downloadQueue.size())
        finishDownload();
}

void RecordsInfoTab::finishDownload()
{
    m_downloadButton->setEnabled(true);
    m_downloadStatusLabel->setText(QString("Done — %1 succeeded, %2 failed (of %3 total)")
                                       .arg(m_downloadSuccessCount)
                                       .arg(m_downloadFailCount)
                                       .arg(m_downloadQueue.size()));

    disconnect(DBClient::getInstance(), &DBClient::replayDownloadFinished, this, &RecordsInfoTab::onDownloadFinished);

    // Full refresh of the browser
    scanRecordedDays();
}

void RecordsInfoTab::onDownloadFinished(const QString& p_symbol,
                                        const QDate& p_date,
                                        bool p_success,
                                        const QString& p_errorMessage)
{
    Q_UNUSED(p_errorMessage);
    if (p_date != m_downloadDate)
        return;
    if (!m_inFlightSymbols.remove(p_symbol))
        return;

    if (p_success)
        m_downloadSuccessCount++;
    else
        m_downloadFailCount++;

    m_completedCount++;
    m_downloadProgressBar->setValue(m_completedCount);

    // Live-update browser tables on success
    if (p_success)
        updateDaysTableRow(p_date);

    dispatchDownloads();
}

void RecordsInfoTab::onRefreshClicked()
{
    scanRecordedDays();
}

void RecordsInfoTab::updateDaysTableRow(const QDate& p_date)
{
    QString dirPath = DBClient::getReplayDataDir(p_date);
    QDir dir(dirPath);
    if (!dir.exists())
        return;

    QStringList dbnFiles = dir.entryList({"*.dbn.zst"}, QDir::Files);
    qint64 totalSize = 0;
    for (const QString& f: dbnFiles)
        totalSize += QFileInfo(dir.absoluteFilePath(f)).size();

    // Find existing row or insert new one
    int targetRow = -1;
    for (int row = 0; row < m_daysTable->rowCount(); ++row)
    {
        QTableWidgetItem* item = m_daysTable->item(row, 0);
        if (item != nullptr && item->data(Qt::UserRole).toDate() == p_date)
        {
            targetRow = row;
            break;
        }
    }

    if (targetRow < 0)
    {
        targetRow = m_daysTable->rowCount();
        m_daysTable->insertRow(targetRow);
        auto* dateItem = new QTableWidgetItem(p_date.toString(Qt::ISODate));
        dateItem->setData(Qt::UserRole, p_date);
        m_daysTable->setItem(targetRow, 0, dateItem);
    }

    m_daysTable->setItem(targetRow, 1, new QTableWidgetItem(QString::number(dbnFiles.size())));
    m_daysTable->setItem(targetRow, 2, new QTableWidgetItem(formatFileSize(totalSize)));
    m_daysTable->resizeColumnsToContents();

    // Auto-select the download date and refresh symbols
    m_selectedDate = p_date;
    m_daysTable->blockSignals(true);
    m_daysTable->selectRow(targetRow);
    m_daysTable->blockSignals(false);
    loadSymbolsForDay(p_date);
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
