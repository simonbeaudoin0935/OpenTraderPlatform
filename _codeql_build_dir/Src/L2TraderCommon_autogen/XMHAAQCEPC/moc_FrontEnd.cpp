/****************************************************************************
** Meta object code from reading C++ file 'FrontEnd.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Src/FrontEnd/FrontEnd.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'FrontEnd.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_FrontEnd_t {
    uint offsetsAndSizes[90];
    char stringdata0[9];
    char stringdata1[29];
    char stringdata2[1];
    char stringdata3[16];
    char stringdata4[7];
    char stringdata5[29];
    char stringdata6[15];
    char stringdata7[8];
    char stringdata8[29];
    char stringdata9[10];
    char stringdata10[13];
    char stringdata11[19];
    char stringdata12[6];
    char stringdata13[20];
    char stringdata14[8];
    char stringdata15[9];
    char stringdata16[9];
    char stringdata17[16];
    char stringdata18[11];
    char stringdata19[17];
    char stringdata20[6];
    char stringdata21[6];
    char stringdata22[15];
    char stringdata23[8];
    char stringdata24[8];
    char stringdata25[35];
    char stringdata26[7];
    char stringdata27[4];
    char stringdata28[4];
    char stringdata29[46];
    char stringdata30[17];
    char stringdata31[6];
    char stringdata32[16];
    char stringdata33[7];
    char stringdata34[7];
    char stringdata35[26];
    char stringdata36[20];
    char stringdata37[20];
    char stringdata38[31];
    char stringdata39[22];
    char stringdata40[18];
    char stringdata41[19];
    char stringdata42[17];
    char stringdata43[37];
    char stringdata44[48];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_FrontEnd_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_FrontEnd_t qt_meta_stringdata_FrontEnd = {
    {
        QT_MOC_LITERAL(0, 8),  // "FrontEnd"
        QT_MOC_LITERAL(9, 28),  // "tradeStationAuthStateChanged"
        QT_MOC_LITERAL(38, 0),  // ""
        QT_MOC_LITERAL(39, 15),  // "isAuthenticated"
        QT_MOC_LITERAL(55, 6),  // "reason"
        QT_MOC_LITERAL(62, 28),  // "tradeStationAccountsReceived"
        QT_MOC_LITERAL(91, 14),  // "QList<Account>"
        QT_MOC_LITERAL(106, 7),  // "results"
        QT_MOC_LITERAL(114, 28),  // "tradeStationDataUsageUpdated"
        QT_MOC_LITERAL(143, 9),  // "qsizetype"
        QT_MOC_LITERAL(153, 12),  // "newDataUsage"
        QT_MOC_LITERAL(166, 18),  // "streamCountUpdated"
        QT_MOC_LITERAL(185, 5),  // "count"
        QT_MOC_LITERAL(191, 19),  // "newPositionReceived"
        QT_MOC_LITERAL(211, 7),  // "account"
        QT_MOC_LITERAL(219, 8),  // "Position"
        QT_MOC_LITERAL(228, 8),  // "position"
        QT_MOC_LITERAL(237, 15),  // "positionDeleted"
        QT_MOC_LITERAL(253, 10),  // "positionID"
        QT_MOC_LITERAL(264, 16),  // "newOrderReceived"
        QT_MOC_LITERAL(281, 5),  // "Order"
        QT_MOC_LITERAL(287, 5),  // "order"
        QT_MOC_LITERAL(293, 14),  // "balanceUpdated"
        QT_MOC_LITERAL(308, 7),  // "Balance"
        QT_MOC_LITERAL(316, 7),  // "balance"
        QT_MOC_LITERAL(324, 34),  // "currentHighlightedStockBarRec..."
        QT_MOC_LITERAL(359, 6),  // "symbol"
        QT_MOC_LITERAL(366, 3),  // "Bar"
        QT_MOC_LITERAL(370, 3),  // "bar"
        QT_MOC_LITERAL(374, 45),  // "currentHighlightedReceivedNew..."
        QT_MOC_LITERAL(420, 16),  // "MarketDepthQuote"
        QT_MOC_LITERAL(437, 5),  // "quote"
        QT_MOC_LITERAL(443, 15),  // "bidAskImbalance"
        QT_MOC_LITERAL(459, 6),  // "bidDWP"
        QT_MOC_LITERAL(466, 6),  // "askDWP"
        QT_MOC_LITERAL(473, 25),  // "onTSClientDataUsageUpdate"
        QT_MOC_LITERAL(499, 19),  // "onMemoryUsageUpdate"
        QT_MOC_LITERAL(519, 19),  // "onStreamCountUpdate"
        QT_MOC_LITERAL(539, 30),  // "onTradeStationAccountsReceived"
        QT_MOC_LITERAL(570, 21),  // "onNewPositionReceived"
        QT_MOC_LITERAL(592, 17),  // "onPositionDeleted"
        QT_MOC_LITERAL(610, 18),  // "onNewOrderReceived"
        QT_MOC_LITERAL(629, 16),  // "onBalanceUpdated"
        QT_MOC_LITERAL(646, 36),  // "onCurrentHighlightedStockBarR..."
        QT_MOC_LITERAL(683, 47)   // "onCurrentHighlightedReceivedN..."
    },
    "FrontEnd",
    "tradeStationAuthStateChanged",
    "",
    "isAuthenticated",
    "reason",
    "tradeStationAccountsReceived",
    "QList<Account>",
    "results",
    "tradeStationDataUsageUpdated",
    "qsizetype",
    "newDataUsage",
    "streamCountUpdated",
    "count",
    "newPositionReceived",
    "account",
    "Position",
    "position",
    "positionDeleted",
    "positionID",
    "newOrderReceived",
    "Order",
    "order",
    "balanceUpdated",
    "Balance",
    "balance",
    "currentHighlightedStockBarReceived",
    "symbol",
    "Bar",
    "bar",
    "currentHighlightedReceivedNewMarketDepthQuote",
    "MarketDepthQuote",
    "quote",
    "bidAskImbalance",
    "bidDWP",
    "askDWP",
    "onTSClientDataUsageUpdate",
    "onMemoryUsageUpdate",
    "onStreamCountUpdate",
    "onTradeStationAccountsReceived",
    "onNewPositionReceived",
    "onPositionDeleted",
    "onNewOrderReceived",
    "onBalanceUpdated",
    "onCurrentHighlightedStockBarReceived",
    "onCurrentHighlightedReceivedNewMarketDepthQuote"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_FrontEnd[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      20,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      10,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,  134,    2, 0x06,    1 /* Public */,
       5,    1,  139,    2, 0x06,    4 /* Public */,
       8,    1,  142,    2, 0x06,    6 /* Public */,
      11,    1,  145,    2, 0x06,    8 /* Public */,
      13,    2,  148,    2, 0x06,   10 /* Public */,
      17,    2,  153,    2, 0x06,   13 /* Public */,
      19,    2,  158,    2, 0x06,   16 /* Public */,
      22,    1,  163,    2, 0x06,   19 /* Public */,
      25,    2,  166,    2, 0x06,   21 /* Public */,
      29,    5,  171,    2, 0x06,   24 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      35,    1,  182,    2, 0x0a,   30 /* Public */,
      36,    1,  185,    2, 0x0a,   32 /* Public */,
      37,    1,  188,    2, 0x0a,   34 /* Public */,
      38,    1,  191,    2, 0x0a,   36 /* Public */,
      39,    2,  194,    2, 0x0a,   38 /* Public */,
      40,    2,  199,    2, 0x0a,   41 /* Public */,
      41,    2,  204,    2, 0x0a,   44 /* Public */,
      42,    1,  209,    2, 0x0a,   47 /* Public */,
      43,    2,  212,    2, 0x0a,   49 /* Public */,
      44,    5,  217,    2, 0x0a,   52 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,    3,    4,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, 0x80000000 | 9,   10,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 15,   14,   16,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   14,   18,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 20,   14,   21,
    QMetaType::Void, 0x80000000 | 23,   24,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 27,   26,   28,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 30, QMetaType::Double, QMetaType::Double, QMetaType::Double,   26,   31,   32,   33,   34,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 9,   10,
    QMetaType::Void, 0x80000000 | 9,   10,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 15,   14,   16,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   14,   18,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 20,   14,   21,
    QMetaType::Void, 0x80000000 | 23,   24,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 27,   26,   28,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 30, QMetaType::Double, QMetaType::Double, QMetaType::Double,   26,   31,   32,   33,   34,

       0        // eod
};

