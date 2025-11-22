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

    // Cache hierarchy tests
    void testDatabasePersistence();
    void testDatabaseOnlyHit();
    void testMixedCacheStates();
    void testCacheClearingAndRepopulation();

    // Edge case tests
    void testSingleBarRequest();
    void testLargeRangeRequest();
    void testOverlappingRequests();
    void testBoundaryConditions();

    // Data integrity tests
    void testIdentifyMissingRanges();
    void testNullBarsMixedWithRealBars();
    void testDuplicateStoreTracking();

    // Metrics validation tests
    void testHitTypeAccuracy();
    void testMetricsTracking();
private:
};
