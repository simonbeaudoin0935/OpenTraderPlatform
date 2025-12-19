/***************************************************************************
**                                                                        **
**  QCustomPlot - Stub Implementation                                    **
**                                                                        **
**  This is a minimal stub for integration. Replace with actual          **
**  QCustomPlot library from https://www.qcustomplot.com/                **
**                                                                        **
***************************************************************************/

#include "qcustomplot.h"
#include <QPainter>
#include <QDebug>

// QCPLayer implementation
QCPLayer::QCPLayer(QCustomPlot *parentPlot, const QString &layerName)
    : QObject(parentPlot)
{
    Q_UNUSED(layerName);
}

// QCPAxis implementation
QCPAxis::QCPAxis(QCPAxisRect *parent, AxisType type)
    : QObject(parent)
    , m_layer("axes")
    , m_grid(nullptr)
    , m_tickLabelRotation(0)
    , m_basePen(Qt::black)
    , m_tickLabels(true)
    , m_ticks(true)
    , m_range(0, 1)
{
    Q_UNUSED(type);
    m_grid = new QCPLayer(nullptr, "grid");
}

void QCPAxis::setRange(double lower, double upper)
{
    QCPRange oldRange = m_range;
    m_range = QCPRange(lower, upper);
    emit rangeChanged(m_range);
    emit rangeChanged(m_range, oldRange);
}

void QCPAxis::setRange(const QCPRange &range)
{
    setRange(range.lower, range.upper);
}

void QCPAxis::rescale(bool onlyVisiblePlottables)
{
    Q_UNUSED(onlyVisiblePlottables);
    // Stub implementation
}

void QCPAxis::scaleRange(double factor, double center)
{
    double rangeDist = m_range.upper - m_range.lower;
    double centerDist = center - m_range.lower;
    double newRangeDist = rangeDist * factor;
    setRange(center - centerDist * factor, center + (rangeDist - centerDist) * factor);
}

// QCPAxisRect implementation
QCPAxisRect::QCPAxisRect(QCustomPlot *parentPlot, bool setupDefaultAxes)
    : m_parentPlot(parentPlot)
{
    if (setupDefaultAxes) {
        m_axes[QCPAxis::atLeft] = new QCPAxis(this, QCPAxis::atLeft);
        m_axes[QCPAxis::atRight] = new QCPAxis(this, QCPAxis::atRight);
        m_axes[QCPAxis::atTop] = new QCPAxis(this, QCPAxis::atTop);
        m_axes[QCPAxis::atBottom] = new QCPAxis(this, QCPAxis::atBottom);
    }
}

QCPAxis* QCPAxisRect::axis(QCPAxis::AxisType type) const
{
    return m_axes.value(type, nullptr);
}

// QCPFinancial implementation
QCPFinancial::QCPFinancial(QCPAxis *keyAxis, QCPAxis *valueAxis)
    : m_chartStyle(csCandlestick)
    , m_data(new QCPFinancialDataContainer)
    , m_width(1.0)
    , m_twoColored(false)
    , m_keyAxis(keyAxis)
    , m_valueAxis(valueAxis)
{
}

