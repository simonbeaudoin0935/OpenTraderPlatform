/****************************************************************************
** Meta object code from reading C++ file 'PlaceOrder.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Src/Clients/TSClient/OrderExecution/PlaceOrder/PlaceOrder.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PlaceOrder.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_OrderType_t {
    uint offsetsAndSizes[12];
    char stringdata0[10];
    char stringdata1[5];
    char stringdata2[7];
    char stringdata3[6];
    char stringdata4[11];
    char stringdata5[10];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_OrderType_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_OrderType_t qt_meta_stringdata_OrderType = {
    {
        QT_MOC_LITERAL(0, 9),  // "OrderType"
        QT_MOC_LITERAL(10, 4),  // "Type"
        QT_MOC_LITERAL(15, 6),  // "Market"
        QT_MOC_LITERAL(22, 5),  // "Limit"
        QT_MOC_LITERAL(28, 10),  // "StopMarket"
        QT_MOC_LITERAL(39, 9)   // "StopLimit"
    },
    "OrderType",
    "Type",
    "Market",
    "Limit",
    "StopMarket",
    "StopLimit"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_OrderType[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       1,   14, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // enums: name, alias, flags, count, data
       1,    1, 0x2,    4,   19,

 // enum data: key, value
       2, uint(OrderType::Type::Market),
       3, uint(OrderType::Type::Limit),
       4, uint(OrderType::Type::StopMarket),
       5, uint(OrderType::Type::StopLimit),

       0        // eod
};

Q_CONSTINIT const QMetaObject OrderType::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_OrderType.offsetsAndSizes,
    qt_meta_data_OrderType,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_OrderType_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<OrderType, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP
QT_END_MOC_NAMESPACE
