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
    void onMarketDepthLevelChanged(int value);

  private:
    void setupUI();
    void loadSettings();
    void saveSetting(const QString& key, const QVariant& value);

    QSpinBox* m_marketDepthLevelSpinBox;
};