Q_CONSTINIT const QMetaObject FrontEnd::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_FrontEnd.offsetsAndSizes,
    qt_meta_data_FrontEnd,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_FrontEnd_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<FrontEnd, std::true_type>,
        // method 'tradeStationAuthStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'tradeStationAccountsReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QVector<Account>, std::false_type>,
        // method 'tradeStationDataUsageUpdated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qsizetype, std::false_type>,
        // method 'streamCountUpdated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'newPositionReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<Position, std::false_type>,
        // method 'positionDeleted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'newOrderReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<Order, std::false_type>,
        // method 'balanceUpdated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Balance, std::false_type>,
        // method 'currentHighlightedStockBarReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<Bar, std::false_type>,
        // method 'currentHighlightedReceivedNewMarketDepthQuote'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<MarketDepthQuote, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'onTSClientDataUsageUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qsizetype, std::false_type>,
        // method 'onMemoryUsageUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qsizetype, std::false_type>,
        // method 'onStreamCountUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onTradeStationAccountsReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QVector<Account>, std::false_type>,
        // method 'onNewPositionReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<Position, std::false_type>,
        // method 'onPositionDeleted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'onNewOrderReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<Order, std::false_type>,
        // method 'onBalanceUpdated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Balance, std::false_type>,
        // method 'onCurrentHighlightedStockBarReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<Bar, std::false_type>,
        // method 'onCurrentHighlightedReceivedNewMarketDepthQuote'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<MarketDepthQuote, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>
    >,
    nullptr
} };

