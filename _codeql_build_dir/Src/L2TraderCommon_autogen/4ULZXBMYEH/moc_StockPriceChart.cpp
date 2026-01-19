/****************************************************************************
** Meta object code from reading C++ file 'StockPriceChart.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.h"
#include <QtGui/qtextcursor.h>
#include <QScreen>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'StockPriceChart.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.4.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
namespace {
struct qt_meta_stringdata_StockPriceChart_t {
    uint offsetsAndSizes[40];
    char stringdata0[16];
    char stringdata1[19];
    char stringdata2[1];
    char stringdata3[21];
    char stringdata4[13];
    char stringdata5[11];
    char stringdata6[7];
    char stringdata7[4];
    char stringdata8[4];
    char stringdata9[31];
    char stringdata10[28];
    char stringdata11[8];
    char stringdata12[19];
    char stringdata13[31];
    char stringdata14[8];
    char stringdata15[27];
    char stringdata16[8];
    char stringdata17[19];
    char stringdata18[5];
    char stringdata19[31];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_StockPriceChart_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_StockPriceChart_t qt_meta_stringdata_StockPriceChart = {
    {
        QT_MOC_LITERAL(0, 15),  // "StockPriceChart"
        QT_MOC_LITERAL(16, 18),  // "requestMissingBars"
        QT_MOC_LITERAL(35, 0),  // ""
        QT_MOC_LITERAL(36, 20),  // "viewStartTimeRounded"
        QT_MOC_LITERAL(57, 12),  // "firstBarTime"
        QT_MOC_LITERAL(70, 10),  // "addLiveBar"
        QT_MOC_LITERAL(81, 6),  // "symbol"
        QT_MOC_LITERAL(88, 3),  // "Bar"
        QT_MOC_LITERAL(92, 3),  // "bar"
        QT_MOC_LITERAL(96, 30),  // "onRequestedMissingBarsReceived"
        QT_MOC_LITERAL(127, 27),  // "std::shared_ptr<QList<Bar>>"
        QT_MOC_LITERAL(155, 7),  // "barsPtr"
        QT_MOC_LITERAL(163, 18),  // "onAxisRangeChanged"
        QT_MOC_LITERAL(182, 30),  // "onVolumeChartVisibilityChanged"
        QT_MOC_LITERAL(213, 7),  // "visible"
        QT_MOC_LITERAL(221, 26),  // "onVolumeAutoRescaleChanged"
        QT_MOC_LITERAL(248, 7),  // "enabled"
        QT_MOC_LITERAL(256, 18),  // "onReplayDayChanged"
        QT_MOC_LITERAL(275, 4),  // "date"
        QT_MOC_LITERAL(280, 30)   // "onReplayTimeRangeQueryFinished"
    },
    "StockPriceChart",
    "requestMissingBars",
    "",
    "viewStartTimeRounded",
    "firstBarTime",
    "addLiveBar",
    "symbol",
    "Bar",
    "bar",
    "onRequestedMissingBarsReceived",
    "std::shared_ptr<QList<Bar>>",
    "barsPtr",
    "onAxisRangeChanged",
    "onVolumeChartVisibilityChanged",
    "visible",
    "onVolumeAutoRescaleChanged",
    "enabled",
    "onReplayDayChanged",
    "date",
    "onReplayTimeRangeQueryFinished"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_StockPriceChart[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,   62,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       5,    2,   67,    2, 0x0a,    4 /* Public */,
       9,    1,   72,    2, 0x0a,    7 /* Public */,
      12,    0,   75,    2, 0x08,    9 /* Private */,
      13,    1,   76,    2, 0x08,   10 /* Private */,
      15,    1,   79,    2, 0x08,   12 /* Private */,
      17,    1,   82,    2, 0x08,   14 /* Private */,
      19,    0,   85,    2, 0x08,   16 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QDateTime, QMetaType::QDateTime,    3,    4,

 // slots: parameters
    QMetaType::Void, QMetaType::QString, 0x80000000 | 7,    6,    8,
    QMetaType::Void, 0x80000000 | 10,   11,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   14,
    QMetaType::Void, QMetaType::Bool,   16,
    QMetaType::Void, QMetaType::QDate,   18,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject StockPriceChart::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_StockPriceChart.offsetsAndSizes,
    qt_meta_data_StockPriceChart,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_StockPriceChart_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<StockPriceChart, std::true_type>,
        // method 'requestMissingBars'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QDateTime, std::false_type>,
        QtPrivate::TypeAndForceComplete<QDateTime, std::false_type>,
        // method 'addLiveBar'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const Bar &, std::false_type>,
        // method 'onRequestedMissingBarsReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const std::shared_ptr<QVector<Bar>>, std::false_type>,
        // method 'onAxisRangeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onVolumeChartVisibilityChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onVolumeAutoRescaleChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onReplayDayChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QDate &, std::false_type>,
        // method 'onReplayTimeRangeQueryFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void StockPriceChart::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<StockPriceChart *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->requestMissingBars((*reinterpret_cast< std::add_pointer_t<QDateTime>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QDateTime>>(_a[2]))); break;
        case 1: _t->addLiveBar((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<Bar>>(_a[2]))); break;
        case 2: _t->onRequestedMissingBarsReceived((*reinterpret_cast< std::add_pointer_t<std::shared_ptr<QList<Bar>>>>(_a[1]))); break;
        case 3: _t->onAxisRangeChanged(); break;
        case 4: _t->onVolumeChartVisibilityChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->onVolumeAutoRescaleChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->onReplayDayChanged((*reinterpret_cast< std::add_pointer_t<QDate>>(_a[1]))); break;
        case 7: _t->onReplayTimeRangeQueryFinished(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Bar >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (StockPriceChart::*)(QDateTime , QDateTime );
            if (_t _q_method = &StockPriceChart::requestMissingBars; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *StockPriceChart::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *StockPriceChart::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_StockPriceChart.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int StockPriceChart::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void StockPriceChart::requestMissingBars(QDateTime _t1, QDateTime _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
