#pragma once

#include <QObject>

class TestBarCache : public QObject {
    Q_OBJECT
private slots:
    void initTestCase_data();
    void initTestCase();
    void init();
    void cleanup();

    void testGetBars();
    void testGetBarsOnlyHoles();
    void testGetBarsWithHoles();

    void testBarStreaming();
private:
};
