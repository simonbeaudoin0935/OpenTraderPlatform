/****************************************************************************
** Meta object code from reading C++ file 'StreamPositions.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Src/Clients/TSClient/Brokerage/StreamPositions/StreamPositions.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'StreamPositions.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_StreamPositions_t {
    uint offsetsAndSizes[16];
    char stringdata0[16];
    char stringdata1[20];
    char stringdata2[1];
    char stringdata3[9];
    char stringdata4[9];
    char stringdata5[20];
    char stringdata6[16];
    char stringdata7[11];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_StreamPositions_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_StreamPositions_t qt_meta_stringdata_StreamPositions = {
    {
        QT_MOC_LITERAL(0, 15),  // "StreamPositions"
        QT_MOC_LITERAL(16, 19),  // "newPositionReceived"
        QT_MOC_LITERAL(36, 0),  // ""
        QT_MOC_LITERAL(37, 8),  // "Position"
        QT_MOC_LITERAL(46, 8),  // "position"
        QT_MOC_LITERAL(55, 19),  // "endSnapshotReceived"
        QT_MOC_LITERAL(75, 15),  // "positionDeleted"
        QT_MOC_LITERAL(91, 10)   // "positionID"
    },
    "StreamPositions",
    "newPositionReceived",
    "",
    "Position",
    "position",
    "endSnapshotReceived",
    "positionDeleted",
    "positionID"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_StreamPositions[] = {

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
       5,    0,   35,    2, 0x06,    3 /* Public */,
       6,    1,   36,    2, 0x06,    4 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    7,

       0        // eod
};

Q_CONSTINIT const QMetaObject StreamPositions::staticMetaObject = { {
    QMetaObject::SuperData::link<Stream::staticMetaObject>(),
    qt_meta_stringdata_StreamPositions.offsetsAndSizes,
    qt_meta_data_StreamPositions,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_StreamPositions_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<StreamPositions, std::true_type>,
        // method 'newPositionReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Position, std::false_type>,
        // method 'endSnapshotReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'positionDeleted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>
    >,
    nullptr
} };

void StreamPositions::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<StreamPositions *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->newPositionReceived((*reinterpret_cast< std::add_pointer_t<Position>>(_a[1]))); break;
        case 1: _t->endSnapshotReceived(); break;
        case 2: _t->positionDeleted((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Position >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (StreamPositions::*)(Position );
            if (_t _q_method = &StreamPositions::newPositionReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (StreamPositions::*)();
            if (_t _q_method = &StreamPositions::endSnapshotReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (StreamPositions::*)(QString );
            if (_t _q_method = &StreamPositions::positionDeleted; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
    }
}

const QMetaObject *StreamPositions::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *StreamPositions::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_StreamPositions.stringdata0))
        return static_cast<void*>(this);
    return Stream::qt_metacast(_clname);
}

int StreamPositions::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Stream::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void StreamPositions::newPositionReceived(Position _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void StreamPositions::endSnapshotReceived()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void StreamPositions::positionDeleted(QString _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
