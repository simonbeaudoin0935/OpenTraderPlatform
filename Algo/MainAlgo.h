#ifndef MAINALGO_H
#define MAINALGO_H

#include <QLoggingCategory>
#include <QObject>


// Define the logging category
Q_DECLARE_LOGGING_CATEGORY(MainAlgoLog)


class QThread;

class MainAlgo : public QObject
{
    Q_OBJECT
public:
    MainAlgo();

    void start();

private slots:
    void onThreadStarted();

private:
    void loadCriterias();

    QThread *thread;

    double PriceRangeLow;
    double PriceRangeHigh;
    unsigned long long PreferedFloat;
    unsigned long long MaxFloat;
    double RelativeVolume;
    double GapPercentage;

};

#endif // MAINALGO_H
