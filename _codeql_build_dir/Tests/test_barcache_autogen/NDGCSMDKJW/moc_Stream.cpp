/****************************************************************************
** Meta object code from reading C++ file 'Stream.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Src/Clients/TSClient/Stream/Stream.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Stream.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_Stream_t {
    uint offsetsAndSizes[20];
    char stringdata0[7];
    char stringdata1[24];
    char stringdata2[1];
    char stringdata3[7];
    char stringdata4[6];
    char stringdata5[19];
    char stringdata6[8];
    char stringdata7[17];
    char stringdata8[16];
    char stringdata9[24];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_Stream_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_Stream_t qt_meta_stringdata_Stream = {
    {
        QT_MOC_LITERAL(0, 6),  // "Stream"
        QT_MOC_LITERAL(7, 23),  // "newAmountOfDataReceived"
        QT_MOC_LITERAL(31, 0),  // ""
        QT_MOC_LITERAL(32, 6),  // "size_t"
        QT_MOC_LITERAL(39, 5),  // "bytes"
        QT_MOC_LITERAL(45, 18),  // "receivedNewRawData"
        QT_MOC_LITERAL(64, 7),  // "rawData"
        QT_MOC_LITERAL(72, 16),  // "onReplyReadyRead"
        QT_MOC_LITERAL(89, 15),  // "onReplyFinished"
        QT_MOC_LITERAL(105, 23)   // "onHeartbeatTimerTimeout"
    },
    "Stream",
    "newAmountOfDataReceived",
    "",
    "size_t",
    "bytes",
    "receivedNewRawData",
    "rawData",
    "onReplyReadyRead",
    "onReplyFinished",
    "onHeartbeatTimerTimeout"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_Stream[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   44,    2, 0x06,    1 /* Public */,
       5,    1,   47,    2, 0x06,    3 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       7,    0,   50,    2, 0x0a,    5 /* Public */,
       8,    0,   51,    2, 0x0a,    6 /* Public */,
       9,    0,   52,    2, 0x08,    7 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::QByteArray,    6,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject Stream::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_Stream.offsetsAndSizes,
    qt_meta_data_Stream,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_Stream_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<Stream, std::true_type>,
        // method 'newAmountOfDataReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<size_t, std::false_type>,
        // method 'receivedNewRawData'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QByteArray &, std::false_type>,
        // method 'onReplyReadyRead'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onReplyFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onHeartbeatTimerTimeout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void Stream::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Stream *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->newAmountOfDataReceived((*reinterpret_cast< std::add_pointer_t<size_t>>(_a[1]))); break;
        case 1: _t->receivedNewRawData((*reinterpret_cast< std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 2: _t->onReplyReadyRead(); break;
        case 3: _t->onReplyFinished(); break;
        case 4: _t->onHeartbeatTimerTimeout(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (Stream::*)(size_t );
            if (_t _q_method = &Stream::newAmountOfDataReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (Stream::*)(const QByteArray & );
            if (_t _q_method = &Stream::receivedNewRawData; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *Stream::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Stream::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_Stream.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Stream::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void Stream::newAmountOfDataReceived(size_t _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void Stream::receivedNewRawData(const QByteArray & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
