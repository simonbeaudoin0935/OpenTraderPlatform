/****************************************************************************
** Meta object code from reading C++ file 'AuthHandler.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Src/Clients/TSClient/Auth/AuthHandler.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AuthHandler.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_AuthHandler_t {
    uint offsetsAndSizes[42];
    char stringdata0[12];
    char stringdata1[13];
    char stringdata2[1];
    char stringdata3[8];
    char stringdata4[10];
    char stringdata5[6];
    char stringdata6[7];
    char stringdata7[20];
    char stringdata8[22];
    char stringdata9[19];
    char stringdata10[5];
    char stringdata11[20];
    char stringdata12[9];
    char stringdata13[17];
    char stringdata14[6];
    char stringdata15[18];
    char stringdata16[29];
    char stringdata17[12];
    char stringdata18[25];
    char stringdata19[29];
    char stringdata20[12];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_AuthHandler_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_AuthHandler_t qt_meta_stringdata_AuthHandler = {
    {
        QT_MOC_LITERAL(0, 11),  // "AuthHandler"
        QT_MOC_LITERAL(12, 12),  // "authFinished"
        QT_MOC_LITERAL(25, 0),  // ""
        QT_MOC_LITERAL(26, 7),  // "success"
        QT_MOC_LITERAL(34, 9),  // "AuthToken"
        QT_MOC_LITERAL(44, 5),  // "token"
        QT_MOC_LITERAL(50, 6),  // "reason"
        QT_MOC_LITERAL(57, 19),  // "handleNewConnection"
        QT_MOC_LITERAL(77, 21),  // "handleSocketReadyRead"
        QT_MOC_LITERAL(99, 18),  // "handleCodeReceived"
        QT_MOC_LITERAL(118, 4),  // "code"
        QT_MOC_LITERAL(123, 19),  // "handleTokenResponse"
        QT_MOC_LITERAL(143, 8),  // "response"
        QT_MOC_LITERAL(152, 16),  // "handleTokenError"
        QT_MOC_LITERAL(169, 5),  // "error"
        QT_MOC_LITERAL(175, 17),  // "handleSocketError"
        QT_MOC_LITERAL(193, 28),  // "QAbstractSocket::SocketError"
        QT_MOC_LITERAL(222, 11),  // "socketError"
        QT_MOC_LITERAL(234, 24),  // "handleSocketStateChanged"
        QT_MOC_LITERAL(259, 28),  // "QAbstractSocket::SocketState"
        QT_MOC_LITERAL(288, 11)   // "socketState"
    },
    "AuthHandler",
    "authFinished",
    "",
    "success",
    "AuthToken",
    "token",
    "reason",
    "handleNewConnection",
    "handleSocketReadyRead",
    "handleCodeReceived",
    "code",
    "handleTokenResponse",
    "response",
    "handleTokenError",
    "error",
    "handleSocketError",
    "QAbstractSocket::SocketError",
    "socketError",
    "handleSocketStateChanged",
    "QAbstractSocket::SocketState",
    "socketState"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_AuthHandler[] = {

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
       1,    3,   62,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       7,    0,   69,    2, 0x08,    5 /* Private */,
       8,    0,   70,    2, 0x08,    6 /* Private */,
       9,    1,   71,    2, 0x08,    7 /* Private */,
      11,    1,   74,    2, 0x08,    9 /* Private */,
      13,    1,   77,    2, 0x08,   11 /* Private */,
      15,    1,   80,    2, 0x08,   13 /* Private */,
      18,    1,   83,    2, 0x08,   15 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::Bool, 0x80000000 | 4, QMetaType::QString,    3,    5,    6,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   10,
    QMetaType::Void, QMetaType::QJsonObject,   12,
    QMetaType::Void, QMetaType::QString,   14,
    QMetaType::Void, 0x80000000 | 16,   17,
    QMetaType::Void, 0x80000000 | 19,   20,

       0        // eod
};

Q_CONSTINIT const QMetaObject AuthHandler::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_AuthHandler.offsetsAndSizes,
    qt_meta_data_AuthHandler,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_AuthHandler_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AuthHandler, std::true_type>,
        // method 'authFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<AuthToken, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'handleNewConnection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'handleSocketReadyRead'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'handleCodeReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleTokenResponse'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QJsonObject &, std::false_type>,
        // method 'handleTokenError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleSocketError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QAbstractSocket::SocketError, std::false_type>,
        // method 'handleSocketStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QAbstractSocket::SocketState, std::false_type>
    >,
    nullptr
} };

void AuthHandler::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AuthHandler *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->authFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<AuthToken>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 1: _t->handleNewConnection(); break;
        case 2: _t->handleSocketReadyRead(); break;
        case 3: _t->handleCodeReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->handleTokenResponse((*reinterpret_cast< std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 5: _t->handleTokenError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->handleSocketError((*reinterpret_cast< std::add_pointer_t<QAbstractSocket::SocketError>>(_a[1]))); break;
        case 7: _t->handleSocketStateChanged((*reinterpret_cast< std::add_pointer_t<QAbstractSocket::SocketState>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAbstractSocket::SocketError >(); break;
            }
            break;
        case 7:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAbstractSocket::SocketState >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AuthHandler::*)(bool , AuthToken , QString );
            if (_t _q_method = &AuthHandler::authFinished; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *AuthHandler::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AuthHandler::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_AuthHandler.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AuthHandler::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
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
void AuthHandler::authFinished(bool _t1, AuthToken _t2, QString _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
