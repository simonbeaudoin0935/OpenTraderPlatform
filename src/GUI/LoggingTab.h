#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QTextEdit>
#include <QMap>

class LoggingTab : public QWidget {
    Q_OBJECT

public:
    explicit LoggingTab(QWidget* parent = nullptr);
    ~LoggingTab() override = default;

public slots:
    void updateLiveLogDisplay(const QString& message);

private slots:
    void onCategoryCheckBoxToggled(bool checked);

private:
    void setupUI();
    void populateCategoryCheckboxes();

    QVBoxLayout* categoryCheckBoxLayout;
    QTextEdit* liveLogDisplay;
    QMap<QString, QCheckBox*> categoryCheckBoxes;
};