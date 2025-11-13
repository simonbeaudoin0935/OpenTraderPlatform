/****************************************************************************
** Meta object code from reading C++ file 'FMPClient.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../src/Clients/FMPClient/FMPClient.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'FMPClient.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_FMPClient_t {
    uint offsetsAndSizes[20];
    char stringdata0[10];
    char stringdata1[19];
    char stringdata2[1];
    char stringdata3[28];
    char stringdata4[7];
    char stringdata5[20];
    char stringdata6[29];
    char stringdata7[18];
    char stringdata8[23];
    char stringdata9[8];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_FMPClient_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_FMPClient_t qt_meta_stringdata_FMPClient = {
    {
        QT_MOC_LITERAL(0, 9),  // "FMPClient"
        QT_MOC_LITERAL(10, 18),  // "quoteShortReceived"
        QT_MOC_LITERAL(29, 0),  // ""
        QT_MOC_LITERAL(30, 27),  // "FMPClient::QuoteShortResult"
        QT_MOC_LITERAL(58, 6),  // "result"
        QT_MOC_LITERAL(65, 19),  // "sharesFloatReceived"
        QT_MOC_LITERAL(85, 28),  // "FMPClient::SharesFloatResult"
        QT_MOC_LITERAL(114, 17),  // "stockNewsReceived"
        QT_MOC_LITERAL(132, 22),  // "QList<StockNewsResult>"
        QT_MOC_LITERAL(155, 7)   // "results"
    },
    "FMPClient",
    "quoteShortReceived",
    "",
    "FMPClient::QuoteShortResult",
    "result",
    "sharesFloatReceived",
    "FMPClient::SharesFloatResult",
    "stockNewsReceived",
    "QList<StockNewsResult>",
    "results"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_FMPClient[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   32,    2, 0x06,    1 /* Public */,
       5,    1,   35,    2, 0x06,    3 /* Public */,
       7,    1,   38,    2, 0x06,    5 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 6,    4,
    QMetaType::Void, 0x80000000 | 8,    9,

       0        // eod
};

Q_CONSTINIT const QMetaObject FMPClient::staticMetaObject = { {
    QMetaObject::SuperData::link<RESTClient::staticMetaObject>(),
    qt_meta_stringdata_FMPClient.offsetsAndSizes,
    qt_meta_data_FMPClient,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_FMPClient_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<FMPClient, std::true_type>,
        // method 'quoteShortReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<FMPClient::QuoteShortResult, std::false_type>,
        // method 'sharesFloatReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<FMPClient::SharesFloatResult, std::false_type>,
        // method 'stockNewsReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QVector<StockNewsResult>, std::false_type>
    >,
    nullptr
} };

void FMPClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<FMPClient *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->quoteShortReceived((*reinterpret_cast< std::add_pointer_t<FMPClient::QuoteShortResult>>(_a[1]))); break;
        case 1: _t->sharesFloatReceived((*reinterpret_cast< std::add_pointer_t<FMPClient::SharesFloatResult>>(_a[1]))); break;
        case 2: _t->stockNewsReceived((*reinterpret_cast< std::add_pointer_t<QList<StockNewsResult>>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (FMPClient::*)(FMPClient::QuoteShortResult );
            if (_t _q_method = &FMPClient::quoteShortReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (FMPClient::*)(FMPClient::SharesFloatResult );
            if (_t _q_method = &FMPClient::sharesFloatReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (FMPClient::*)(QVector<StockNewsResult> );
            if (_t _q_method = &FMPClient::stockNewsReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
    }
}

const QMetaObject *FMPClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FMPClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_FMPClient.stringdata0))
        return static_cast<void*>(this);
    return RESTClient::qt_metacast(_clname);
}

int FMPClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = RESTClient::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void FMPClient::quoteShortReceived(FMPClient::QuoteShortResult _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void FMPClient::sharesFloatReceived(FMPClient::SharesFloatResult _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void FMPClient::stockNewsReceived(QVector<StockNewsResult> _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
