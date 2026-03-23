#pragma once

#include <QComboBox>
#include <QPushButton>
#include <QWidget>

class ReviewSessionBar : public QWidget
{
  public:
    explicit ReviewSessionBar(QWidget* parent = nullptr);

    void scanAndPopulateSessions();
    [[nodiscard]] QString getSelectedSessionId() const;
    void setSelectedSessionId(const QString& p_sessionId);
    [[nodiscard]] QComboBox* sessionComboBox() const
    {
        return m_sessionCombo;
    }

  private:
    QComboBox* m_sessionCombo = nullptr;
    QPushButton* m_refreshButton = nullptr;
};
