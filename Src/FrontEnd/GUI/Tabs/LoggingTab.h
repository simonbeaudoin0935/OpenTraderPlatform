#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QMap>
#include <QSpinBox>

class LoggingTab : public QWidget
{
    Q_OBJECT

  public:
    explicit LoggingTab(QWidget* parent = nullptr);
    ~LoggingTab() override = default;

  signals:
    /// Emitted when logger window visibility changes
    /// @param visible True to show logger window, false to hide
    void loggerVisibilityChanged(bool visible);

    /// Emitted when maximum log depth (lines) changes
    /// @param maxLines Maximum number of log lines to keep in memory
    void logDepthChanged(int maxLines);

  private slots:
    /// Handle logging category checkbox toggle
    /// Enables/disables specific logging category
    /// @param checked True if category should be enabled
    void onCategoryCheckBoxToggled(bool checked);

    /// Handle logger window visibility checkbox
    /// @param checked True to show logger window, false to hide
    void onLoggerVisibilityToggled(bool checked);

    /// Handle log depth spinbox value change
    /// @param value New maximum number of log lines
    void onLogDepthValueChanged(int value);

    /// Handle global DEBUG logging disable checkbox
    /// @param checked True to disable all DEBUG messages
    void onGlobalDebugDisableToggled(bool checked);

    /// Handle global INFO logging disable checkbox
    /// @param checked True to disable all INFO messages
    void onGlobalInfoDisableToggled(bool checked);

  private:
    void setupUI();
    void populateCategoryCheckboxes();
    void loadSettings();
    void saveSetting(const QString& key, const QVariant& value);

    QVBoxLayout* categoryCheckBoxLayout;
    QMap<QString, QCheckBox*> categoryCheckBoxes;
    QCheckBox* loggerVisibilityCheckBox;
    QSpinBox* logDepthSpinBox;
    QCheckBox* globalDebugDisableCheckBox;
    QCheckBox* globalInfoDisableCheckBox;
};