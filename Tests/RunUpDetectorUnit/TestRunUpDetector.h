#pragma once

#include <QObject>

class TestRunUpDetector : public QObject {
    Q_OBJECT
private slots:
    void initTestCase_data();
    void initTestCase();
    void init();
    void cleanup();

    void testPriorDayAfterMarket();

private:
};