QVector<QCPFinancialDataContainer::FinancialData> QCPFinancial::timeSeriesToOhlc(
    const QVector<double> &time, 
    const QVector<double> &value, 
    double binSize, 
    double startTime)
{
    QVector<QCPFinancialDataContainer::FinancialData> result;
    
    if (time.isEmpty() || value.isEmpty())
        return result;
    
    // Simple implementation that bins data
    int currentBinStartIndex = 0;
    double currentBinStartTime = startTime;
    
    for (int i = 0; i < time.size(); ++i) {
        // Check if we've moved to a new bin
        if (time[i] >= currentBinStartTime + binSize) {
            // Process previous bin if it had data
            if (i > currentBinStartIndex) {
                QCPFinancialDataContainer::FinancialData data;
                data.key = currentBinStartTime;
                data.open = value[currentBinStartIndex];
                data.close = value[i - 1];
                data.high = value[currentBinStartIndex];
                data.low = value[currentBinStartIndex];
                
                for (int j = currentBinStartIndex; j < i; ++j) {
                    data.high = qMax(data.high, value[j]);
                    data.low = qMin(data.low, value[j]);
                }
                
                result.append(data);
            }
            
            // Move to next bin
            currentBinStartTime += binSize;
            currentBinStartIndex = i;
        }
    }
    
    // Process final bin
    if (currentBinStartIndex < time.size()) {
        QCPFinancialDataContainer::FinancialData data;
        data.key = currentBinStartTime;
        data.open = value[currentBinStartIndex];
        data.close = value[time.size() - 1];
        data.high = value[currentBinStartIndex];
        data.low = value[currentBinStartIndex];
        
        for (int j = currentBinStartIndex; j < time.size(); ++j) {
            data.high = qMax(data.high, value[j]);
            data.low = qMin(data.low, value[j]);
        }
        
        result.append(data);
    }
    
    return result;
}

// QCPBars implementation
QCPBars::QCPBars(QCPAxis *keyAxis, QCPAxis *valueAxis)
    : m_width(1.0)
    , m_pen(Qt::black)
    , m_brush(Qt::NoBrush)
{
    Q_UNUSED(keyAxis);
    Q_UNUSED(valueAxis);
}

// QCPLayoutGrid implementation
void QCPLayoutGrid::addElement(int row, int column, QCPLayoutElement *element)
{
    m_elements[qMakePair(row, column)] = element;
}

// QCPMarginGroup implementation
QCPMarginGroup::QCPMarginGroup(QCustomPlot *parentPlot)
    : QObject(parentPlot)
{
}

// QCustomPlot implementation
QCustomPlot::QCustomPlot(QWidget *parent)
    : QWidget(parent)
    , xAxis(nullptr)
    , yAxis(nullptr)
    , xAxis2(nullptr)
    , yAxis2(nullptr)
    , legend(new QCPLegend)
    , m_plotLayout(new QCPLayoutGrid)
    , m_autoAddPlottableToLegend(true)
{
    setMinimumSize(300, 300);
    
    // Create default axis rect
    QCPAxisRect *defaultAxisRect = new QCPAxisRect(this, true);
    m_axisRects.append(defaultAxisRect);
    m_plotLayout->addElement(0, 0, defaultAxisRect);
    
    // Set up convenience pointers
    xAxis = defaultAxisRect->axis(QCPAxis::atBottom);
    yAxis = defaultAxisRect->axis(QCPAxis::atLeft);
    xAxis2 = defaultAxisRect->axis(QCPAxis::atTop);
    yAxis2 = defaultAxisRect->axis(QCPAxis::atRight);
}

QCustomPlot::~QCustomPlot()
{
}

void QCustomPlot::replot()
{
    update();
}

void QCustomPlot::rescaleAxes(bool onlyVisiblePlottables)
{
    if (xAxis)
        xAxis->rescale(onlyVisiblePlottables);
    if (yAxis)
        yAxis->rescale(onlyVisiblePlottables);
}

QCPLayer* QCustomPlot::layer(const QString &name) const
{
    Q_UNUSED(name);
    return nullptr;
}

bool QCustomPlot::addLayer(const QString &name, QCPLayer *otherLayer, QCPLayer::LayerInsertMode insertMode)
{
    Q_UNUSED(name);
    Q_UNUSED(otherLayer);
    Q_UNUSED(insertMode);
    return true;
}

QPointF QCustomPlot::coordsToPixels(double key, double value) const
{
    Q_UNUSED(key);
    Q_UNUSED(value);
    return QPointF(0, 0);
}

void QCustomPlot::pixelsToCoords(QPoint pixelPos, double &key, double &value) const
{
    Q_UNUSED(pixelPos);
    key = 0;
    value = 0;
}

void QCustomPlot::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    painter.setPen(Qt::black);
    painter.drawText(rect(), Qt::AlignCenter, 
                     "QCustomPlot Stub\nReplace with actual library from\nwww.qcustomplot.com");
}

void QCustomPlot::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}
