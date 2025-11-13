#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QTextEdit>
#include <QMap>
#include <QSpinBox>

class LoggingTab : public QWidget {
    Q_OBJECT

public:
    explicit LoggingTab(QWidget* parent = nullptr);
    ~LoggingTab() override = default;

public slots:
    void updateLiveLogDisplay(const QString& message);

private slots:
    void onCategoryCheckBoxToggled(bool checked);
    void onMaxLogLinesChanged(int value);

private:
    void setupUI();
    void populateCategoryCheckboxes();
    void enforceMaxLogLines();

    QVBoxLayout* categoryCheckBoxLayout;
    QTextEdit* liveLogDisplay;
    QSpinBox* maxLogLinesSpinBox;
    QMap<QString, QCheckBox*> categoryCheckBoxes;
    int maxLogLines;
};