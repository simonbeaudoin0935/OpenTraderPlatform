/****************************************************************************
** Meta object code from reading C++ file 'TSClient.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../src/Clients/TSClient/TSClient.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'TSClient.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_TSClient_t {
    uint offsetsAndSizes[52];
    char stringdata0[9];
    char stringdata1[17];
    char stringdata2[1];
    char stringdata3[16];
    char stringdata4[7];
    char stringdata5[25];
    char stringdata6[15];
    char stringdata7[8];
    char stringdata8[25];
    char stringdata9[15];
    char stringdata10[31];
    char stringdata11[21];
    char stringdata12[15];
    char stringdata13[24];
    char stringdata14[17];
    char stringdata15[7];
    char stringdata16[25];
    char stringdata17[18];
    char stringdata18[21];
    char stringdata19[7];
    char stringdata20[11];
    char stringdata21[5];
    char stringdata22[28];
    char stringdata23[10];
    char stringdata24[10];
    char stringdata25[9];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_TSClient_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_TSClient_t qt_meta_stringdata_TSClient = {
    {
        QT_MOC_LITERAL(0, 8),  // "TSClient"
        QT_MOC_LITERAL(9, 16),  // "authStateChanged"
        QT_MOC_LITERAL(26, 0),  // ""
        QT_MOC_LITERAL(27, 15),  // "isAuthenticated"
        QT_MOC_LITERAL(43, 6),  // "reason"
        QT_MOC_LITERAL(50, 24),  // "getAccountsAsyncReceived"
        QT_MOC_LITERAL(75, 14),  // "QList<Account>"
        QT_MOC_LITERAL(90, 7),  // "results"
        QT_MOC_LITERAL(98, 24),  // "getBalancesAsyncReceived"
        QT_MOC_LITERAL(123, 14),  // "QList<Balance>"
        QT_MOC_LITERAL(138, 30),  // "getQuoteSnapshotsAsyncReceived"
        QT_MOC_LITERAL(169, 20),  // "QList<QuoteSnapshot>"
        QT_MOC_LITERAL(190, 14),  // "quoteSnapshots"
        QT_MOC_LITERAL(205, 23),  // "placeOrderAsyncReceived"
        QT_MOC_LITERAL(229, 16),  // "PlaceOrderResult"
        QT_MOC_LITERAL(246, 6),  // "result"
        QT_MOC_LITERAL(253, 24),  // "cancelOrderAsyncReceived"
        QT_MOC_LITERAL(278, 17),  // "CancelOrderResult"
        QT_MOC_LITERAL(296, 20),  // "getBarsAsyncReceived"
        QT_MOC_LITERAL(317, 6),  // "symbol"
        QT_MOC_LITERAL(324, 10),  // "QList<Bar>"
        QT_MOC_LITERAL(335, 4),  // "bars"
        QT_MOC_LITERAL(340, 27),  // "onAsyncRefreshTokenFinished"
        QT_MOC_LITERAL(368, 9),  // "completed"
        QT_MOC_LITERAL(378, 9),  // "AuthToken"
        QT_MOC_LITERAL(388, 8)   // "newToken"
    },
    "TSClient",
    "authStateChanged",
    "",
    "isAuthenticated",
    "reason",
    "getAccountsAsyncReceived",
    "QList<Account>",
    "results",
    "getBalancesAsyncReceived",
    "QList<Balance>",
    "getQuoteSnapshotsAsyncReceived",
    "QList<QuoteSnapshot>",
    "quoteSnapshots",
    "placeOrderAsyncReceived",
    "PlaceOrderResult",
    "result",
    "cancelOrderAsyncReceived",
    "CancelOrderResult",
    "getBarsAsyncReceived",
    "symbol",
    "QList<Bar>",
    "bars",
    "onAsyncRefreshTokenFinished",
    "completed",
    "AuthToken",
    "newToken"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_TSClient[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,   62,    2, 0x06,    1 /* Public */,
       5,    1,   67,    2, 0x06,    4 /* Public */,
       8,    1,   70,    2, 0x06,    6 /* Public */,
      10,    1,   73,    2, 0x06,    8 /* Public */,
      13,    1,   76,    2, 0x06,   10 /* Public */,
      16,    1,   79,    2, 0x06,   12 /* Public */,
      18,    2,   82,    2, 0x06,   14 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      22,    2,   87,    2, 0x08,   17 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,    3,    4,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, 0x80000000 | 9,    7,
    QMetaType::Void, 0x80000000 | 11,   12,
    QMetaType::Void, 0x80000000 | 14,   15,
    QMetaType::Void, 0x80000000 | 17,   15,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 20,   19,   21,

 // slots: parameters
    QMetaType::Void, QMetaType::Bool, 0x80000000 | 24,   23,   25,

       0        // eod
};

