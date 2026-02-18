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

    /// Set the value range for the gauge
    /// @param min Minimum value (left side)
    /// @param max Maximum value (right side)
    void setRange(double min, double max);

    /// Get the current label text
    /// @return Label string displayed above gauge
    QString label() const
    {
        return m_label;
    }

    /// Set the label text displayed above gauge
    /// @param label Label string to display
    void setLabel(const QString& label);

  public slots:
    /// Set the current value and trigger repaint
    /// Value is clamped to [min, max] range
    /// @param value New value to display
    void setValue(double value);

  signals:
    /// Emitted when value changes
    /// @param value The new value
    void valueChanged(double value);

  protected:
    /// Custom paint event for drawing the gauge
    /// @param event Paint event details
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
