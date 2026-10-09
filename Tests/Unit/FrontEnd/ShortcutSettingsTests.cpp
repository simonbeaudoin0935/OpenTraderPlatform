#include <QTest>
#include <QTemporaryDir>
#include <memory>
#include "Misc/ShortcutSettings.h"
#include "Misc/Settings.h"

class ShortcutSettingsTests : public QObject
{
    Q_OBJECT
    QTemporaryDir m_directory;
    std::unique_ptr<QSettings> m_settings;

  private slots:
    void initTestCase()
    {
        QVERIFY(m_directory.isValid());
        m_settings = std::make_unique<QSettings>(m_directory.filePath("state.ini"), QSettings::IniFormat);
        appStateSettings = m_settings.get();
    }

    void mapsOneThroughNine()
    {
        auto& settings = ShortcutSettings::getInstance();
        const QList<ShortcutSettings::ShortcutId> ids{ShortcutSettings::TimeFrame1m,
                                                      ShortcutSettings::TimeFrame5m,
                                                      ShortcutSettings::TimeFrame15m,
                                                      ShortcutSettings::TimeFrame30m,
                                                      ShortcutSettings::TimeFrame1h,
                                                      ShortcutSettings::TimeFrame4h,
                                                      ShortcutSettings::TimeFrame1d,
                                                      ShortcutSettings::TimeFrame1w,
                                                      ShortcutSettings::TimeFrame1M};
        for (int i = 0; i < ids.size(); ++i)
        {
            QCOMPARE(settings.getDefaultShortcut(ids[i]), QKeySequence(QString::number(i + 1)));
            QCOMPARE(settings.getShortcut(ids[i]), QKeySequence(QString::number(i + 1)));
        }
    }

    static QString legacyRemovedShortcutKey()
    {
        return QStringLiteral("Shortcuts/TimeFrame") + QStringLiteral("10") + QStringLiteral("s");
    }

    void migratesSavedLegacyDefaultsAndIsIdempotent()
    {
        QSettings settings(m_directory.filePath("legacy.ini"), QSettings::IniFormat);
        settings.setValue(legacyRemovedShortcutKey(), "1");
        settings.setValue("Shortcuts/TimeFrame1m", "2");
        settings.setValue("Shortcuts/TimeFrame5m", "3");
        settings.setValue("Shortcuts/TimeFrame1M", "0");
        settings.setValue("Shortcuts/QuitApplication", "Ctrl+Q");
        ShortcutSettings::migrateLegacyTimeFrameShortcuts(settings);
        QVERIFY(!settings.contains(legacyRemovedShortcutKey()));
        QVERIFY(!settings.contains("Shortcuts/TimeFrame1m"));
        QVERIFY(!settings.contains("Shortcuts/TimeFrame5m"));
        QVERIFY(!settings.contains("Shortcuts/TimeFrame1M"));
        QCOMPARE(settings.value("Shortcuts/QuitApplication").toString(), QString("Ctrl+Q"));
        settings.setValue("Shortcuts/TimeFrame1m", "1");
        ShortcutSettings::migrateLegacyTimeFrameShortcuts(settings);
        QCOMPARE(settings.value("Shortcuts/TimeFrame1m").toString(), QString("1"));
    }

    void preservesCustomizedLayoutButRemovesLegacyBinding()
    {
        QSettings settings(m_directory.filePath("custom.ini"), QSettings::IniFormat);
        settings.setValue(legacyRemovedShortcutKey(), "Ctrl+1");
        settings.setValue("Shortcuts/TimeFrame1m", "Alt+1");
        settings.setValue("Shortcuts/TimeFrame5m", "3");
        ShortcutSettings::migrateLegacyTimeFrameShortcuts(settings);
        QVERIFY(!settings.contains(legacyRemovedShortcutKey()));
        QCOMPARE(settings.value("Shortcuts/TimeFrame1m").toString(), QString("Alt+1"));
        QCOMPARE(settings.value("Shortcuts/TimeFrame5m").toString(), QString("3"));
    }
};

QTEST_GUILESS_MAIN(ShortcutSettingsTests)
#include "ShortcutSettingsTests.moc"
