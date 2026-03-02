#pragma once

#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

class TimeAndSalesWidget;

class ConfigTab : public QWidget
{
    Q_OBJECT

  public:
    explicit ConfigTab(QWidget* parent = nullptr);
    ~ConfigTab() override = default;

    /// Set the T&S widget to update when max entries changes
    void setTimeAndSalesWidget(TimeAndSalesWidget* p_widget);

  private slots:
    void onTimeAndSalesMaxEntriesChanged(int value);

  private:
    void setupUI();
    void loadSettings();
    void saveSetting(const QString& key, const QVariant& value);

    QSpinBox* m_timeAndSalesMaxEntriesSpinBox;
    TimeAndSalesWidget* m_timeAndSalesWidget = nullptr;
};
