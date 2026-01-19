/****************************************************************************
** Meta object code from reading C++ file 'ShortcutSettings.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Src/Misc/ShortcutSettings.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ShortcutSettings.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_ShortcutSettings_t {
    uint offsetsAndSizes[28];
    char stringdata0[17];
    char stringdata1[16];
    char stringdata2[1];
    char stringdata3[11];
    char stringdata4[5];
    char stringdata5[13];
    char stringdata6[14];
    char stringdata7[16];
    char stringdata8[16];
    char stringdata9[16];
    char stringdata10[17];
    char stringdata11[23];
    char stringdata12[24];
    char stringdata13[16];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_ShortcutSettings_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_ShortcutSettings_t qt_meta_stringdata_ShortcutSettings = {
    {
        QT_MOC_LITERAL(0, 16),  // "ShortcutSettings"
        QT_MOC_LITERAL(17, 15),  // "shortcutChanged"
        QT_MOC_LITERAL(33, 0),  // ""
        QT_MOC_LITERAL(34, 10),  // "ShortcutId"
        QT_MOC_LITERAL(45, 4),  // "p_id"
        QT_MOC_LITERAL(50, 12),  // "QKeySequence"
        QT_MOC_LITERAL(63, 13),  // "p_newSequence"
        QT_MOC_LITERAL(77, 15),  // "QuitApplication"
        QT_MOC_LITERAL(93, 15),  // "FocusStockInput"
        QT_MOC_LITERAL(109, 15),  // "ExecuteBuyOrder"
        QT_MOC_LITERAL(125, 16),  // "ExecuteSellOrder"
        QT_MOC_LITERAL(142, 22),  // "ExecuteBuyToCoverOrder"
        QT_MOC_LITERAL(165, 23),  // "ExecuteSellToCoverOrder"
        QT_MOC_LITERAL(189, 15)   // "CancelAllOrders"
    },
    "ShortcutSettings",
    "shortcutChanged",
    "",
    "ShortcutId",
    "p_id",
    "QKeySequence",
    "p_newSequence",
    "QuitApplication",
    "FocusStockInput",
    "ExecuteBuyOrder",
    "ExecuteSellOrder",
    "ExecuteBuyToCoverOrder",
    "ExecuteSellToCoverOrder",
    "CancelAllOrders"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_ShortcutSettings[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       0,    0, // properties
       1,   25, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,   20,    2, 0x06,    1 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3, 0x80000000 | 5,    4,    6,

 // enums: name, alias, flags, count, data
       3,    3, 0x0,    7,   30,

 // enum data: key, value
       7, uint(ShortcutSettings::QuitApplication),
       8, uint(ShortcutSettings::FocusStockInput),
       9, uint(ShortcutSettings::ExecuteBuyOrder),
      10, uint(ShortcutSettings::ExecuteSellOrder),
      11, uint(ShortcutSettings::ExecuteBuyToCoverOrder),
      12, uint(ShortcutSettings::ExecuteSellToCoverOrder),
      13, uint(ShortcutSettings::CancelAllOrders),

       0        // eod
};

Q_CONSTINIT const QMetaObject ShortcutSettings::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ShortcutSettings.offsetsAndSizes,
    qt_meta_data_ShortcutSettings,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_ShortcutSettings_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ShortcutSettings, std::true_type>,
        // method 'shortcutChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<ShortcutId, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QKeySequence &, std::false_type>
    >,
    nullptr
} };

void ShortcutSettings::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ShortcutSettings *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->shortcutChanged((*reinterpret_cast< std::add_pointer_t<ShortcutId>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QKeySequence>>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ShortcutSettings::*)(ShortcutId , const QKeySequence & );
            if (_t _q_method = &ShortcutSettings::shortcutChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *ShortcutSettings::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShortcutSettings::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ShortcutSettings.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShortcutSettings::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void ShortcutSettings::shortcutChanged(ShortcutId _t1, const QKeySequence & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
