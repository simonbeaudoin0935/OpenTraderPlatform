# Offscreen frontend component tests

[IndicatorZoomTests.cpp](IndicatorZoomTests.cpp) exercises MACD/RSI plot ranges:
manual zoom/pan persists through new bars, forming-bar changes and visibility
toggles; clearing resets range behavior; untouched MACD still autoscales.

CTest sets `QT_QPA_PLATFORM=offscreen`; this executable requires QApplication.
The target's float-equality warning exception accommodates vendored QCustomPlot
exact range comparisons. Do not broaden exceptions to other test targets.
These cases check range UX, not the numerical correctness of indicator formulas.
