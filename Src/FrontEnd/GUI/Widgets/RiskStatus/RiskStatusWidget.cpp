#include "RiskStatusWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace
{
    [[nodiscard]] QString drawdownBasisLabel(const RiskDrawdownBasis p_basis)
    {
        switch (p_basis)
        {
        case RiskDrawdownBasis::EquityPeak:
            return QStringLiteral("Equity Peak");
        case RiskDrawdownBasis::TodaysProfitLoss:
            return QStringLiteral("Today's PnL");
        case RiskDrawdownBasis::RealizedProfitLoss:
            return QStringLiteral("Realized PnL");
        case RiskDrawdownBasis::TodaysProfitLossFromBaseline:
            return QStringLiteral("Today's PnL (Baseline Loss)");
        }

        return QStringLiteral("Equity Peak");
    }

    [[nodiscard]] bool usesBaselineReference(const RiskDrawdownBasis p_basis)
    {
        return p_basis == RiskDrawdownBasis::TodaysProfitLossFromBaseline;
    }

    [[nodiscard]] QString drawdownReferenceLabel(const RiskDrawdownBasis p_basis)
    {
        return usesBaselineReference(p_basis) ? QStringLiteral("baseline") : QStringLiteral("peak");
    }

    [[nodiscard]] double drawdownReferenceValue(const RiskStatusSnapshot& p_snapshot)
    {
        return usesBaselineReference(p_snapshot.runtime.drawdownBasis) ? p_snapshot.runtime.basisBaseline
                                                                       : p_snapshot.runtime.basisPeak;
    }
} // namespace

RiskStatusWidget::RiskStatusWidget(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    m_primaryLabel = new QLabel(this);
    m_primaryLabel->setStyleSheet("QLabel { color: #d7e6f5; font-weight: bold; font-size: 11px; }");
    layout->addWidget(m_primaryLabel);

    m_headroomBar = new QProgressBar(this);
    m_headroomBar->setRange(0, 1000);
    m_headroomBar->setTextVisible(true);
    m_headroomBar->setFormat("%p%");
    m_headroomBar->setStyleSheet("QProgressBar {"
                                 "    border: 1px solid #2d3f53;"
                                 "    border-radius: 4px;"
                                 "    background-color: #141b22;"
                                 "    color: #eef4fb;"
                                 "    text-align: center;"
                                 "    height: 14px;"
                                 "}"
                                 "QProgressBar::chunk {"
                                 "    background-color: #2ca65a;"
                                 "    border-radius: 3px;"
                                 "}");
    layout->addWidget(m_headroomBar);

    m_secondaryLabel = new QLabel(this);
    m_secondaryLabel->setStyleSheet("QLabel { color: #b6c6d6; font-size: 10px; }");
    m_secondaryLabel->setWordWrap(true);
    layout->addWidget(m_secondaryLabel);

    setObjectName("RiskStatusWidget");
    setStyleSheet("QWidget#RiskStatusWidget {"
                  "    background-color: #10202e;"
                  "    border: 1px solid #30485e;"
                  "    border-radius: 6px;"
                  "}");

    RiskStatusSnapshot emptySnapshot;
    setSnapshot(emptySnapshot);
}

void RiskStatusWidget::applyStateColor(const QString& p_colorHex)
{
    m_headroomBar->setStyleSheet(QString("QProgressBar {"
                                         "    border: 1px solid #2d3f53;"
                                         "    border-radius: 4px;"
                                         "    background-color: #141b22;"
                                         "    color: #eef4fb;"
                                         "    text-align: center;"
                                         "    height: 14px;"
                                         "}"
                                         "QProgressBar::chunk {"
                                         "    background-color: %1;"
                                         "    border-radius: 3px;"
                                         "}")
                                     .arg(p_colorHex));
}

