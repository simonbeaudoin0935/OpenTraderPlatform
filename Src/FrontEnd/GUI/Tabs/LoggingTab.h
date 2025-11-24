#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QMap>
#include <QSpinBox>

class LoggingTab : public QWidget {
    Q_OBJECT

public:
    explicit LoggingTab(QWidget* parent = nullptr);
    ~LoggingTab() override = default;

signals:
    void loggerVisibilityChanged(bool visible);
    void logDepthChanged(int maxLines);

private slots:
    void onCategoryCheckBoxToggled(bool checked);
    void onLoggerVisibilityToggled(bool checked);
    void onLogDepthValueChanged(int value);
    void onGlobalDebugDisableToggled(bool checked);
    void onGlobalInfoDisableToggled(bool checked);

private:
    void setupUI();
    void populateCategoryCheckboxes();

    QVBoxLayout* categoryCheckBoxLayout;
    QMap<QString, QCheckBox*> categoryCheckBoxes;
    QCheckBox* loggerVisibilityCheckBox;
    QSpinBox* logDepthSpinBox;
    QCheckBox* globalDebugDisableCheckBox;
    QCheckBox* globalInfoDisableCheckBox;
};