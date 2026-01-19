/****************************************************************************
** Meta object code from reading C++ file 'GUIFrontend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../Src/FrontEnd/GUI/GUIFrontend.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'GUIFrontend.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_GUIFrontend_t {
    uint offsetsAndSizes[102];
    char stringdata0[12];
    char stringdata1[26];
    char stringdata2[1];
    char stringdata3[10];
    char stringdata4[13];
    char stringdata5[31];
    char stringdata6[15];
    char stringdata7[8];
    char stringdata8[20];
    char stringdata9[20];
    char stringdata10[6];
    char stringdata11[37];
    char stringdata12[7];
    char stringdata13[4];
    char stringdata14[4];
    char stringdata15[48];
    char stringdata16[17];
    char stringdata17[6];
    char stringdata18[16];
    char stringdata19[7];
    char stringdata20[7];
    char stringdata21[22];
    char stringdata22[8];
    char stringdata23[9];
    char stringdata24[9];
    char stringdata25[18];
    char stringdata26[11];
    char stringdata27[19];
    char stringdata28[6];
    char stringdata29[6];
    char stringdata30[17];
    char stringdata31[8];
    char stringdata32[8];
    char stringdata33[31];
    char stringdata34[16];
    char stringdata35[7];
    char stringdata36[29];
    char stringdata37[21];
    char stringdata38[8];
    char stringdata39[26];
    char stringdata40[8];
    char stringdata41[18];
    char stringdata42[9];
    char stringdata43[14];
    char stringdata44[18];
    char stringdata45[18];
    char stringdata46[29];
    char stringdata47[5];
    char stringdata48[13];
    char stringdata49[14];
    char stringdata50[18];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_GUIFrontend_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_GUIFrontend_t qt_meta_stringdata_GUIFrontend = {
    {
        QT_MOC_LITERAL(0, 11),  // "GUIFrontend"
        QT_MOC_LITERAL(12, 25),  // "onTSClientDataUsageUpdate"
        QT_MOC_LITERAL(38, 0),  // ""
        QT_MOC_LITERAL(39, 9),  // "qsizetype"
        QT_MOC_LITERAL(49, 12),  // "newDataUsage"
        QT_MOC_LITERAL(62, 30),  // "onTradeStationAccountsReceived"
        QT_MOC_LITERAL(93, 14),  // "QList<Account>"
        QT_MOC_LITERAL(108, 7),  // "results"
        QT_MOC_LITERAL(116, 19),  // "onMemoryUsageUpdate"
        QT_MOC_LITERAL(136, 19),  // "onStreamCountUpdate"
        QT_MOC_LITERAL(156, 5),  // "count"
        QT_MOC_LITERAL(162, 36),  // "onCurrentHighlightedStockBarR..."
        QT_MOC_LITERAL(199, 6),  // "symbol"
        QT_MOC_LITERAL(206, 3),  // "Bar"
        QT_MOC_LITERAL(210, 3),  // "bar"
        QT_MOC_LITERAL(214, 47),  // "onCurrentHighlightedReceivedN..."
        QT_MOC_LITERAL(262, 16),  // "MarketDepthQuote"
        QT_MOC_LITERAL(279, 5),  // "quote"
        QT_MOC_LITERAL(285, 15),  // "bidAskImbalance"
        QT_MOC_LITERAL(301, 6),  // "bidDWP"
        QT_MOC_LITERAL(308, 6),  // "askDWP"
        QT_MOC_LITERAL(315, 21),  // "onNewPositionReceived"
        QT_MOC_LITERAL(337, 7),  // "account"
        QT_MOC_LITERAL(345, 8),  // "Position"
        QT_MOC_LITERAL(354, 8),  // "position"
        QT_MOC_LITERAL(363, 17),  // "onPositionDeleted"
        QT_MOC_LITERAL(381, 10),  // "positionID"
        QT_MOC_LITERAL(392, 18),  // "onNewOrderReceived"
        QT_MOC_LITERAL(411, 5),  // "Order"
        QT_MOC_LITERAL(417, 5),  // "order"
        QT_MOC_LITERAL(423, 16),  // "onBalanceUpdated"
        QT_MOC_LITERAL(440, 7),  // "Balance"
        QT_MOC_LITERAL(448, 7),  // "balance"
        QT_MOC_LITERAL(456, 30),  // "onTradeStationAuthStateChanged"
        QT_MOC_LITERAL(487, 15),  // "isAuthenticated"
        QT_MOC_LITERAL(503, 6),  // "reason"
        QT_MOC_LITERAL(510, 28),  // "onNewDisplayedStockSelection"
        QT_MOC_LITERAL(539, 20),  // "updateLiveLogDisplay"
        QT_MOC_LITERAL(560, 7),  // "message"
        QT_MOC_LITERAL(568, 25),  // "onLoggerVisibilityChanged"
        QT_MOC_LITERAL(594, 7),  // "visible"
        QT_MOC_LITERAL(602, 17),  // "onLogDepthChanged"
        QT_MOC_LITERAL(620, 8),  // "maxLines"
        QT_MOC_LITERAL(629, 13),  // "onOrderPlaced"
        QT_MOC_LITERAL(643, 17),  // "PlaceOrderRequest"
        QT_MOC_LITERAL(661, 17),  // "onShortcutChanged"
        QT_MOC_LITERAL(679, 28),  // "ShortcutSettings::ShortcutId"
        QT_MOC_LITERAL(708, 4),  // "p_id"
        QT_MOC_LITERAL(713, 12),  // "QKeySequence"
        QT_MOC_LITERAL(726, 13),  // "p_newSequence"
        QT_MOC_LITERAL(740, 17)   // "onCancelAllOrders"
    },
    "GUIFrontend",
    "onTSClientDataUsageUpdate",
    "",
    "qsizetype",
    "newDataUsage",
    "onTradeStationAccountsReceived",
    "QList<Account>",
    "results",
    "onMemoryUsageUpdate",
    "onStreamCountUpdate",
    "count",
    "onCurrentHighlightedStockBarReceived",
    "symbol",
    "Bar",
    "bar",
    "onCurrentHighlightedReceivedNewMarketDepthQuote",
    "MarketDepthQuote",
    "quote",
    "bidAskImbalance",
    "bidDWP",
    "askDWP",
    "onNewPositionReceived",
    "account",
    "Position",
    "position",
    "onPositionDeleted",
    "positionID",
    "onNewOrderReceived",
    "Order",
    "order",
    "onBalanceUpdated",
    "Balance",
    "balance",
    "onTradeStationAuthStateChanged",
    "isAuthenticated",
    "reason",
    "onNewDisplayedStockSelection",
    "updateLiveLogDisplay",
    "message",
    "onLoggerVisibilityChanged",
    "visible",
    "onLogDepthChanged",
    "maxLines",
    "onOrderPlaced",
    "PlaceOrderRequest",
    "onShortcutChanged",
    "ShortcutSettings::ShortcutId",
    "p_id",
    "QKeySequence",
    "p_newSequence",
    "onCancelAllOrders"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_GUIFrontend[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      18,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,  122,    2, 0x0a,    1 /* Public */,
       5,    1,  125,    2, 0x0a,    3 /* Public */,
       8,    1,  128,    2, 0x0a,    5 /* Public */,
       9,    1,  131,    2, 0x0a,    7 /* Public */,
      11,    2,  134,    2, 0x0a,    9 /* Public */,
      15,    5,  139,    2, 0x0a,   12 /* Public */,
      21,    2,  150,    2, 0x0a,   18 /* Public */,
      25,    2,  155,    2, 0x0a,   21 /* Public */,
      27,    2,  160,    2, 0x0a,   24 /* Public */,
      30,    1,  165,    2, 0x0a,   27 /* Public */,
      33,    2,  168,    2, 0x08,   29 /* Private */,
      36,    0,  173,    2, 0x08,   32 /* Private */,
      37,    1,  174,    2, 0x08,   33 /* Private */,
      39,    1,  177,    2, 0x08,   35 /* Private */,
      41,    1,  180,    2, 0x08,   37 /* Private */,
      43,    1,  183,    2, 0x08,   39 /* Private */,
      45,    2,  186,    2, 0x08,   41 /* Private */,
      50,    0,  191,    2, 0x08,   44 /* Private */,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::Int,   10,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 13,   12,   14,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 16, QMetaType::Double, QMetaType::Double, QMetaType::Double,   12,   17,   18,   19,   20,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 23,   22,   24,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   22,   26,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 28,   22,   29,
    QMetaType::Void, 0x80000000 | 31,   32,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   34,   35,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   38,
    QMetaType::Void, QMetaType::Bool,   40,
    QMetaType::Void, QMetaType::Int,   42,
    QMetaType::Void, 0x80000000 | 44,   29,
    QMetaType::Void, 0x80000000 | 46, 0x80000000 | 48,   47,   49,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject GUIFrontend::staticMetaObject = { {
    QMetaObject::SuperData::link<FrontEnd::staticMetaObject>(),
    qt_meta_stringdata_GUIFrontend.offsetsAndSizes,
    qt_meta_data_GUIFrontend,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_GUIFrontend_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<GUIFrontend, std::true_type>,
        // method 'onTSClientDataUsageUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qsizetype, std::false_type>,
        // method 'onTradeStationAccountsReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QVector<Account>, std::false_type>,
        // method 'onMemoryUsageUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qsizetype, std::false_type>,
        // method 'onStreamCountUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
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
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
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
        // method 'onTradeStationAuthStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'onNewDisplayedStockSelection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateLiveLogDisplay'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onLoggerVisibilityChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onLogDepthChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onOrderPlaced'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const PlaceOrderRequest &, std::false_type>,
        // method 'onShortcutChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<ShortcutSettings::ShortcutId, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QKeySequence &, std::false_type>,
        // method 'onCancelAllOrders'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void GUIFrontend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<GUIFrontend *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onTSClientDataUsageUpdate((*reinterpret_cast< std::add_pointer_t<qsizetype>>(_a[1]))); break;
        case 1: _t->onTradeStationAccountsReceived((*reinterpret_cast< std::add_pointer_t<QList<Account>>>(_a[1]))); break;
        case 2: _t->onMemoryUsageUpdate((*reinterpret_cast< std::add_pointer_t<qsizetype>>(_a[1]))); break;
        case 3: _t->onStreamCountUpdate((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->onCurrentHighlightedStockBarReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<Bar>>(_a[2]))); break;
        case 5: _t->onCurrentHighlightedReceivedNewMarketDepthQuote((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<MarketDepthQuote>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[5]))); break;
        case 6: _t->onNewPositionReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<Position>>(_a[2]))); break;
        case 7: _t->onPositionDeleted((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->onNewOrderReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<Order>>(_a[2]))); break;
        case 9: _t->onBalanceUpdated((*reinterpret_cast< std::add_pointer_t<Balance>>(_a[1]))); break;
        case 10: _t->onTradeStationAuthStateChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 11: _t->onNewDisplayedStockSelection(); break;
        case 12: _t->updateLiveLogDisplay((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->onLoggerVisibilityChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 14: _t->onLogDepthChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 15: _t->onOrderPlaced((*reinterpret_cast< std::add_pointer_t<PlaceOrderRequest>>(_a[1]))); break;
        case 16: _t->onShortcutChanged((*reinterpret_cast< std::add_pointer_t<ShortcutSettings::ShortcutId>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QKeySequence>>(_a[2]))); break;
        case 17: _t->onCancelAllOrders(); break;
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
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Bar >(); break;
            }
            break;
        case 5:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< MarketDepthQuote >(); break;
            }
            break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Position >(); break;
            }
            break;
        case 8:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Order >(); break;
            }
            break;
        }
    }
}

const QMetaObject *GUIFrontend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *GUIFrontend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_GUIFrontend.stringdata0))
        return static_cast<void*>(this);
    return FrontEnd::qt_metacast(_clname);
}

int GUIFrontend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = FrontEnd::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
