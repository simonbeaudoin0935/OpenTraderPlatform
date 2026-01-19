/****************************************************************************
** Meta object code from reading C++ file 'MarketDepthQuoteReceiver.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Src/Algo/MarketDepthQuoteReceiver/MarketDepthQuoteReceiver.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MarketDepthQuoteReceiver.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_MarketDepthQuoteReceiver_t {
    uint offsetsAndSizes[20];
    char stringdata0[25];
    char stringdata1[28];
    char stringdata2[1];
    char stringdata3[7];
    char stringdata4[17];
    char stringdata5[17];
    char stringdata6[16];
    char stringdata7[7];
    char stringdata8[7];
    char stringdata9[30];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_MarketDepthQuoteReceiver_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_MarketDepthQuoteReceiver_t qt_meta_stringdata_MarketDepthQuoteReceiver = {
    {
        QT_MOC_LITERAL(0, 24),  // "MarketDepthQuoteReceiver"
        QT_MOC_LITERAL(25, 27),  // "receivedNewMarketDepthQuote"
        QT_MOC_LITERAL(53, 0),  // ""
        QT_MOC_LITERAL(54, 6),  // "symbol"
        QT_MOC_LITERAL(61, 16),  // "MarketDepthQuote"
        QT_MOC_LITERAL(78, 16),  // "marketDepthQuote"
        QT_MOC_LITERAL(95, 15),  // "bidAskImbalance"
        QT_MOC_LITERAL(111, 6),  // "bidDWP"
        QT_MOC_LITERAL(118, 6),  // "askDWP"
        QT_MOC_LITERAL(125, 29)   // "onReceivedNewMarketDepthQuote"
    },
    "MarketDepthQuoteReceiver",
    "receivedNewMarketDepthQuote",
    "",
    "symbol",
    "MarketDepthQuote",
    "marketDepthQuote",
    "bidAskImbalance",
    "bidDWP",
    "askDWP",
    "onReceivedNewMarketDepthQuote"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_MarketDepthQuoteReceiver[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    5,   26,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       9,    1,   37,    2, 0x08,    7 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString, 0x80000000 | 4, QMetaType::Double, QMetaType::Double, QMetaType::Double,    3,    5,    6,    7,    8,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 4,    5,

       0        // eod
};

Q_CONSTINIT const QMetaObject MarketDepthQuoteReceiver::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_MarketDepthQuoteReceiver.offsetsAndSizes,
    qt_meta_data_MarketDepthQuoteReceiver,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_MarketDepthQuoteReceiver_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MarketDepthQuoteReceiver, std::true_type>,
        // method 'receivedNewMarketDepthQuote'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<MarketDepthQuote, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'onReceivedNewMarketDepthQuote'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<MarketDepthQuote, std::false_type>
    >,
    nullptr
} };

void MarketDepthQuoteReceiver::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MarketDepthQuoteReceiver *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->receivedNewMarketDepthQuote((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<MarketDepthQuote>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[5]))); break;
        case 1: _t->onReceivedNewMarketDepthQuote((*reinterpret_cast< std::add_pointer_t<MarketDepthQuote>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< MarketDepthQuote >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< MarketDepthQuote >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (MarketDepthQuoteReceiver::*)(QString , MarketDepthQuote , double , double , double );
            if (_t _q_method = &MarketDepthQuoteReceiver::receivedNewMarketDepthQuote; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *MarketDepthQuoteReceiver::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MarketDepthQuoteReceiver::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MarketDepthQuoteReceiver.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int MarketDepthQuoteReceiver::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void MarketDepthQuoteReceiver::receivedNewMarketDepthQuote(QString _t1, MarketDepthQuote _t2, double _t3, double _t4, double _t5)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t5))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
