/****************************************************************************
** Meta object code from reading C++ file 'RecorderTab.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Src/FrontEnd/GUI/Tabs/RecorderTab.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'RecorderTab.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_RecorderTab_t {
    uint offsetsAndSizes[22];
    char stringdata0[12];
    char stringdata1[17];
    char stringdata2[1];
    char stringdata3[16];
    char stringdata4[21];
    char stringdata5[31];
    char stringdata6[18];
    char stringdata7[9];
    char stringdata8[22];
    char stringdata9[21];
    char stringdata10[7];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_RecorderTab_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_RecorderTab_t qt_meta_stringdata_RecorderTab = {
    {
        QT_MOC_LITERAL(0, 11),  // "RecorderTab"
        QT_MOC_LITERAL(12, 16),  // "onStartRecording"
        QT_MOC_LITERAL(29, 0),  // ""
        QT_MOC_LITERAL(30, 15),  // "onStopRecording"
        QT_MOC_LITERAL(46, 20),  // "refreshRecorderStats"
        QT_MOC_LITERAL(67, 30),  // "onTradeStationAuthStateChanged"
        QT_MOC_LITERAL(98, 17),  // "p_isAuthenticated"
        QT_MOC_LITERAL(116, 8),  // "p_reason"
        QT_MOC_LITERAL(125, 21),  // "onBrowseButtonClicked"
        QT_MOC_LITERAL(147, 20),  // "onCsvFilePathChanged"
        QT_MOC_LITERAL(168, 6)   // "p_text"
    },
    "RecorderTab",
    "onStartRecording",
    "",
    "onStopRecording",
    "refreshRecorderStats",
    "onTradeStationAuthStateChanged",
    "p_isAuthenticated",
    "p_reason",
    "onBrowseButtonClicked",
    "onCsvFilePathChanged",
    "p_text"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_RecorderTab[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   50,    2, 0x08,    1 /* Private */,
       3,    0,   51,    2, 0x08,    2 /* Private */,
       4,    0,   52,    2, 0x08,    3 /* Private */,
       5,    2,   53,    2, 0x08,    4 /* Private */,
       8,    0,   58,    2, 0x08,    7 /* Private */,
       9,    1,   59,    2, 0x08,    8 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,    6,    7,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   10,

       0        // eod
};

Q_CONSTINIT const QMetaObject RecorderTab::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_RecorderTab.offsetsAndSizes,
    qt_meta_data_RecorderTab,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_RecorderTab_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<RecorderTab, std::true_type>,
        // method 'onStartRecording'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onStopRecording'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'refreshRecorderStats'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onTradeStationAuthStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'onBrowseButtonClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCsvFilePathChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void RecorderTab::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<RecorderTab *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onStartRecording(); break;
        case 1: _t->onStopRecording(); break;
        case 2: _t->refreshRecorderStats(); break;
        case 3: _t->onTradeStationAuthStateChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->onBrowseButtonClicked(); break;
        case 5: _t->onCsvFilePathChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *RecorderTab::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RecorderTab::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_RecorderTab.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int RecorderTab::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
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
QT_WARNING_POP
QT_END_MOC_NAMESPACE