void FrontEnd::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<FrontEnd *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->tradeStationAuthStateChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 1: _t->tradeStationAccountsReceived((*reinterpret_cast< std::add_pointer_t<QList<Account>>>(_a[1]))); break;
        case 2: _t->tradeStationDataUsageUpdated((*reinterpret_cast< std::add_pointer_t<qsizetype>>(_a[1]))); break;
        case 3: _t->streamCountUpdated((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->newPositionReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<Position>>(_a[2]))); break;
        case 5: _t->positionDeleted((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 6: _t->newOrderReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<Order>>(_a[2]))); break;
        case 7: _t->balanceUpdated((*reinterpret_cast< std::add_pointer_t<Balance>>(_a[1]))); break;
        case 8: _t->currentHighlightedStockBarReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<Bar>>(_a[2]))); break;
        case 9: _t->currentHighlightedReceivedNewMarketDepthQuote((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<MarketDepthQuote>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[5]))); break;
        case 10: _t->onTSClientDataUsageUpdate((*reinterpret_cast< std::add_pointer_t<qsizetype>>(_a[1]))); break;
        case 11: _t->onMemoryUsageUpdate((*reinterpret_cast< std::add_pointer_t<qsizetype>>(_a[1]))); break;
        case 12: _t->onStreamCountUpdate((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 13: _t->onTradeStationAccountsReceived((*reinterpret_cast< std::add_pointer_t<QList<Account>>>(_a[1]))); break;
        case 14: _t->onNewPositionReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<Position>>(_a[2]))); break;
        case 15: _t->onPositionDeleted((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 16: _t->onNewOrderReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<Order>>(_a[2]))); break;
        case 17: _t->onBalanceUpdated((*reinterpret_cast< std::add_pointer_t<Balance>>(_a[1]))); break;
        case 18: _t->onCurrentHighlightedStockBarReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<Bar>>(_a[2]))); break;
        case 19: _t->onCurrentHighlightedReceivedNewMarketDepthQuote((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<MarketDepthQuote>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[5]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<Account> >(); break;
            }
            break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Position >(); break;
            }
            break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Order >(); break;
            }
            break;
        case 8:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Bar >(); break;
            }
            break;
        case 9:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< MarketDepthQuote >(); break;
            }
            break;
        case 13:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<Account> >(); break;
            }
            break;
        case 14:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Position >(); break;
            }
            break;
        case 16:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Order >(); break;
            }
            break;
        case 18:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Bar >(); break;
            }
            break;
        case 19:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< MarketDepthQuote >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (FrontEnd::*)(bool , QString );
            if (_t _q_method = &FrontEnd::tradeStationAuthStateChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (FrontEnd::*)(QVector<Account> );
            if (_t _q_method = &FrontEnd::tradeStationAccountsReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (FrontEnd::*)(qsizetype );
            if (_t _q_method = &FrontEnd::tradeStationDataUsageUpdated; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (FrontEnd::*)(int );
            if (_t _q_method = &FrontEnd::streamCountUpdated; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (FrontEnd::*)(QString , Position );
            if (_t _q_method = &FrontEnd::newPositionReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (FrontEnd::*)(QString , QString );
            if (_t _q_method = &FrontEnd::positionDeleted; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (FrontEnd::*)(QString , Order );
            if (_t _q_method = &FrontEnd::newOrderReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (FrontEnd::*)(Balance );
            if (_t _q_method = &FrontEnd::balanceUpdated; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _t = void (FrontEnd::*)(QString , Bar );
            if (_t _q_method = &FrontEnd::currentHighlightedStockBarReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _t = void (FrontEnd::*)(QString , MarketDepthQuote , double , double , double );
            if (_t _q_method = &FrontEnd::currentHighlightedReceivedNewMarketDepthQuote; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
    }
}

const QMetaObject *FrontEnd::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FrontEnd::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_FrontEnd.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int FrontEnd::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 20)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 20;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 20)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 20;
    }
    return _id;
}

// SIGNAL 0
void FrontEnd::tradeStationAuthStateChanged(bool _t1, QString _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void FrontEnd::tradeStationAccountsReceived(QVector<Account> _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void FrontEnd::tradeStationDataUsageUpdated(qsizetype _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void FrontEnd::streamCountUpdated(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void FrontEnd::newPositionReceived(QString _t1, Position _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void FrontEnd::positionDeleted(QString _t1, QString _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void FrontEnd::newOrderReceived(QString _t1, Order _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void FrontEnd::balanceUpdated(Balance _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void FrontEnd::currentHighlightedStockBarReceived(QString _t1, Bar _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void FrontEnd::currentHighlightedReceivedNewMarketDepthQuote(QString _t1, MarketDepthQuote _t2, double _t3, double _t4, double _t5)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t5))) };
    QMetaObject::activate(this, &staticMetaObject, 9, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
