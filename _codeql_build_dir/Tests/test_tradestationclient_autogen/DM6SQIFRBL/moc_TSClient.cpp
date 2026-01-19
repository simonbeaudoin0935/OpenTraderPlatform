/****************************************************************************
** Meta object code from reading C++ file 'TSClient.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Src/Clients/TSClient/TSClient.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
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
    uint offsetsAndSizes[42];
    char stringdata0[9];
    char stringdata1[32];
    char stringdata2[1];
    char stringdata3[10];
    char stringdata4[9];
    char stringdata5[23];
    char stringdata6[7];
    char stringdata7[6];
    char stringdata8[17];
    char stringdata9[16];
    char stringdata10[7];
    char stringdata11[18];
    char stringdata12[15];
    char stringdata13[8];
    char stringdata14[10];
    char stringdata15[6];
    char stringdata16[23];
    char stringdata17[6];
    char stringdata18[8];
    char stringdata19[10];
    char stringdata20[6];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_TSClient_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_TSClient_t qt_meta_stringdata_TSClient = {
    {
        QT_MOC_LITERAL(0, 8),  // "TSClient"
        QT_MOC_LITERAL(9, 31),  // "totalDataReceivedBytesIncreased"
        QT_MOC_LITERAL(41, 0),  // ""
        QT_MOC_LITERAL(42, 9),  // "qsizetype"
        QT_MOC_LITERAL(52, 8),  // "dataSize"
        QT_MOC_LITERAL(61, 22),  // "openStreamCountChanged"
        QT_MOC_LITERAL(84, 6),  // "size_t"
        QT_MOC_LITERAL(91, 5),  // "count"
        QT_MOC_LITERAL(97, 16),  // "authStateChanged"
        QT_MOC_LITERAL(114, 15),  // "isAuthenticated"
        QT_MOC_LITERAL(130, 6),  // "reason"
        QT_MOC_LITERAL(137, 17),  // "launchAuthProcess"
        QT_MOC_LITERAL(155, 14),  // "onAuthFinished"
        QT_MOC_LITERAL(170, 7),  // "success"
        QT_MOC_LITERAL(178, 9),  // "AuthToken"
        QT_MOC_LITERAL(188, 5),  // "token"
        QT_MOC_LITERAL(194, 22),  // "onAuthHandlerDestroyed"
        QT_MOC_LITERAL(217, 5),  // "Error"
        QT_MOC_LITERAL(223, 7),  // "Timeout"
        QT_MOC_LITERAL(231, 9),  // "JSONError"
        QT_MOC_LITERAL(241, 5)   // "Other"
    },
    "TSClient",
    "totalDataReceivedBytesIncreased",
    "",
    "qsizetype",
    "dataSize",
    "openStreamCountChanged",
    "size_t",
    "count",
    "authStateChanged",
    "isAuthenticated",
    "reason",
    "launchAuthProcess",
    "onAuthFinished",
    "success",
    "AuthToken",
    "token",
    "onAuthHandlerDestroyed",
    "Error",
    "Timeout",
    "JSONError",
    "Other"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_TSClient[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       1,   70, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   50,    2, 0x06,    1 /* Public */,
       5,    1,   53,    2, 0x06,    3 /* Public */,
       8,    2,   56,    2, 0x06,    5 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      11,    0,   61,    2, 0x0a,    8 /* Public */,
      12,    3,   62,    2, 0x08,    9 /* Private */,
      16,    0,   69,    2, 0x08,   13 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,    9,   10,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool, 0x80000000 | 14, QMetaType::QString,   13,   15,   10,
    QMetaType::Void,

 // enums: name, alias, flags, count, data
      17,   17, 0x2,    3,   75,

 // enum data: key, value
      18, uint(TSClient::Error::Timeout),
      19, uint(TSClient::Error::JSONError),
      20, uint(TSClient::Error::Other),

       0        // eod
};

Q_CONSTINIT const QMetaObject TSClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_TSClient.offsetsAndSizes,
    qt_meta_data_TSClient,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_TSClient_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<TSClient, std::true_type>,
        // method 'totalDataReceivedBytesIncreased'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qsizetype, std::false_type>,
        // method 'openStreamCountChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<size_t, std::false_type>,
        // method 'authStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'launchAuthProcess'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onAuthFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<AuthToken, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'onAuthHandlerDestroyed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void TSClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<TSClient *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->totalDataReceivedBytesIncreased((*reinterpret_cast< std::add_pointer_t<qsizetype>>(_a[1]))); break;
        case 1: _t->openStreamCountChanged((*reinterpret_cast< std::add_pointer_t<size_t>>(_a[1]))); break;
        case 2: _t->authStateChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 3: _t->launchAuthProcess(); break;
        case 4: _t->onAuthFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<AuthToken>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 5: _t->onAuthHandlerDestroyed(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (TSClient::*)(qsizetype );
            if (_t _q_method = &TSClient::totalDataReceivedBytesIncreased; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (TSClient::*)(size_t );
            if (_t _q_method = &TSClient::openStreamCountChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (TSClient::*)(bool , QString );
            if (_t _q_method = &TSClient::authStateChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
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
    return QObject::qt_metacast(_clname);
}

int TSClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void TSClient::totalDataReceivedBytesIncreased(qsizetype _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void TSClient::openStreamCountChanged(size_t _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void TSClient::authStateChanged(bool _t1, QString _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
