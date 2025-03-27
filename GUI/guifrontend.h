#ifndef GUIFRONTEND_H
#define GUIFRONTEND_H

#include "../Core/appfrontend.h"
#include <QMainWindow>

// Forward declare the generated UI class
namespace Ui {
class GuiFrontend;
}

class GuiFrontend : public AppFrontend {
    Q_OBJECT
public:
    explicit GuiFrontend(QObject* parent = nullptr);
    ~GuiFrontend() override;

public slots:
    void onPriceUpdated(const QJsonObject& priceData) override;
    void onPricesFetched() override;

private:
    Ui::GuiFrontend* ui;  // Pointer to the UI object
};

#endif // GUIFRONTEND_H
