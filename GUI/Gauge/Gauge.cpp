#include "Gauge.h"
#include <QPainter>
#include <QRectF>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QFontMetrics>

Gauge::Gauge(QWidget *parent)
    : QWidget(parent)
    , m_value(0.0)
    , m_minValue(-1.0)
    , m_maxValue(1.0)
    , m_label("BAI")
{
    setMinimumSize(100, 80);
}

Gauge::Gauge(const QString &label, QWidget *parent)
    : QWidget(parent)
    , m_value(0.0)
    , m_minValue(-1.0)
    , m_maxValue(1.0)
    , m_label(label)
{
    setMinimumSize(100, 80);
}

void Gauge::setLabel(const QString &label) {
    if (m_label != label) {
        m_label = label;
        update();
    }
}

void Gauge::setValue(double value) {
    m_value = qBound(m_minValue, value, m_maxValue);
    emit valueChanged(m_value);
    update();
}

void Gauge::setRange(double min, double max) {
    if (min < max) {
        m_minValue = min;
        m_maxValue = max;
        m_value = qBound(min, m_value, max);
        update();
    }
}

void Gauge::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    drawBackground(painter);
    drawBar(painter);
    drawTicks(painter);
    drawIndicator(painter);
    drawCenterLogo(painter);
}

void Gauge::drawBackground(QPainter &painter) {
    // Calculate font metrics for the largest possible label
    QFont font = painter.font();
    font.setPointSize(qMax(7, static_cast<int>(height() * 0.06)));
    QFontMetrics fm(font);



    // Create background rect with adjusted padding
    QRectF rect = QRectF(0, 0, width(), height())
                    .adjusted(5,                    // Left padding
                             5,                     // Top padding
                             -5,                    // Right padding
                             -5);                   // Minimal bottom padding to extend background

    // Background with dark gradient
    QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
    gradient.setColorAt(0, Qt::darkGray);
    gradient.setColorAt(1, Qt::black);
    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect, 10, 10);
}

void Gauge::drawBar(QPainter &painter) {
    // Calculate dimensions relative to widget size
    double padding = width() * 0.05; // 5% padding
    double barHeight = height() * 0.25; // 25% of height
    QRectF rect = QRectF(padding, height() / 2 - barHeight / 2, 
                        width() - 2 * padding, barHeight);

    // Draw the bar background with a smoother dark blue gradient
    QLinearGradient barGradient(rect.topLeft(), rect.topRight());
    barGradient.setColorAt(0, QColor(0, 50, 100));
    barGradient.setColorAt(0.5, QColor(0, 35, 75));
    barGradient.setColorAt(1, QColor(0, 20, 50));
    painter.setBrush(barGradient);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect, barHeight * 0.16, barHeight * 0.16); // 16% of height for radius

    // Draw the red section (-1 to 0, sell-side) with smoother intensity gradient
    QRectF redRect = rect;
    redRect.setWidth(rect.width() / 2);
    QLinearGradient redGradient(redRect.topRight(), redRect.topLeft());
    redGradient.setColorAt(0, QColor(255, 100, 100));
    redGradient.setColorAt(0.5, QColor(255, 50, 50));
    redGradient.setColorAt(1, Qt::red);
    painter.setBrush(redGradient);
    painter.drawRoundedRect(redRect, barHeight * 0.16, barHeight * 0.16);

    // Draw the green section (0 to 1, buy-side) with smoother intensity gradient
    QRectF greenRect = rect;
    greenRect.setX(rect.x() + rect.width() / 2);
    greenRect.setWidth(rect.width() / 2);
    QLinearGradient greenGradient(greenRect.topLeft(), greenRect.topRight());
    greenGradient.setColorAt(0, QColor(100, 255, 100));
    greenGradient.setColorAt(0.5, QColor(50, 255, 50));
    greenGradient.setColorAt(1, Qt::green);
    painter.setBrush(greenGradient);
    painter.drawRoundedRect(greenRect, barHeight * 0.16, barHeight * 0.16);
}

void Gauge::drawTicks(QPainter &painter) {
    double padding = width() * 0.05;
    double barHeight = height() * 0.25;
    QRectF rect = QRectF(padding, height() / 2 - barHeight / 2, 
                        width() - 2 * padding, barHeight);
    
    int majorTicks = 5;
    double step = rect.width() / (majorTicks - 1);

    QPen pen(Qt::white);
    pen.setWidth(qMax(1, static_cast<int>(height() * 0.01))); // 1% of height
    painter.setPen(pen);

    // First pass: determine the widest label
    QFont font = painter.font();
    font.setPointSize(qMax(7, static_cast<int>(height() * 0.06))); // 6% of height, minimum 7pt
    QFontMetrics fm(font);
    int maxWidth = 0;
    int maxHeight = 0;
    
    for (int i = 0; i < majorTicks; ++i) {
        double value = m_minValue + (m_maxValue - m_minValue) * i / (majorTicks - 1);
        QString label = QString::number(value, 'f', 1);
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(label));
        maxHeight = qMax(maxHeight, fm.height());
    }

    // Scale font if needed to fit all labels
    if (maxWidth > width() * 0.12) { // Allow up to 12% of gauge width per label
        double scaleFactor = (width() * 0.12) / maxWidth;
        font.setPointSizeF(font.pointSizeF() * scaleFactor);
        painter.setFont(font);
        fm = QFontMetrics(font);
        maxHeight = fm.height();
    }
    painter.setFont(font);

    // Calculate tick and label dimensions
    double tickLength = barHeight * 0.16; // 16% of bar height
    double labelPadding = height() * 0.03; // Increased padding between tick and label
    double totalLabelHeight = maxHeight + labelPadding * 2; // Add padding above and below text

    // Second pass: draw ticks and labels
    for (int i = 0; i < majorTicks; ++i) {
        double x = rect.x() + i * step;
        
        // Draw major tick
        painter.drawLine(QPointF(x, rect.y() - tickLength), QPointF(x, rect.y()));
        painter.drawLine(QPointF(x, rect.y() + rect.height()), QPointF(x, rect.y() + rect.height() + tickLength));

        // Draw label with adjusted rect
        double value = m_minValue + (m_maxValue - m_minValue) * i / (majorTicks - 1);
        QString label = QString::number(value, 'f', 1);
        
        // Create a wider rectangle for the label with proper vertical positioning
        QRectF labelRect(x - width() * 0.06,  // Horizontal position
                        rect.y() + rect.height() + tickLength + labelPadding, // Vertical position
                        width() * 0.12,        // Width
                        totalLabelHeight);     // Height with padding
        
        painter.setPen(Qt::white);
        painter.drawText(labelRect, Qt::AlignCenter, label);
    }
}

