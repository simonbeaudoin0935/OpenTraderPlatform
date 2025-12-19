/***************************************************************************
**                                                                        **
**  QCustomPlot - Stub Header                                            **
**                                                                        **
**  This is a minimal stub for integration. Replace with actual          **
**  QCustomPlot library from https://www.qcustomplot.com/                **
**                                                                        **
***************************************************************************/

#ifndef QCUSTOMPLOT_H
#define QCUSTOMPLOT_H

#include <QWidget>
#include <QVector>
#include <QSharedPointer>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QMargins>
#include <QSize>
#include <QDateTime>
#include <QPixmap>
#include <QPainter>
#include <QPointer>
#include <QMap>

// Forward declarations
class QCustomPlot;
class QCPAxis;
class QCPAxisRect;
class QCPAbstractPlottable;
class QCPFinancial;
class QCPBars;
class QCPLegend;
class QCPLayoutGrid;
class QCPMarginGroup;
class QCPAxisTicker;
class QCPAxisTickerDateTime;

// Enums
class QCP {
public:
    enum MarginSide {
        msLeft = 0x01,
        msRight = 0x02,
        msTop = 0x04,
        msBottom = 0x08,
        msAll = 0xFF,
        msNone = 0x00
    };
    Q_DECLARE_FLAGS(MarginSides, MarginSide)
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCP::MarginSides)

// Range class
class QCPRange {
public:
    double lower, upper;
    QCPRange() : lower(0), upper(0) {}
    QCPRange(double lower, double upper) : lower(lower), upper(upper) {}
    double size() const { return upper - lower; }
    double center() const { return (lower + upper) / 2.0; }
};

// Layer class
class QCPLayer : public QObject {
    Q_OBJECT
public:
    enum LayerInsertMode { limBelow, limAbove };
    explicit QCPLayer(QCustomPlot *parentPlot, const QString &layerName);
};

// Layout element base class
class QCPLayoutElement {
public:
    virtual ~QCPLayoutElement() {}
    virtual void setAutoMargins(QCP::MarginSides sides) { m_autoMarginSides = sides; }
    virtual void setMargins(const QMargins &margins) { m_margins = margins; }
    virtual void setMaximumSize(const QSize &size) { m_maximumSize = size; }
    virtual void setMarginGroup(QCP::MarginSides sides, QCPMarginGroup *group) { Q_UNUSED(sides); Q_UNUSED(group); }
protected:
    QCP::MarginSides m_autoMarginSides;
    QMargins m_margins;
    QSize m_maximumSize;
};

// Axis class
class QCPAxis : public QObject {
    Q_OBJECT
public:
    enum AxisType { atLeft, atRight, atTop, atBottom };
    
    explicit QCPAxis(QCPAxisRect *parent, AxisType type);
    
    void setLayer(const QString &layer) { m_layer = layer; }
    QString layer() const { return m_layer; }
    QCPLayer* grid() { return m_grid; }
    void setTicker(QSharedPointer<QCPAxisTicker> ticker) { m_ticker = ticker; }
    void setTickLabelRotation(double degrees) { m_tickLabelRotation = degrees; }
    void setBasePen(const QPen &pen) { m_basePen = pen; }
    void setTickLabels(bool show) { m_tickLabels = show; }
    void setTicks(bool show) { m_ticks = show; }
    
    void setRange(double lower, double upper);
    void setRange(const QCPRange &range);
    QCPRange range() const { return m_range; }
    
    void rescale(bool onlyVisiblePlottables = false);
    void scaleRange(double factor, double center);
    
signals:
    void rangeChanged(const QCPRange &newRange);
    void rangeChanged(const QCPRange &newRange, const QCPRange &oldRange);
    
protected:
    QString m_layer;
    QCPLayer *m_grid;
    QSharedPointer<QCPAxisTicker> m_ticker;
    double m_tickLabelRotation;
    QPen m_basePen;
    bool m_tickLabels;
    bool m_ticks;
    QCPRange m_range;
};

// Axis ticker base class
class QCPAxisTicker {
public:
    virtual ~QCPAxisTicker() {}
};

// DateTime ticker
class QCPAxisTickerDateTime : public QCPAxisTicker {
public:
    QCPAxisTickerDateTime() : m_dateTimeSpec(Qt::LocalTime) {}
    void setDateTimeSpec(Qt::TimeSpec spec) { m_dateTimeSpec = spec; }
    void setDateTimeFormat(const QString &format) { m_dateTimeFormat = format; }
protected:
    Qt::TimeSpec m_dateTimeSpec;
    QString m_dateTimeFormat;
};

// Axis rect class
class QCPAxisRect : public QCPLayoutElement {
public:
    explicit QCPAxisRect(QCustomPlot *parentPlot, bool setupDefaultAxes = true);
    
    QCPAxis* axis(QCPAxis::AxisType type) const;
    
protected:
    QMap<QCPAxis::AxisType, QCPAxis*> m_axes;
    QCustomPlot *m_parentPlot;
};

// Data container for financial data
class QCPFinancialDataContainer {
public:
    struct FinancialData {
        double key;
        double open;
        double high;
        double low;
        double close;
    };
    
