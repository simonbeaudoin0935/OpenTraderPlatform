/****************************************************************************
** Meta object code from reading C++ file 'BreakingNewsFetcher.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../src/Algo/BreakingNewsFetcher/BreakingNewsFetcher.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'BreakingNewsFetcher.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_BreakingNewsFetcher_t {
    uint offsetsAndSizes[16];
    char stringdata0[20];
    char stringdata1[13];
    char stringdata2[1];
    char stringdata3[16];
    char stringdata4[8];
    char stringdata5[20];
    char stringdata6[23];
    char stringdata7[8];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_BreakingNewsFetcher_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_BreakingNewsFetcher_t qt_meta_stringdata_BreakingNewsFetcher = {
    {
        QT_MOC_LITERAL(0, 19),  // "BreakingNewsFetcher"
        QT_MOC_LITERAL(20, 12),  // "foundNewNews"
        QT_MOC_LITERAL(33, 0),  // ""
        QT_MOC_LITERAL(34, 15),  // "StockNewsResult"
        QT_MOC_LITERAL(50, 7),  // "newNews"
        QT_MOC_LITERAL(58, 19),  // "onStockNewsReceived"
        QT_MOC_LITERAL(78, 22),  // "QList<StockNewsResult>"
        QT_MOC_LITERAL(101, 7)   // "results"
    },
    "BreakingNewsFetcher",
    "foundNewNews",
    "",
    "StockNewsResult",
    "newNews",
    "onStockNewsReceived",
    "QList<StockNewsResult>",
    "results"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_BreakingNewsFetcher[] = {

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
       1,    1,   26,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       5,    1,   29,    2, 0x08,    3 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 6,    7,

       0        // eod
};

Q_CONSTINIT const QMetaObject BreakingNewsFetcher::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_BreakingNewsFetcher.offsetsAndSizes,
    qt_meta_data_BreakingNewsFetcher,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_BreakingNewsFetcher_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<BreakingNewsFetcher, std::true_type>,
        // method 'foundNewNews'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<StockNewsResult, std::false_type>,
        // method 'onStockNewsReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QVector<StockNewsResult>, std::false_type>
    >,
    nullptr
} };

void BreakingNewsFetcher::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<BreakingNewsFetcher *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->foundNewNews((*reinterpret_cast< std::add_pointer_t<StockNewsResult>>(_a[1]))); break;
        case 1: _t->onStockNewsReceived((*reinterpret_cast< std::add_pointer_t<QList<StockNewsResult>>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (BreakingNewsFetcher::*)(StockNewsResult );
            if (_t _q_method = &BreakingNewsFetcher::foundNewNews; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *BreakingNewsFetcher::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *BreakingNewsFetcher::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_BreakingNewsFetcher.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int BreakingNewsFetcher::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void BreakingNewsFetcher::foundNewNews(StockNewsResult _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
