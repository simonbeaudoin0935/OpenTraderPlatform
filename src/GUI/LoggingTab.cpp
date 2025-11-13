#include "LoggingTab.h"
#include <QGroupBox>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>

#include "Misc/Logging.h"

LoggingTab::LoggingTab(QWidget* parent)
    : QWidget(parent),
      categoryCheckBoxLayout(nullptr),
      liveLogDisplay(nullptr),
      maxLogLinesSpinBox(nullptr),
      maxLogLines(1000)
{
    setupUI();
    populateCategoryCheckboxes();

    // Connect to the log broadcaster
    connect(&LogBroadcaster::instance(), &LogBroadcaster::logMessageReceived,
            this, &LoggingTab::updateLiveLogDisplay, Qt::QueuedConnection);
}

void LoggingTab::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Category management section
    QGroupBox* categoryGroupBox = new QGroupBox("Logging Categories");
    QVBoxLayout* categoryLayout = new QVBoxLayout(categoryGroupBox);

    QScrollArea* categoryScrollArea = new QScrollArea();
    categoryScrollArea->setWidgetResizable(true);

    QWidget* categoryScrollWidget = new QWidget();
    categoryCheckBoxLayout = new QVBoxLayout(categoryScrollWidget);
    categoryCheckBoxLayout->setAlignment(Qt::AlignTop);

    categoryScrollArea->setWidget(categoryScrollWidget);
    categoryLayout->addWidget(categoryScrollArea);

    // Live log display section
    QGroupBox* logDisplayGroupBox = new QGroupBox("Live Log Output");
    QVBoxLayout* logDisplayLayout = new QVBoxLayout(logDisplayGroupBox);

    // Add max log lines setting
    QHBoxLayout* settingsLayout = new QHBoxLayout();
    QLabel* maxLinesLabel = new QLabel("Max Log Lines:");
    maxLogLinesSpinBox = new QSpinBox();
    maxLogLinesSpinBox->setMinimum(100);
    maxLogLinesSpinBox->setMaximum(10000);
    maxLogLinesSpinBox->setValue(maxLogLines);
    maxLogLinesSpinBox->setSingleStep(100);
    maxLogLinesSpinBox->setToolTip("Maximum number of log lines to keep in the display buffer");
    
    connect(maxLogLinesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LoggingTab::onMaxLogLinesChanged);

    settingsLayout->addWidget(maxLinesLabel);
    settingsLayout->addWidget(maxLogLinesSpinBox);
    settingsLayout->addStretch();

    liveLogDisplay = new QTextEdit();
    liveLogDisplay->setReadOnly(true);

    // Set monospace font for log display
    QFont font("Monospace");
    font.setPointSize(9);
    liveLogDisplay->setFont(font);

    logDisplayLayout->addLayout(settingsLayout);
    logDisplayLayout->addWidget(liveLogDisplay);

    // Add both sections to main layout
    mainLayout->addWidget(categoryGroupBox);
    mainLayout->addWidget(logDisplayGroupBox);
}

void LoggingTab::populateCategoryCheckboxes() {
    // Clear existing checkboxes
    QLayoutItem* item;
    while ((item = categoryCheckBoxLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    categoryCheckBoxes.clear();

    // Populate category checkboxes
    QStringList categories = LoggingConfig::instance().getCategories();
    for (const QString& category : categories) {
        QCheckBox* checkBox = new QCheckBox(category);
        checkBox->setChecked(LoggingConfig::instance().isCategoryEnabled(category));
        checkBox->setProperty("category", category);

        connect(checkBox, &QCheckBox::toggled, this, &LoggingTab::onCategoryCheckBoxToggled);

        categoryCheckBoxes[category] = checkBox;
        categoryCheckBoxLayout->addWidget(checkBox);
    }

    // Add stretch to push checkboxes to the top
    categoryCheckBoxLayout->addStretch();
}

void LoggingTab::onCategoryCheckBoxToggled(bool checked) {
    QCheckBox* checkBox = qobject_cast<QCheckBox*>(sender());
    if (checkBox) {
        QString category = checkBox->property("category").toString();
        LoggingConfig::instance().setCategoryEnabled(category, checked);

        QString status = checked ? "enabled" : "disabled";
        updateLiveLogDisplay(QString("Category '%1' %2").arg(category, status));
    }
}

void LoggingTab::onMaxLogLinesChanged(int value) {
    maxLogLines = value;
    enforceMaxLogLines();
}

void LoggingTab::enforceMaxLogLines() {
    if (!liveLogDisplay) {
        return;
    }

    // Get current document
    QTextDocument* doc = liveLogDisplay->document();
    int lineCount = doc->lineCount();

    // If we exceed max lines, remove from the top
    if (lineCount > maxLogLines) {
        QTextCursor cursor(doc);
        cursor.movePosition(QTextCursor::Start);
        
        // Calculate how many lines to remove
        int linesToRemove = lineCount - maxLogLines;
        
        // Select and delete the excess lines
        for (int i = 0; i < linesToRemove; ++i) {
            cursor.select(QTextCursor::LineUnderCursor);
            cursor.removeSelectedText();
            cursor.deleteChar(); // Remove the newline
        }
    }
}

void LoggingTab::updateLiveLogDisplay(const QString& message) {
    if (liveLogDisplay) {
        liveLogDisplay->append(message);
        
        // Enforce max log lines
        enforceMaxLogLines();
        
        // Auto-scroll to bottom
        QTextCursor cursor = liveLogDisplay->textCursor();
        cursor.movePosition(QTextCursor::End);
        liveLogDisplay->setTextCursor(cursor);
    }
}