    void set(const QVector<FinancialData> &data) { m_data = data; }
    void clear() { m_data.clear(); }
    int size() const { return m_data.size(); }
    
protected:
    QVector<FinancialData> m_data;
};

// Financial plottable
class QCPFinancial : public QObject {
    Q_OBJECT
public:
    enum ChartStyle { csOhlc, csCandlestick };
    
    QCPFinancial(QCPAxis *keyAxis, QCPAxis *valueAxis);
    
    void setName(const QString &name) { m_name = name; }
    void setChartStyle(ChartStyle style) { m_chartStyle = style; }
    QSharedPointer<QCPFinancialDataContainer> data() { return m_data; }
    void setWidth(double width) { m_width = width; }
    void setTwoColored(bool twoColored) { m_twoColored = twoColored; }
    void setBrushPositive(const QBrush &brush) { m_brushPositive = brush; }
    void setBrushNegative(const QBrush &brush) { m_brushNegative = brush; }
    void setPenPositive(const QPen &pen) { m_penPositive = pen; }
    void setPenNegative(const QPen &pen) { m_penNegative = pen; }
    
    static QVector<QCPFinancialDataContainer::FinancialData> timeSeriesToOhlc(
        const QVector<double> &time, 
        const QVector<double> &value, 
        double binSize, 
        double startTime);
    
protected:
    QString m_name;
    ChartStyle m_chartStyle;
    QSharedPointer<QCPFinancialDataContainer> m_data;
    double m_width;
    bool m_twoColored;
    QBrush m_brushPositive;
    QBrush m_brushNegative;
    QPen m_penPositive;
    QPen m_penNegative;
    QCPAxis *m_keyAxis;
    QCPAxis *m_valueAxis;
};

// Bars plottable
class QCPBars : public QObject {
    Q_OBJECT
public:
    QCPBars(QCPAxis *keyAxis, QCPAxis *valueAxis);
    
    void addData(double key, double value) { m_data.append(qMakePair(key, value)); }
    void setWidth(double width) { m_width = width; }
    void setPen(const QPen &pen) { m_pen = pen; }
    void setBrush(const QBrush &brush) { m_brush = brush; }
    void clearData() { m_data.clear(); }
    
protected:
    QVector<QPair<double, double>> m_data;
    double m_width;
    QPen m_pen;
    QBrush m_brush;
};

// Legend class
class QCPLegend : public QObject {
    Q_OBJECT
public:
    void setVisible(bool visible) { m_visible = visible; }
    bool visible() const { return m_visible; }
    
protected:
    bool m_visible;
};

// Layout grid
class QCPLayoutGrid : public QObject {
    Q_OBJECT
public:
    void addElement(int row, int column, QCPLayoutElement *element);
    void setRowSpacing(int spacing) { m_rowSpacing = spacing; }
    
protected:
    int m_rowSpacing;
    QMap<QPair<int, int>, QCPLayoutElement*> m_elements;
};

// Margin group
class QCPMarginGroup : public QObject {
    Q_OBJECT
public:
    explicit QCPMarginGroup(QCustomPlot *parentPlot);
};

// Abstract plottable base class
class QCPAbstractPlottable : public QObject {
    Q_OBJECT
public:
    explicit QCPAbstractPlottable(QCPAxis *keyAxis, QCPAxis *valueAxis) 
        : m_keyAxis(keyAxis), m_valueAxis(valueAxis) {}
    virtual ~QCPAbstractPlottable() {}
protected:
    QCPAxis *m_keyAxis;
    QCPAxis *m_valueAxis;
};

// Main QCustomPlot widget
class QCustomPlot : public QWidget {
    Q_OBJECT
public:
    explicit QCustomPlot(QWidget *parent = nullptr);
    virtual ~QCustomPlot();
    
    // Axis access
    QCPAxis *xAxis, *yAxis, *xAxis2, *yAxis2;
    
    // Legend
    QCPLegend *legend;
    
    // Layout
    QCPLayoutGrid *plotLayout() { return m_plotLayout; }
    
    // Axis rect
    QCPAxisRect *axisRect(int index = 0) { return m_axisRects.value(index, nullptr); }
    
    // Plottable management
    void setAutoAddPlottableToLegend(bool enabled) { m_autoAddPlottableToLegend = enabled; }
    
    // Update and replot
    void replot();
    void rescaleAxes(bool onlyVisiblePlottables = false);
    
    // Layer management
    QCPLayer* layer(const QString &name) const;
    bool addLayer(const QString &name, QCPLayer *otherLayer = nullptr, QCPLayer::LayerInsertMode insertMode = QCPLayer::limAbove);
    
    // Coordinate mapping
    QPointF coordsToPixels(double key, double value) const;
    void pixelsToCoords(QPoint pixelPos, double &key, double &value) const;
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    
    QCPLayoutGrid *m_plotLayout;
    QVector<QCPAxisRect*> m_axisRects;
    bool m_autoAddPlottableToLegend;
    QVector<QCPAbstractPlottable*> m_plottables;
};

// Missing constants
#ifndef QWIDGETSIZE_MAX
#define QWIDGETSIZE_MAX ((1<<24)-1)
#endif

#endif // QCUSTOMPLOT_H
