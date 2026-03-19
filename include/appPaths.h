#ifndef APPPATHS_H
#define APPPATHS_H

#include <QString>
#include <string>

QString appDataDirectoryPath();
QString writableDataFilePath(const std::string& configuredPath);
QString readableDataFilePath(const std::string& configuredPath);
QString legacyReadableDataFilePath(const std::string& configuredPath);

#endif