void Gauge::drawIndicator(QPainter &painter) {
    double padding = width() * 0.05;
    double barHeight = height() * 0.25;
    QRectF rect = QRectF(padding, height() / 2 - barHeight / 2, 
                        width() - 2 * padding, barHeight);

    // Calculate the position of the indicator
    double valueRange = m_maxValue - m_minValue;
    double normalizedValue = (m_value - m_minValue) / valueRange;
    double x = rect.x() + normalizedValue * rect.width();

    // Calculate color based on direction and intensity
    double intensity = qAbs(m_value);
    QColor baseColor;
    if (m_value < 0) {
        // Smooth transition from red to gray
        int redValue = static_cast<int>(100 + intensity * 155);
        int grayValue = static_cast<int>(150 * (1 - intensity));
        baseColor = QColor(redValue, grayValue, grayValue);
    } else if (m_value > 0) {
        // Smooth transition from green to gray
        int greenValue = static_cast<int>(100 + intensity * 155);
        int grayValue = static_cast<int>(150 * (1 - intensity));
        baseColor = QColor(grayValue, greenValue, grayValue);
    } else {
        baseColor = QColor(150, 150, 150);
    }

    // Calculate the size based on position (grows as it moves away from center)
    double baseSize = barHeight * 0.6; // Base size is 60% of bar height
    double sizeMultiplier = 1.0 + (intensity * 0.8); // Grows up to 80% larger at extremes
    double indicatorSize = baseSize * sizeMultiplier;

    QRectF indicatorRect(x - indicatorSize / 2, rect.y() - indicatorSize / 2, 
                        indicatorSize, indicatorSize);
    
    // Create a more dynamic gradient based on position
    QRadialGradient metalGradient(indicatorRect.center(), indicatorSize / 2);
    if (m_value < 0) {
        metalGradient.setColorAt(0, baseColor.lighter(150 + intensity * 50));
        metalGradient.setColorAt(0.5, baseColor);
        metalGradient.setColorAt(0.8, baseColor.darker(150 + intensity * 50));
        metalGradient.setColorAt(1, baseColor.darker(200 + intensity * 50));
    } else if (m_value > 0) {
        metalGradient.setColorAt(0, baseColor.lighter(150 + intensity * 50));
        metalGradient.setColorAt(0.5, baseColor);
        metalGradient.setColorAt(0.8, baseColor.darker(150 + intensity * 50));
        metalGradient.setColorAt(1, baseColor.darker(200 + intensity * 50));
    } else {
        metalGradient.setColorAt(0, baseColor.lighter(150));
        metalGradient.setColorAt(0.5, baseColor);
        metalGradient.setColorAt(0.8, baseColor.darker(150));
        metalGradient.setColorAt(1, baseColor.darker(200));
    }
    
    painter.setBrush(metalGradient);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(indicatorRect);

    // Add a dynamic highlight that also scales with position
    QRadialGradient highlight(indicatorRect.center(), indicatorSize / 2);
    highlight.setColorAt(0, QColor(255, 255, 255, 50 + intensity * 50));
    highlight.setColorAt(0.5, QColor(255, 255, 255, 0));
    painter.setBrush(highlight);
    painter.drawEllipse(indicatorRect);
}

void Gauge::drawCenterLogo(QPainter &painter) {
    double padding = width() * 0.05;
    double barHeight = height() * 0.25;
    QRectF rect = QRectF(padding, height() / 2 - barHeight / 2, 
                        width() - 2 * padding, barHeight);
    
    // Create a larger rectangle for the label that extends above the bar
    QRectF logoRect(rect.center().x() - width() * 0.2, rect.y() - height() * 0.28,
                   width() * 0.4, height() * 0.15);

    painter.save();
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(qMax(8, static_cast<int>(height() * 0.08))); // Slightly smaller font, minimum 8pt
    font.setBold(true);
    painter.setFont(font);

    // Test if text fits in the current rect
    QFontMetrics fm(font);
    QString text = m_label;
    int textWidth = fm.horizontalAdvance(text);
    
    // If text is too wide, scale down the font
    if (textWidth > logoRect.width()) {
        double scaleFactor = logoRect.width() / textWidth;
        font.setPointSizeF(font.pointSizeF() * scaleFactor);
        painter.setFont(font);
    }
    
    painter.drawText(logoRect, Qt::AlignCenter, text);
    painter.restore();
} 