void RiskStatusWidget::setSnapshot(const RiskStatusSnapshot& p_snapshot)
{
    if (p_snapshot.accountId.trimmed().isEmpty())
    {
        m_primaryLabel->setText("Risk: select an account");
        m_secondaryLabel->setText("No account selected.");
        m_headroomBar->setValue(1000);
        m_headroomBar->setFormat("N/A");
        m_headroomBar->setToolTip(QString());
        m_primaryLabel->setToolTip(QString());
        m_secondaryLabel->setToolTip(QString());
        applyStateColor("#3a4b5e");
        return;
    }

    const double remainingPercent = std::clamp(p_snapshot.drawdownRemainingPercent, 0.0, 100.0);
    const double usedPercent = std::clamp(100.0 - remainingPercent, 0.0, 100.0);
    const double drawdownUsedUsd =
        std::max(0.0, p_snapshot.config.dailyDrawdownLimitUsd - p_snapshot.drawdownRemainingUsd);
    const int amberUsed = std::clamp(p_snapshot.config.warningAmberUsedPercent, 0, 100);
    const int redUsed = std::clamp(p_snapshot.config.warningRedUsedPercent, amberUsed, 100);

    QString color = "#2ca65a"; // green
    if (p_snapshot.runtime.tradingLocked || usedPercent >= redUsed)
    {
        color = "#cc3f3f";
    }
    else if (usedPercent >= amberUsed)
    {
        color = "#d28b26";
    }
    applyStateColor(color);

    const QString basisLabel = drawdownBasisLabel(p_snapshot.runtime.drawdownBasis);
    const QString referenceLabel = drawdownReferenceLabel(p_snapshot.runtime.drawdownBasis);
    const double referenceValue = drawdownReferenceValue(p_snapshot);
    m_primaryLabel->setText(QString("Risk %1 | DD headroom: %2% (used %3%)")
                                .arg(p_snapshot.accountId,
                                     QString::number(remainingPercent, 'f', 1),
                                     QString::number(usedPercent, 'f', 1)));
    m_headroomBar->setValue(static_cast<int>(std::llround(remainingPercent * 10.0)));
    m_headroomBar->setFormat(QString("%1% rem / %2% used")
                                 .arg(QString::number(remainingPercent, 'f', 1), QString::number(usedPercent, 'f', 1)));

    QString details = QString("$%1 / $%2 remaining (used $%3) | Basis %4 %5 $%6 -> now $%7 | Entries %8/%9 | Open "
                              "%10/%11")
                          .arg(QString::number(p_snapshot.drawdownRemainingUsd, 'f', 2),
                               QString::number(p_snapshot.config.dailyDrawdownLimitUsd, 'f', 2))
                          .arg(QString::number(drawdownUsedUsd, 'f', 2))
                          .arg(basisLabel,
                               referenceLabel,
                               QString::number(referenceValue, 'f', 2),
                               QString::number(p_snapshot.runtime.currentMetric, 'f', 2))
                          .arg(p_snapshot.runtime.entryTradesCount)
                          .arg(p_snapshot.config.maxDailyEntryTrades)
                          .arg(p_snapshot.runtime.openPositionsCount)
                          .arg(p_snapshot.config.maxOpenPositions);

    if (p_snapshot.runtime.tradingLocked)
    {
        details += QString(" | LOCKED: %1").arg(p_snapshot.runtime.lockReason);
    }
    else if (p_snapshot.cooldownActive)
    {
        details += QString(" | Cooldown until %1")
                       .arg(p_snapshot.runtime.cooldownUntil.toString(QStringLiteral("HH:mm:ss t")));
    }
    else
    {
        details += QString(" | Ready");
    }

    const QString headroomToolTip = QString("Headroom bar shows remaining drawdown budget.\n"
                                            "Used drawdown: $%1 (%2%%)\n"
                                            "Basis: %3, %4=$%5, current=$%6")
                                        .arg(QString::number(drawdownUsedUsd, 'f', 2),
                                             QString::number(usedPercent, 'f', 1),
                                             basisLabel,
                                             referenceLabel,
                                             QString::number(referenceValue, 'f', 2),
                                             QString::number(p_snapshot.runtime.currentMetric, 'f', 2));
    m_headroomBar->setToolTip(headroomToolTip);
    m_primaryLabel->setToolTip(headroomToolTip);
    m_secondaryLabel->setToolTip(headroomToolTip);
    m_secondaryLabel->setText(details);
}
