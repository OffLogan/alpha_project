#include "../include/appPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

namespace {
QString normalizedFileName(const QString& configuredPath)
{
    const QFileInfo fileInfo(configuredPath);
    const QString fileName = fileInfo.fileName().trimmed();
    return fileName.isEmpty() ? QString("data.json") : fileName;
}

QString legacyDataFilePath(const QString& configuredPath)
{
    const QFileInfo directInfo(configuredPath);
    if (directInfo.exists() || directInfo.isAbsolute()) {
        return directInfo.filePath();
    }

    const QString applicationPath = QCoreApplication::applicationDirPath();
    QStringList candidatePaths;

    auto appendProjectCandidates = [&](const QString& startPath) {
        QDir dir(startPath);
        while (dir.exists() && !dir.isRoot()) {
            if (QFileInfo::exists(dir.filePath("CMakeLists.txt"))) {
                candidatePaths.append(dir.filePath(configuredPath));
                break;
            }
            dir.cdUp();
        }
    };

    appendProjectCandidates(QDir::currentPath());
    appendProjectCandidates(applicationPath);

    candidatePaths.append(QDir::cleanPath(applicationPath + "/../" + configuredPath));
    candidatePaths.append(QDir::cleanPath(applicationPath + "/../../" + configuredPath));
    candidatePaths.append(QDir::cleanPath(applicationPath + "/../../../" + configuredPath));
    candidatePaths.append(QDir::cleanPath(QDir::current().absoluteFilePath(configuredPath)));

    for (const QString& candidate : candidatePaths) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return QString();
}
}

QString appDataDirectoryPath()
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.trimmed().isEmpty()) {
        appDataPath = QDir::home().filePath(".kmn-space");
    }

    return appDataPath;
}

QString writableDataFilePath(const std::string& configuredPath)
{
    const QString fileName = normalizedFileName(QString::fromStdString(configuredPath));
    return QDir(appDataDirectoryPath()).filePath(fileName);
}

QString readableDataFilePath(const std::string& configuredPath)
{
    const QString preferredPath = writableDataFilePath(configuredPath);
    if (QFileInfo::exists(preferredPath)) {
        return preferredPath;
    }

    const QString legacyPath = legacyDataFilePath(QString::fromStdString(configuredPath));
    if (!legacyPath.isEmpty()) {
        return legacyPath;
    }

    return preferredPath;
}

QString legacyReadableDataFilePath(const std::string& configuredPath)
{
    return legacyDataFilePath(QString::fromStdString(configuredPath));
}
