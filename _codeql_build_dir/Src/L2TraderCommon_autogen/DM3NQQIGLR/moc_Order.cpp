/****************************************************************************
** Meta object code from reading C++ file 'Order.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Src/Clients/TSClient/Brokerage/GetOrders/Order.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Order.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_OrderNS__AdvancedOptions_t {
    uint offsetsAndSizes[22];
    char stringdata0[25];
    char stringdata1[5];
    char stringdata2[4];
    char stringdata3[4];
    char stringdata4[4];
    char stringdata5[7];
    char stringdata6[6];
    char stringdata7[4];
    char stringdata8[7];
    char stringdata9[4];
    char stringdata10[4];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_OrderNS__AdvancedOptions_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_OrderNS__AdvancedOptions_t qt_meta_stringdata_OrderNS__AdvancedOptions = {
    {
        QT_MOC_LITERAL(0, 24),  // "OrderNS::AdvancedOptions"
        QT_MOC_LITERAL(25, 4),  // "Type"
        QT_MOC_LITERAL(30, 3),  // "CND"
        QT_MOC_LITERAL(34, 3),  // "AON"
        QT_MOC_LITERAL(38, 3),  // "TRL"
        QT_MOC_LITERAL(42, 6),  // "SHWQTY"
        QT_MOC_LITERAL(49, 5),  // "DSCPR"
        QT_MOC_LITERAL(55, 3),  // "NON"
        QT_MOC_LITERAL(59, 6),  // "PEGVAL"
        QT_MOC_LITERAL(66, 3),  // "BKO"
        QT_MOC_LITERAL(70, 3)   // "PSO"
    },
    "OrderNS::AdvancedOptions",
    "Type",
    "CND",
    "AON",
    "TRL",
    "SHWQTY",
    "DSCPR",
    "NON",
    "PEGVAL",
    "BKO",
    "PSO"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_OrderNS__AdvancedOptions[] = {

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
       1,    1, 0x2,    9,   19,

 // enum data: key, value
       2, uint(OrderNS::AdvancedOptions::Type::CND),
       3, uint(OrderNS::AdvancedOptions::Type::AON),
       4, uint(OrderNS::AdvancedOptions::Type::TRL),
       5, uint(OrderNS::AdvancedOptions::Type::SHWQTY),
       6, uint(OrderNS::AdvancedOptions::Type::DSCPR),
       7, uint(OrderNS::AdvancedOptions::Type::NON),
       8, uint(OrderNS::AdvancedOptions::Type::PEGVAL),
       9, uint(OrderNS::AdvancedOptions::Type::BKO),
      10, uint(OrderNS::AdvancedOptions::Type::PSO),

       0        // eod
};

Q_CONSTINIT const QMetaObject OrderNS::AdvancedOptions::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_OrderNS__AdvancedOptions.offsetsAndSizes,
    qt_meta_data_OrderNS__AdvancedOptions,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_OrderNS__AdvancedOptions_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AdvancedOptions, std::true_type>
    >,
    nullptr
} };

namespace {
struct qt_meta_stringdata_Order_t {
    uint offsetsAndSizes[44];
    char stringdata0[6];
    char stringdata1[7];
    char stringdata2[4];
    char stringdata3[4];
    char stringdata4[4];
    char stringdata5[4];
    char stringdata6[4];
    char stringdata7[4];
    char stringdata8[4];
    char stringdata9[4];
    char stringdata10[4];
    char stringdata11[4];
    char stringdata12[4];
    char stringdata13[4];
    char stringdata14[4];
    char stringdata15[4];
    char stringdata16[4];
    char stringdata17[4];
    char stringdata18[4];
    char stringdata19[4];
    char stringdata20[4];
    char stringdata21[4];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_Order_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_Order_t qt_meta_stringdata_Order = {
    {
        QT_MOC_LITERAL(0, 5),  // "Order"
        QT_MOC_LITERAL(6, 6),  // "Status"
        QT_MOC_LITERAL(13, 3),  // "ACK"
        QT_MOC_LITERAL(17, 3),  // "BRO"
        QT_MOC_LITERAL(21, 3),  // "CAN"
        QT_MOC_LITERAL(25, 3),  // "EXP"
        QT_MOC_LITERAL(29, 3),  // "FLL"
        QT_MOC_LITERAL(33, 3),  // "FLP"
        QT_MOC_LITERAL(37, 3),  // "FPR"
        QT_MOC_LITERAL(41, 3),  // "LAT"
        QT_MOC_LITERAL(45, 3),  // "OPN"
        QT_MOC_LITERAL(49, 3),  // "OUT"
        QT_MOC_LITERAL(53, 3),  // "REJ"
        QT_MOC_LITERAL(57, 3),  // "UCH"
        QT_MOC_LITERAL(61, 3),  // "UCN"
        QT_MOC_LITERAL(65, 3),  // "TSC"
        QT_MOC_LITERAL(69, 3),  // "RJC"
        QT_MOC_LITERAL(73, 3),  // "DON"
        QT_MOC_LITERAL(77, 3),  // "RSN"
        QT_MOC_LITERAL(81, 3),  // "CND"
        QT_MOC_LITERAL(85, 3),  // "OSO"
        QT_MOC_LITERAL(89, 3)   // "SUS"
    },
    "Order",
    "Status",
    "ACK",
    "BRO",
    "CAN",
    "EXP",
    "FLL",
    "FLP",
    "FPR",
    "LAT",
    "OPN",
    "OUT",
    "REJ",
    "UCH",
    "UCN",
    "TSC",
    "RJC",
    "DON",
    "RSN",
    "CND",
    "OSO",
    "SUS"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_Order[] = {

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
       1,    1, 0x2,   20,   19,

 // enum data: key, value
       2, uint(Order::Status::ACK),
       3, uint(Order::Status::BRO),
       4, uint(Order::Status::CAN),
       5, uint(Order::Status::EXP),
       6, uint(Order::Status::FLL),
       7, uint(Order::Status::FLP),
       8, uint(Order::Status::FPR),
       9, uint(Order::Status::LAT),
      10, uint(Order::Status::OPN),
      11, uint(Order::Status::OUT),
      12, uint(Order::Status::REJ),
      13, uint(Order::Status::UCH),
      14, uint(Order::Status::UCN),
      15, uint(Order::Status::TSC),
      16, uint(Order::Status::RJC),
      17, uint(Order::Status::DON),
      18, uint(Order::Status::RSN),
      19, uint(Order::Status::CND),
      20, uint(Order::Status::OSO),
      21, uint(Order::Status::SUS),

       0        // eod
};

Q_CONSTINIT const QMetaObject Order::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_Order.offsetsAndSizes,
    qt_meta_data_Order,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_Order_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<Order, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP
QT_END_MOC_NAMESPACE
