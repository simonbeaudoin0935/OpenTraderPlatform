/****************************************************************************
** Meta object code from reading C++ file 'TestRunUpDetector.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Tests/RunUpDetectorUnit/TestRunUpDetector.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'TestRunUpDetector.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_TestRunUpDetector_t {
    uint offsetsAndSizes[14];
    char stringdata0[18];
    char stringdata1[18];
    char stringdata2[1];
    char stringdata3[13];
    char stringdata4[5];
    char stringdata5[8];
    char stringdata6[24];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_TestRunUpDetector_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_TestRunUpDetector_t qt_meta_stringdata_TestRunUpDetector = {
    {
        QT_MOC_LITERAL(0, 17),  // "TestRunUpDetector"
        QT_MOC_LITERAL(18, 17),  // "initTestCase_data"
        QT_MOC_LITERAL(36, 0),  // ""
        QT_MOC_LITERAL(37, 12),  // "initTestCase"
        QT_MOC_LITERAL(50, 4),  // "init"
        QT_MOC_LITERAL(55, 7),  // "cleanup"
        QT_MOC_LITERAL(63, 23)   // "testPriorDayAfterMarket"
    },
    "TestRunUpDetector",
    "initTestCase_data",
    "",
    "initTestCase",
    "init",
    "cleanup",
    "testPriorDayAfterMarket"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_TestRunUpDetector[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   44,    2, 0x08,    1 /* Private */,
       3,    0,   45,    2, 0x08,    2 /* Private */,
       4,    0,   46,    2, 0x08,    3 /* Private */,
       5,    0,   47,    2, 0x08,    4 /* Private */,
       6,    0,   48,    2, 0x08,    5 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject TestRunUpDetector::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_TestRunUpDetector.offsetsAndSizes,
    qt_meta_data_TestRunUpDetector,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_TestRunUpDetector_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<TestRunUpDetector, std::true_type>,
        // method 'initTestCase_data'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'initTestCase'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'init'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cleanup'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'testPriorDayAfterMarket'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void TestRunUpDetector::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<TestRunUpDetector *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->initTestCase_data(); break;
        case 1: _t->initTestCase(); break;
        case 2: _t->init(); break;
        case 3: _t->cleanup(); break;
        case 4: _t->testPriorDayAfterMarket(); break;
        default: ;
        }
    }
    (void)_a;
}

const QMetaObject *TestRunUpDetector::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TestRunUpDetector::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_TestRunUpDetector.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int TestRunUpDetector::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
QT_WARNING_POP
QT_END_MOC_NAMESPACE
