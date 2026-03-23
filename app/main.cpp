#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QSize>
#include <QStringList>
#include <QTimer>

#include "../include/appPaths.h"
#include "../include/homescreen.h"

namespace {
QPixmap loadBaseLogoPixmap()
{
    const QPixmap resourcePixmap(":/images/kmn_app_icon.png");
    if (!resourcePixmap.isNull()) {
        return resourcePixmap;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::current().filePath("resources/kmn_app_icon.png"),
        QDir(appDir).filePath("resources/kmn_app_icon.png"),
        QDir(appDir).filePath("../resources/kmn_app_icon.png"),
        QDir(appDir).filePath("../../resources/kmn_app_icon.png"),
        QDir(appDir).filePath("../../../resources/kmn_app_icon.png"),
        QDir::current().filePath("resources/kmn_logo.PNG"),
        QDir(appDir).filePath("resources/kmn_logo.PNG"),
        QDir(appDir).filePath("../resources/kmn_logo.PNG"),
        QDir(appDir).filePath("../../resources/kmn_logo.PNG"),
        QDir(appDir).filePath("../../../resources/kmn_logo.PNG")
    };

    for (const QString& path : candidates) {
        if (QFileInfo::exists(path)) {
            const QPixmap filePixmap(path);
            if (!filePixmap.isNull()) {
                return filePixmap;
            }
        }
    }

    return QPixmap();
}

QIcon loadAppIcon()
{
    const QPixmap baseLogo = loadBaseLogoPixmap();
    if (baseLogo.isNull()) {
        return QIcon();
    }

    const int squareSide = qMin(baseLogo.width(), baseLogo.height());
    const QRect cropRect((baseLogo.width() - squareSide) / 2,
                         (baseLogo.height() - squareSide) / 2,
                         squareSide,
                         squareSide);
    const QPixmap squareLogo = baseLogo.copy(cropRect);

    QIcon icon;
    const QList<QSize> iconSizes = {
        QSize(16, 16),
        QSize(32, 32),
        QSize(64, 64),
        QSize(128, 128),
        QSize(256, 256),
        QSize(512, 512),
        QSize(1024, 1024)
    };

    for (const QSize& size : iconSizes) {
        QPixmap canvas(size);
        canvas.fill(Qt::transparent);

        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QSize targetSize(size);
        const QPixmap scaledLogo = squareLogo.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const int x = (size.width() - scaledLogo.width()) / 2;
        const int y = (size.height() - scaledLogo.height()) / 2;
        painter.drawPixmap(x, y, scaledLogo);

        icon.addPixmap(canvas);
    }

    return icon;
}

void runStartupSmokeChecks(const homeScreen& window)
{
    QStringList failures;

    if (loadBaseLogoPixmap().isNull()) {
        failures << "No se pudo cargar el logo de la app.";
    }

    if (appDataDirectoryPath().trimmed().isEmpty()) {
        failures << "La ruta de datos de la app esta vacia.";
    }

    if (window.windowTitle().trimmed().isEmpty()) {
        failures << "La ventana principal no tiene titulo.";
    }

    if (!window.isVisible()) {
        failures << "La ventana principal no llego a mostrarse.";
    }

    if (window.findChildren<QListWidget*>().size() < 2) {
        failures << "La home no encontro los widgets basicos de listas.";
    }

    if (failures.isEmpty()) {
        qInfo().noquote() << "[startup-smoke] OK";
        return;
    }

    for (const QString& failure : failures) {
        qWarning().noquote() << "[startup-smoke]" << failure;
    }
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("KMN Space");
    app.setOrganizationDomain("kmnspace.app");
    app.setApplicationName("KMN Space");
    app.setApplicationDisplayName("KMN Space");
    app.setDesktopFileName("KMN Space");
    const QIcon appIcon = loadAppIcon();
    app.setWindowIcon(appIcon);

    homeScreen window;
    window.setWindowTitle("KMN Space");
    window.setWindowIcon(appIcon);
    window.show();

    if (!qEnvironmentVariableIsSet("KMN_SKIP_STARTUP_SMOKE_TESTS")) {
        QTimer::singleShot(0, [&window]() {
            runStartupSmokeChecks(window);
        });
    }

    return app.exec();
}
