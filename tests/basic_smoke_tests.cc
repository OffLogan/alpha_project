#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTest>

#include "../include/appPaths.h"
#include "../include/reminder.h"
#include "../include/task.h"

class BasicSmokeTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void taskLifecycle();
    void reminderDueDateValidation();
    void appPathsResolveExpectedFileNames();
};

void BasicSmokeTests::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("KMN Space");
    QCoreApplication::setOrganizationDomain("kmnspace.app");
    QCoreApplication::setApplicationName("KMN Space");
}

void BasicSmokeTests::taskLifecycle()
{
    Task task("Estudiar", "Repasar algebra");

    QCOMPARE(task.GetTaskId(), 0);
    QCOMPARE(task.GetName(), std::string("Estudiar"));
    QCOMPARE(task.GetDescription(), std::string("Repasar algebra"));
    QVERIFY(!task.GetStatus());

    QVERIFY(!task.SetId(0));
    QVERIFY(task.SetId(7));
    task.CompleteTask();

    QCOMPARE(task.GetTaskId(), 7);
    QVERIFY(task.GetStatus());
}

void BasicSmokeTests::reminderDueDateValidation()
{
    Reminder reminder("Entrega", "Subir proyecto", "19/03/26");

    QCOMPARE(reminder.GetDue(), std::string("19/03/26"));
    QVERIFY(reminder.SetDue("20/03/26"));
    QCOMPARE(reminder.GetDue(), std::string("20/03/26"));
    QVERIFY(!reminder.SetDue("2026-03-20"));
    QCOMPARE(reminder.GetDue(), std::string("20/03/26"));
}

void BasicSmokeTests::appPathsResolveExpectedFileNames()
{
    const QString appDataPath = appDataDirectoryPath();
    QVERIFY(!appDataPath.trimmed().isEmpty());

    const QString writableTasksPath = writableDataFilePath("data/tasks.json");
    QCOMPARE(QFileInfo(writableTasksPath).fileName(), QString("tasks.json"));
    QVERIFY(writableTasksPath.startsWith(appDataPath));

    const QString fallbackNotesPath = readableDataFilePath("data/notes.json");
    QCOMPARE(QFileInfo(fallbackNotesPath).fileName(), QString("notes.json"));
}

QTEST_MAIN(BasicSmokeTests)

#include "basic_smoke_tests.moc"
