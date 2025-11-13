#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QMap>

class LoggingTab : public QWidget {
    Q_OBJECT

public:
    explicit LoggingTab(QWidget* parent = nullptr);
    ~LoggingTab() override = default;

private slots:
    void onCategoryCheckBoxToggled(bool checked);

private:
    void setupUI();
    void populateCategoryCheckboxes();

    QVBoxLayout* categoryCheckBoxLayout;
    QMap<QString, QCheckBox*> categoryCheckBoxes;
};