#include "ReviewSessionBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>

#include "Core/LedgerPaths.h"
#include "Misc/Logging/Logging.h"

ReviewSessionBar::ReviewSessionBar(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 3, 8, 3);
    layout->setSpacing(5);

    auto* headerLabel = new QLabel("Review", this);
    headerLabel->setStyleSheet("QLabel { color: #66c2ff; font-weight: bold; font-size: 11px; padding-right: 4px; }");
    layout->addWidget(headerLabel);

    m_sessionCombo = new QComboBox(this);
    m_sessionCombo->setMinimumWidth(180);
    m_sessionCombo->setToolTip("Selected replay ledger session");
    layout->addWidget(m_sessionCombo);

    m_refreshButton = new QPushButton("Refresh", this);
    m_refreshButton->setToolTip("Rescan replay ledgers");
    layout->addWidget(m_refreshButton);

    setObjectName("ReviewSessionBar");
    setStyleSheet("QWidget#ReviewSessionBar {"
                  "    background-color: #001722;"
                  "    border: 1.5px solid #2277aa;"
                  "    border-radius: 6px;"
                  "}"
                  "QComboBox {"
                  "    background-color: #1e3340;"
                  "    color: #ffffff;"
                  "    border: 1px solid #336688;"
                  "    border-radius: 3px;"
                  "    padding: 2px 4px;"
                  "}"
                  "QComboBox::drop-down { border: none; }"
                  "QPushButton {"
                  "    background-color: #225577;"
                  "    color: white;"
                  "    font-weight: bold;"
                  "    padding: 4px 10px;"
                  "    border-radius: 4px;"
                  "    border: none;"
                  "}"
                  "QPushButton:hover { background-color: #2c6d99; }");

    connect(m_refreshButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                const QString previous = getSelectedSessionId();
                logInputEvent(u"ReviewSessionBar", u"refresh-sessions");
                scanAndPopulateSessions();
                if (!previous.isEmpty())
                {
                    setSelectedSessionId(previous);
                }
            });

    connect(m_sessionCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](const int index)
            {
                if (index >= 0 && isVisible())
                {
                    logInputEvent(u"ReviewSessionBar",
                                  u"select-session",
                                  {inputDetail(u"sessionId", m_sessionCombo->itemData(index).toString())});
                }
            });

    scanAndPopulateSessions();
}

void ReviewSessionBar::scanAndPopulateSessions()
{
    const QSignalBlocker blocker(m_sessionCombo);
    const QString currentSession = getSelectedSessionId();
    const QStringList sessionIds = LedgerPaths::replayLedgerSessionIds();

    m_sessionCombo->clear();
    for (const QString& sessionId: sessionIds)
    {
        m_sessionCombo->addItem(sessionId, sessionId);
    }

    if (!currentSession.isEmpty())
    {
        setSelectedSessionId(currentSession);
    }
    else if (m_sessionCombo->count() > 0)
    {
        m_sessionCombo->setCurrentIndex(0);
    }
}

QString ReviewSessionBar::getSelectedSessionId() const
{
    const int index = m_sessionCombo->currentIndex();
    if (index < 0)
    {
        return {};
    }
    return m_sessionCombo->itemData(index).toString();
}

void ReviewSessionBar::setSelectedSessionId(const QString& p_sessionId)
{
    const QSignalBlocker blocker(m_sessionCombo);
    for (int index = 0; index < m_sessionCombo->count(); ++index)
    {
        if (m_sessionCombo->itemData(index).toString() == p_sessionId)
        {
            m_sessionCombo->setCurrentIndex(index);
            return;
        }
    }
}
