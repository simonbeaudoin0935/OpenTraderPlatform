/****************************************************************************
** Meta object code from reading C++ file 'TestBarCache.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Tests/BarCacheUnit/TestBarCache.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'TestBarCache.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_TestBarCache_t {
    uint offsetsAndSizes[20];
    char stringdata0[13];
    char stringdata1[18];
    char stringdata2[1];
    char stringdata3[13];
    char stringdata4[5];
    char stringdata5[8];
    char stringdata6[12];
    char stringdata7[21];
    char stringdata8[21];
    char stringdata9[17];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_TestBarCache_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_TestBarCache_t qt_meta_stringdata_TestBarCache = {
    {
        QT_MOC_LITERAL(0, 12),  // "TestBarCache"
        QT_MOC_LITERAL(13, 17),  // "initTestCase_data"
        QT_MOC_LITERAL(31, 0),  // ""
        QT_MOC_LITERAL(32, 12),  // "initTestCase"
        QT_MOC_LITERAL(45, 4),  // "init"
        QT_MOC_LITERAL(50, 7),  // "cleanup"
        QT_MOC_LITERAL(58, 11),  // "testGetBars"
        QT_MOC_LITERAL(70, 20),  // "testGetBarsOnlyHoles"
        QT_MOC_LITERAL(91, 20),  // "testGetBarsWithHoles"
        QT_MOC_LITERAL(112, 16)   // "testBarStreaming"
    },
    "TestBarCache",
    "initTestCase_data",
    "",
    "initTestCase",
    "init",
    "cleanup",
    "testGetBars",
    "testGetBarsOnlyHoles",
    "testGetBarsWithHoles",
    "testBarStreaming"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_TestBarCache[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   62,    2, 0x08,    1 /* Private */,
       3,    0,   63,    2, 0x08,    2 /* Private */,
       4,    0,   64,    2, 0x08,    3 /* Private */,
       5,    0,   65,    2, 0x08,    4 /* Private */,
       6,    0,   66,    2, 0x08,    5 /* Private */,
       7,    0,   67,    2, 0x08,    6 /* Private */,
       8,    0,   68,    2, 0x08,    7 /* Private */,
       9,    0,   69,    2, 0x08,    8 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject TestBarCache::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_TestBarCache.offsetsAndSizes,
    qt_meta_data_TestBarCache,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_TestBarCache_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<TestBarCache, std::true_type>,
        // method 'initTestCase_data'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'initTestCase'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'init'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cleanup'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'testGetBars'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'testGetBarsOnlyHoles'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'testGetBarsWithHoles'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'testBarStreaming'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void TestBarCache::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<TestBarCache *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->initTestCase_data(); break;
        case 1: _t->initTestCase(); break;
        case 2: _t->init(); break;
        case 3: _t->cleanup(); break;
        case 4: _t->testGetBars(); break;
        case 5: _t->testGetBarsOnlyHoles(); break;
        case 6: _t->testGetBarsWithHoles(); break;
        case 7: _t->testBarStreaming(); break;
        default: ;
        }
    }
    (void)_a;
}

const QMetaObject *TestBarCache::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TestBarCache::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_TestBarCache.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int TestBarCache::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
