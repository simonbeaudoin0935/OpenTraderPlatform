#pragma once

#include <QWidget>
#include <QString>

class Gauge : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString label READ label WRITE setLabel)
  public:
    explicit Gauge(QWidget* parent = nullptr);
    explicit Gauge(const QString& label, QWidget* parent = nullptr);
    void setRange(double min, double max);
    QString label() const
    {
        return m_label;
    }
    void setLabel(const QString& label);

  public slots:
    void setValue(double value);

  signals:
    void valueChanged(double value);

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    double m_value;
    double m_minValue;
    double m_maxValue;
    QString m_label;

    void drawBackground(QPainter& painter);
    void drawBar(QPainter& painter);
    void drawIndicator(QPainter& painter);
    void drawTicks(QPainter& painter);
    void drawCenterLogo(QPainter& painter);
};
