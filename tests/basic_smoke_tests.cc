#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTest>

#include "../include/appPaths.h"
#include "../include/reminder.h"
#include "../include/reminderGestor.h"
#include "../include/settings.h"
#include "../include/task.h"
#include "../include/taskGestor.h"

class BasicSmokeTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void taskLifecycle();
    void reminderDueDateValidation();
    void appPathsResolveExpectedFileNames();
    void settingsDefaultsAndPersistence();
    void taskManagerCreatesTasks();
    void taskManagerEditsTasks();
    void taskManagerRemovesTasks();
    void reminderManagerCreatesReminders();
    void reminderManagerEditsReminders();
    void reminderManagerRemovesReminders();
    void reminderCanStoreNormalizedFutureDate();
};

namespace {
QString testDataPath(const QString& fileName)
{
    return writableDataFilePath(QString("data/%1").arg(fileName).toStdString());
}

void removeTestDataFile(const QString& fileName)
{
    QFile::remove(testDataPath(fileName));
}
}

void BasicSmokeTests::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("KMN Space");
    QCoreApplication::setOrganizationDomain("kmnspace.app");
    QCoreApplication::setApplicationName("KMN Space");
    QSettings().clear();
}

void BasicSmokeTests::cleanup()
{
    removeTestDataFile("test_tasks.json");
    removeTestDataFile("test_reminders.json");
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

void BasicSmokeTests::settingsDefaultsAndPersistence()
{
    const AppSettings defaults = settings::loadAppSettings();
    QVERIFY(defaults.notificationsEnabled);
    QVERIFY(!defaults.darkModeEnabled);

    QSettings preferences;
    preferences.setValue("preferences/notifications_enabled", false);
    preferences.setValue("preferences/dark_mode_enabled", false);
    preferences.sync();

    const AppSettings stored = settings::loadAppSettings();
    QVERIFY(!stored.notificationsEnabled);
    QVERIFY(!stored.darkModeEnabled);
}

void BasicSmokeTests::taskManagerCreatesTasks()
{
    TaskData taskData("data/test_tasks.json");
    QVERIFY(taskData.Load());

    Task task("Estudiar", "Repasar tema 1");
    QVERIFY(task.SetId(taskData.NextId()));
    QVERIFY(taskData.AddTask(task));

    const std::vector<Task> storedTasks = taskData.Data();
    QCOMPARE(storedTasks.size(), std::size_t(1));
    QCOMPARE(storedTasks.front().GetName(), std::string("Estudiar"));
    QVERIFY(QFileInfo::exists(testDataPath("test_tasks.json")));

    TaskData reloadedTaskData("data/test_tasks.json");
    QVERIFY(reloadedTaskData.Load());
    QCOMPARE(reloadedTaskData.Data().size(), std::size_t(1));
    QCOMPARE(reloadedTaskData.Data().front().GetDescription(), std::string("Repasar tema 1"));
}

void BasicSmokeTests::taskManagerEditsTasks()
{
    TaskData taskData("data/test_tasks.json");
    QVERIFY(taskData.Load());

    Task task("Estudiar", "Repasar tema 1");
    QVERIFY(task.SetId(taskData.NextId()));
    QVERIFY(taskData.AddTask(task));

    Task* storedTask = taskData.FindById(task.GetTaskId());
    QVERIFY(storedTask != nullptr);
    QVERIFY(storedTask->SetName("Estudiar mas"));
    QVERIFY(storedTask->SetDescription(""));
    storedTask->CompleteTask();
    QVERIFY(taskData.UpdateTask(*storedTask));
    QVERIFY(taskData.FindById(task.GetTaskId())->GetStatus());
    taskData.FindById(task.GetTaskId())->ReopenTask();
    QVERIFY(taskData.UpdateTask(*taskData.FindById(task.GetTaskId())));
    QVERIFY(!taskData.FindById(task.GetTaskId())->GetStatus());
    QCOMPARE(taskData.FindById(task.GetTaskId())->GetName(), std::string("Estudiar mas"));
    QCOMPARE(taskData.FindById(task.GetTaskId())->GetDescription(), std::string(""));
}

void BasicSmokeTests::taskManagerRemovesTasks()
{
    TaskData taskData("data/test_tasks.json");
    QVERIFY(taskData.Load());

    Task firstTask("Estudiar", "Repasar tema 1");
    QVERIFY(firstTask.SetId(taskData.NextId()));
    QVERIFY(taskData.AddTask(firstTask));

    Task secondTask("Practicar", "Resolver ejercicios");
    QVERIFY(secondTask.SetId(taskData.NextId()));
    QVERIFY(taskData.AddTask(secondTask));

    QVERIFY(taskData.RemoveTask(firstTask.GetTaskId()));
    QVERIFY(taskData.FindById(firstTask.GetTaskId()) == nullptr);
    QVERIFY(taskData.FindById(secondTask.GetTaskId()) != nullptr);
    QCOMPARE(taskData.Data().size(), std::size_t(1));
}

void BasicSmokeTests::reminderManagerCreatesReminders()
{
    ReminderData reminderData("data/test_reminders.json");
    QVERIFY(reminderData.Load());

    Reminder reminder("Entrega", "Subir memoria", "21/03/26");
    QVERIFY(reminder.SetId(reminderData.NextId()));
    QVERIFY(reminderData.AddReminder(reminder));

    const std::vector<Reminder> storedReminders = reminderData.Data();
    QCOMPARE(storedReminders.size(), std::size_t(1));
    QCOMPARE(storedReminders.front().GetDue(), std::string("21/03/26"));
    QVERIFY(QFileInfo::exists(testDataPath("test_reminders.json")));

    ReminderData reloadedReminderData("data/test_reminders.json");
    QVERIFY(reloadedReminderData.Load());
    QCOMPARE(reloadedReminderData.Data().size(), std::size_t(1));
    QCOMPARE(reloadedReminderData.Data().front().GetName(), std::string("Entrega"));
}

void BasicSmokeTests::reminderManagerEditsReminders()
{
    ReminderData reminderData("data/test_reminders.json");
    QVERIFY(reminderData.Load());

    Reminder reminder("Entrega", "Subir memoria", "21/03/26");
    QVERIFY(reminder.SetId(reminderData.NextId()));
    QVERIFY(reminderData.AddReminder(reminder));

    Reminder* storedReminder = reminderData.FindById(reminder.GetTaskId());
    QVERIFY(storedReminder != nullptr);
    QVERIFY(storedReminder->SetName("Entrega final"));
    QVERIFY(storedReminder->SetDescription(""));
    QVERIFY(storedReminder->SetDue("22/03/26"));
    storedReminder->CompleteTask();
    QVERIFY(reminderData.UpdateReminder(*storedReminder));
    QVERIFY(reminderData.FindById(reminder.GetTaskId())->GetStatus());
    reminderData.FindById(reminder.GetTaskId())->ReopenTask();
    QVERIFY(reminderData.UpdateReminder(*reminderData.FindById(reminder.GetTaskId())));
    QVERIFY(!reminderData.FindById(reminder.GetTaskId())->GetStatus());
    QCOMPARE(reminderData.FindById(reminder.GetTaskId())->GetName(), std::string("Entrega final"));
    QCOMPARE(reminderData.FindById(reminder.GetTaskId())->GetDue(), std::string("22/03/26"));
}

void BasicSmokeTests::reminderManagerRemovesReminders()
{
    ReminderData reminderData("data/test_reminders.json");
    QVERIFY(reminderData.Load());

    Reminder firstReminder("Entrega", "Subir memoria", "21/03/26");
    QVERIFY(firstReminder.SetId(reminderData.NextId()));
    QVERIFY(reminderData.AddReminder(firstReminder));

    Reminder secondReminder("Examen", "Preparar temas", "22/03/26");
    QVERIFY(secondReminder.SetId(reminderData.NextId()));
    QVERIFY(reminderData.AddReminder(secondReminder));

    QVERIFY(reminderData.RemoveReminder(firstReminder.GetTaskId()));
    QVERIFY(reminderData.FindById(firstReminder.GetTaskId()) == nullptr);
    QVERIFY(reminderData.FindById(secondReminder.GetTaskId()) != nullptr);
    QCOMPARE(reminderData.Data().size(), std::size_t(1));
}

void BasicSmokeTests::reminderCanStoreNormalizedFutureDate()
{
    Reminder reminder("Examen", "Repasar unidad", "29/03/26");
    QVERIFY(reminder.SetDue("30/03/26"));
    QCOMPARE(reminder.GetDue(), std::string("30/03/26"));
}

QTEST_MAIN(BasicSmokeTests)

#include "basic_smoke_tests.moc"
