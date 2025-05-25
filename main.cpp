#include <QApplication>
#include <QStyleFactory>
#include <QDebug>
#include <QIcon>
#include <QFile>
#include <QMessageBox>
#include <QDir>

// #include "splashscreen.h" // Bypassing splash
#include "splash_constants.h"
#include "scannerdialog.h" // <<< INCLUDE SCANNERDIALOG

#if defined(_WIN32) || defined(_WIN64)
#include <shobjidl.h>
#endif

QString get_application_asset_path(const QString &relative_path) {
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + QDir::separator() + relative_path);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#if defined(_WIN32) || defined(_WIN64)
    if (FAILED(SetCurrentProcessExplicitAppUserModelID(APP_USER_MODEL_ID.toStdWString().c_str()))) {
        qWarning() << "Failed to set AppUserModelID:" << APP_USER_MODEL_ID;
    } else {
        qDebug() << "AppUserModelID set to:" << APP_USER_MODEL_ID;
    }
#endif

    app.setApplicationName(APP_NAME);
    app.setOrganizationName(ORG_NAME);

    if (QStyleFactory::keys().contains("Fusion", Qt::CaseInsensitive)) {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        qDebug() << "Fusion style applied.";
    } else {
        qDebug() << "Fusion style not found. Using default style.";
    }

    QString globalIconPath = get_application_asset_path(APP_ICON_PATH_PRIMARY_REL);
    if (!QFile::exists(globalIconPath)) {
        globalIconPath = get_application_asset_path(APP_ICON_PATH_FALLBACK_REL);
    }
    if (QFile::exists(globalIconPath)) {
        app.setWindowIcon(QIcon(globalIconPath));
        qDebug() << "Global application icon set from:" << globalIconPath;
    } else {
        qWarning() << "Global application icon not found.";
    }

    qDebug() << "Bypassing SplashScreen, launching ScannerDialog directly for testing.";
    ScannerDialog scannerDialog(nullptr);
    scannerDialog.setAttribute(Qt::WA_DeleteOnClose);
    
    int result = scannerDialog.exec();
    qDebug() << "ScannerDialog closed with result:" << result;
    
    return 0;
}