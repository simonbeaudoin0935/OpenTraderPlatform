#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QSpinBox>

class ConfigTab : public QWidget
{
    Q_OBJECT

  public:
    explicit ConfigTab(QWidget* parent = nullptr);
    ~ConfigTab() override = default;

  private slots:
    /// Handle market depth level setting change
    /// @param value New number of market depth levels to display (1-20)
    void onMarketDepthLevelChanged(int value);

  private:
    void setupUI();
    void loadSettings();
    void saveSetting(const QString& key, const QVariant& value);

    QSpinBox* m_marketDepthLevelSpinBox;
};