Q_CONSTINIT const QMetaObject TSClient::staticMetaObject = { {
    QMetaObject::SuperData::link<RESTClient::staticMetaObject>(),
    qt_meta_stringdata_TSClient.offsetsAndSizes,
    qt_meta_data_TSClient,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_TSClient_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<TSClient, std::true_type>,
        // method 'authStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'getAccountsAsyncReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QVector<Account>, std::false_type>,
        // method 'getBalancesAsyncReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QVector<Balance>, std::false_type>,
        // method 'getQuoteSnapshotsAsyncReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QVector<QuoteSnapshot>, std::false_type>,
        // method 'placeOrderAsyncReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<PlaceOrderResult, std::false_type>,
        // method 'cancelOrderAsyncReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<CancelOrderResult, std::false_type>,
        // method 'getBarsAsyncReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<QVector<Bar>, std::false_type>,
        // method 'onAsyncRefreshTokenFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const AuthToken &, std::false_type>
    >,
    nullptr
} };

void TSClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<TSClient *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->authStateChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 1: _t->getAccountsAsyncReceived((*reinterpret_cast< std::add_pointer_t<QList<Account>>>(_a[1]))); break;
        case 2: _t->getBalancesAsyncReceived((*reinterpret_cast< std::add_pointer_t<QList<Balance>>>(_a[1]))); break;
        case 3: _t->getQuoteSnapshotsAsyncReceived((*reinterpret_cast< std::add_pointer_t<QList<QuoteSnapshot>>>(_a[1]))); break;
        case 4: _t->placeOrderAsyncReceived((*reinterpret_cast< std::add_pointer_t<PlaceOrderResult>>(_a[1]))); break;
        case 5: _t->cancelOrderAsyncReceived((*reinterpret_cast< std::add_pointer_t<CancelOrderResult>>(_a[1]))); break;
        case 6: _t->getBarsAsyncReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QList<Bar>>>(_a[2]))); break;
        case 7: _t->onAsyncRefreshTokenFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<AuthToken>>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<Account> >(); break;
            }
            break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< PlaceOrderResult >(); break;
            }
            break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<Bar> >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (TSClient::*)(bool , QString );
            if (_t _q_method = &TSClient::authStateChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (TSClient::*)(QVector<Account> );
            if (_t _q_method = &TSClient::getAccountsAsyncReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (TSClient::*)(QVector<Balance> );
            if (_t _q_method = &TSClient::getBalancesAsyncReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (TSClient::*)(QVector<QuoteSnapshot> );
            if (_t _q_method = &TSClient::getQuoteSnapshotsAsyncReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (TSClient::*)(PlaceOrderResult );
            if (_t _q_method = &TSClient::placeOrderAsyncReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (TSClient::*)(CancelOrderResult );
            if (_t _q_method = &TSClient::cancelOrderAsyncReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (TSClient::*)(QString , QVector<Bar> );
            if (_t _q_method = &TSClient::getBarsAsyncReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
    }
}

const QMetaObject *TSClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TSClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_TSClient.stringdata0))
        return static_cast<void*>(this);
    return RESTClient::qt_metacast(_clname);
}

int TSClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = RESTClient::qt_metacall(_c, _id, _a);
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
void TSClient::authStateChanged(bool _t1, QString _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void TSClient::getAccountsAsyncReceived(QVector<Account> _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void TSClient::getBalancesAsyncReceived(QVector<Balance> _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void TSClient::getQuoteSnapshotsAsyncReceived(QVector<QuoteSnapshot> _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void TSClient::placeOrderAsyncReceived(PlaceOrderResult _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void TSClient::cancelOrderAsyncReceived(CancelOrderResult _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void TSClient::getBarsAsyncReceived(QString _t1, QVector<Bar> _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
