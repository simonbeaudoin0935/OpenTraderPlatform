/********************************************************************************
** Form generated from reading UI file 'GUIFrontend.ui'
**
** Created by: Qt User Interface Compiler version 6.4.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_GUIFRONTEND_H
#define UI_GUIFRONTEND_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "GUI/BalanceWindow.h"
#include "GUI/Gauge/Gauge.h"
#include "GUI/MarketDepthTable.h"
#include "GUI/OrderEntryWidget.h"
#include "GUI/OrderWindow.h"
#include "GUI/PositionWindow.h"
#include "GUI/StockPriceChart/StockPriceChart.h"

QT_BEGIN_NAMESPACE

class Ui_GUIFrontend
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *topControlsLayout;
    QLineEdit *stockSymbolInput;
    QComboBox *accountSelector;
    QSpacerItem *topControlsSpacer;
    QSplitter *mainSplitter;
    QTabWidget *tabWidget;
    QWidget *tab;
    QVBoxLayout *tradeTabMainLayout;
    QSplitter *tradeTabSplitter;
    QWidget *tradeTopWidget;
    QHBoxLayout *horizontalLayout;
    StockPriceChart *priceChart;
    QWidget *rightPanel;
    QVBoxLayout *rightPanelLayout;
    QWidget *gaugeContainer;
    QGridLayout *gaugeGridLayout;
    Gauge *baiGauge;
    Gauge *dwpHGauge;
    Gauge *oblrGauge;
    Gauge *qrrGauge;
    MarketDepthTable *marketDepthTable;
    QWidget *tradeTabBottomWidget;
    QVBoxLayout *tradeTabBottomWrapperLayout;
    QSplitter *tradeTabBottomSplitter;
    QTextEdit *logDisplay;
    BalanceWindow *balanceWindow;
    PositionWindow *positionWindow;
    OrderWindow *orderWindow;
    OrderEntryWidget *orderEntryWidget;
    QWidget *tab_2;
    QTableView *tableView;
    QPushButton *pushButton;
    QTextEdit *liveLogDisplay;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *GUIFrontend)
    {
        if (GUIFrontend->objectName().isEmpty())
            GUIFrontend->setObjectName("GUIFrontend");
        GUIFrontend->resize(996, 727);
        centralwidget = new QWidget(GUIFrontend);
        centralwidget->setObjectName("centralwidget");
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName("verticalLayout");
        topControlsLayout = new QHBoxLayout();
        topControlsLayout->setObjectName("topControlsLayout");
        stockSymbolInput = new QLineEdit(centralwidget);
        stockSymbolInput->setObjectName("stockSymbolInput");
        stockSymbolInput->setMaximumWidth(75);
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(stockSymbolInput->sizePolicy().hasHeightForWidth());
        stockSymbolInput->setSizePolicy(sizePolicy);

        topControlsLayout->addWidget(stockSymbolInput);

        accountSelector = new QComboBox(centralwidget);
        accountSelector->setObjectName("accountSelector");
        accountSelector->setMinimumWidth(200);
        accountSelector->setMaximumWidth(250);
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(accountSelector->sizePolicy().hasHeightForWidth());
        accountSelector->setSizePolicy(sizePolicy1);

        topControlsLayout->addWidget(accountSelector);

        topControlsSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        topControlsLayout->addItem(topControlsSpacer);


        verticalLayout->addLayout(topControlsLayout);

        mainSplitter = new QSplitter(centralwidget);
        mainSplitter->setObjectName("mainSplitter");
        mainSplitter->setOrientation(Qt::Orientation::Vertical);
        tabWidget = new QTabWidget(mainSplitter);
        tabWidget->setObjectName("tabWidget");
        tabWidget->setTabPosition(QTabWidget::TabPosition::North);
        tabWidget->setTabShape(QTabWidget::TabShape::Rounded);
        tabWidget->setElideMode(Qt::TextElideMode::ElideNone);
        tab = new QWidget();
        tab->setObjectName("tab");
        tradeTabMainLayout = new QVBoxLayout(tab);
        tradeTabMainLayout->setObjectName("tradeTabMainLayout");
        tradeTabSplitter = new QSplitter(tab);
        tradeTabSplitter->setObjectName("tradeTabSplitter");
        tradeTabSplitter->setOrientation(Qt::Orientation::Vertical);
        tradeTopWidget = new QWidget(tradeTabSplitter);
        tradeTopWidget->setObjectName("tradeTopWidget");
        horizontalLayout = new QHBoxLayout(tradeTopWidget);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        priceChart = new StockPriceChart(tradeTopWidget);
        priceChart->setObjectName("priceChart");
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy2.setHorizontalStretch(1);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(priceChart->sizePolicy().hasHeightForWidth());
        priceChart->setSizePolicy(sizePolicy2);

        horizontalLayout->addWidget(priceChart);

        rightPanel = new QWidget(tradeTopWidget);
        rightPanel->setObjectName("rightPanel");
        QSizePolicy sizePolicy3(QSizePolicy::Fixed, QSizePolicy::Expanding);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(rightPanel->sizePolicy().hasHeightForWidth());
        rightPanel->setSizePolicy(sizePolicy3);
        rightPanelLayout = new QVBoxLayout(rightPanel);
        rightPanelLayout->setObjectName("rightPanelLayout");
        rightPanelLayout->setContentsMargins(0, 0, 0, 0);
        gaugeContainer = new QWidget(rightPanel);
        gaugeContainer->setObjectName("gaugeContainer");
        QSizePolicy sizePolicy4(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(gaugeContainer->sizePolicy().hasHeightForWidth());
        gaugeContainer->setSizePolicy(sizePolicy4);
        gaugeGridLayout = new QGridLayout(gaugeContainer);
        gaugeGridLayout->setSpacing(4);
        gaugeGridLayout->setObjectName("gaugeGridLayout");
        baiGauge = new Gauge(gaugeContainer);
        baiGauge->setObjectName("baiGauge");
        QSizePolicy sizePolicy5(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy5.setHorizontalStretch(1);
        sizePolicy5.setVerticalStretch(1);
        sizePolicy5.setHeightForWidth(baiGauge->sizePolicy().hasHeightForWidth());
        baiGauge->setSizePolicy(sizePolicy5);

        gaugeGridLayout->addWidget(baiGauge, 0, 0, 1, 1);

        dwpHGauge = new Gauge(gaugeContainer);
        dwpHGauge->setObjectName("dwpHGauge");
        sizePolicy5.setHeightForWidth(dwpHGauge->sizePolicy().hasHeightForWidth());
        dwpHGauge->setSizePolicy(sizePolicy5);

        gaugeGridLayout->addWidget(dwpHGauge, 0, 1, 1, 1);

        oblrGauge = new Gauge(gaugeContainer);
        oblrGauge->setObjectName("oblrGauge");
        sizePolicy5.setHeightForWidth(oblrGauge->sizePolicy().hasHeightForWidth());
        oblrGauge->setSizePolicy(sizePolicy5);

        gaugeGridLayout->addWidget(oblrGauge, 1, 0, 1, 1);

        qrrGauge = new Gauge(gaugeContainer);
        qrrGauge->setObjectName("qrrGauge");
        sizePolicy5.setHeightForWidth(qrrGauge->sizePolicy().hasHeightForWidth());
        qrrGauge->setSizePolicy(sizePolicy5);

        gaugeGridLayout->addWidget(qrrGauge, 1, 1, 1, 1);


        rightPanelLayout->addWidget(gaugeContainer);

        marketDepthTable = new MarketDepthTable(rightPanel);
        marketDepthTable->setObjectName("marketDepthTable");
        QSizePolicy sizePolicy6(QSizePolicy::Fixed, QSizePolicy::Expanding);
        sizePolicy6.setHorizontalStretch(0);
        sizePolicy6.setVerticalStretch(1);
        sizePolicy6.setHeightForWidth(marketDepthTable->sizePolicy().hasHeightForWidth());
        marketDepthTable->setSizePolicy(sizePolicy6);

        rightPanelLayout->addWidget(marketDepthTable);


        horizontalLayout->addWidget(rightPanel);

        tradeTabSplitter->addWidget(tradeTopWidget);
        tradeTabBottomWidget = new QWidget(tradeTabSplitter);
        tradeTabBottomWidget->setObjectName("tradeTabBottomWidget");
        tradeTabBottomWrapperLayout = new QVBoxLayout(tradeTabBottomWidget);
        tradeTabBottomWrapperLayout->setSpacing(0);
        tradeTabBottomWrapperLayout->setObjectName("tradeTabBottomWrapperLayout");
        tradeTabBottomWrapperLayout->setContentsMargins(0, 0, 0, 0);
        tradeTabBottomSplitter = new QSplitter(tradeTabBottomWidget);
        tradeTabBottomSplitter->setObjectName("tradeTabBottomSplitter");
        tradeTabBottomSplitter->setOrientation(Qt::Orientation::Horizontal);
        tradeTabBottomSplitter->setChildrenCollapsible(true);
        logDisplay = new QTextEdit(tradeTabBottomSplitter);
        logDisplay->setObjectName("logDisplay");
        sizePolicy2.setHeightForWidth(logDisplay->sizePolicy().hasHeightForWidth());
        logDisplay->setSizePolicy(sizePolicy2);
        tradeTabBottomSplitter->addWidget(logDisplay);
        balanceWindow = new BalanceWindow(tradeTabBottomSplitter);
        balanceWindow->setObjectName("balanceWindow");
        QSizePolicy sizePolicy7(QSizePolicy::Preferred, QSizePolicy::Expanding);
        sizePolicy7.setHorizontalStretch(0);
        sizePolicy7.setVerticalStretch(0);
        sizePolicy7.setHeightForWidth(balanceWindow->sizePolicy().hasHeightForWidth());
        balanceWindow->setSizePolicy(sizePolicy7);
        tradeTabBottomSplitter->addWidget(balanceWindow);
        positionWindow = new PositionWindow(tradeTabBottomSplitter);
        positionWindow->setObjectName("positionWindow");
        sizePolicy7.setHeightForWidth(positionWindow->sizePolicy().hasHeightForWidth());
        positionWindow->setSizePolicy(sizePolicy7);
        tradeTabBottomSplitter->addWidget(positionWindow);
        orderWindow = new OrderWindow(tradeTabBottomSplitter);
        orderWindow->setObjectName("orderWindow");
        sizePolicy7.setHeightForWidth(orderWindow->sizePolicy().hasHeightForWidth());
        orderWindow->setSizePolicy(sizePolicy7);
        tradeTabBottomSplitter->addWidget(orderWindow);
        orderEntryWidget = new OrderEntryWidget(tradeTabBottomSplitter);
        orderEntryWidget->setObjectName("orderEntryWidget");
        sizePolicy7.setHeightForWidth(orderEntryWidget->sizePolicy().hasHeightForWidth());
        orderEntryWidget->setSizePolicy(sizePolicy7);
        tradeTabBottomSplitter->addWidget(orderEntryWidget);

        tradeTabBottomWrapperLayout->addWidget(tradeTabBottomSplitter);

        tradeTabSplitter->addWidget(tradeTabBottomWidget);

        tradeTabMainLayout->addWidget(tradeTabSplitter);

        tabWidget->addTab(tab, QString());
        tab_2 = new QWidget();
        tab_2->setObjectName("tab_2");
        tableView = new QTableView(tab_2);
        tableView->setObjectName("tableView");
        tableView->setGeometry(QRect(60, 30, 211, 251));
        pushButton = new QPushButton(tab_2);
        pushButton->setObjectName("pushButton");
        pushButton->setGeometry(QRect(340, 240, 80, 23));
        tabWidget->addTab(tab_2, QString());
        mainSplitter->addWidget(tabWidget);
        liveLogDisplay = new QTextEdit(mainSplitter);
        liveLogDisplay->setObjectName("liveLogDisplay");
        sizePolicy4.setHeightForWidth(liveLogDisplay->sizePolicy().hasHeightForWidth());
        liveLogDisplay->setSizePolicy(sizePolicy4);
        liveLogDisplay->setReadOnly(true);
        mainSplitter->addWidget(liveLogDisplay);

        verticalLayout->addWidget(mainSplitter);

        GUIFrontend->setCentralWidget(centralwidget);
        menubar = new QMenuBar(GUIFrontend);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 996, 20));
        GUIFrontend->setMenuBar(menubar);
        statusbar = new QStatusBar(GUIFrontend);
        statusbar->setObjectName("statusbar");
        GUIFrontend->setStatusBar(statusbar);

        retranslateUi(GUIFrontend);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(GUIFrontend);
    } // setupUi

    void retranslateUi(QMainWindow *GUIFrontend)
    {
        GUIFrontend->setWindowTitle(QCoreApplication::translate("GUIFrontend", "L2Trader", nullptr));
        stockSymbolInput->setPlaceholderText(QCoreApplication::translate("GUIFrontend", "Enter stock symbol and press Enter", nullptr));
        baiGauge->setProperty("label", QVariant(QCoreApplication::translate("GUIFrontend", "BAI", nullptr)));
        dwpHGauge->setProperty("label", QVariant(QCoreApplication::translate("GUIFrontend", "DWP", nullptr)));
        oblrGauge->setProperty("label", QVariant(QCoreApplication::translate("GUIFrontend", "OBLR", nullptr)));
        qrrGauge->setProperty("label", QVariant(QCoreApplication::translate("GUIFrontend", "QRR", nullptr)));
        tabWidget->setTabText(tabWidget->indexOf(tab), QCoreApplication::translate("GUIFrontend", "Trade", nullptr));
        pushButton->setText(QCoreApplication::translate("GUIFrontend", "PushButton", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_2), QCoreApplication::translate("GUIFrontend", "Settings", nullptr));
    } // retranslateUi

};

namespace Ui {
    class GUIFrontend: public Ui_GUIFrontend {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_GUIFRONTEND_H
