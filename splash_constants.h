#ifndef SPLASH_CONSTANTS_H
#define SPLASH_CONSTANTS_H

#include <QString>
#include <QColor>
#include <QLinearGradient> // Included for completeness, though gradient setup is complex for a simple const

// --- Asset Paths ---
const QString BACKGROUND_IMAGE_PATH = QStringLiteral(".engine/Graphics/PNG/Core/Splash/splashimage.png");
const QString ICON_PATH_REL = QStringLiteral(".engine/Graphics/PNG/Core/UIElements");
const QString TEMPLATE_IMAGE_PATH_REL = QStringLiteral(".engine/Graphics/PNG/UI/ProjectManagerWindowGUI/Templates");
const QString APP_ICON_PATH_PRIMARY_REL = QStringLiteral(".engine/Graphics/PNG/Core/Logo/Logo_32x32.ico");
const QString APP_ICON_PATH_FALLBACK_REL = QStringLiteral("Logo_32x32.ico");


// --- Text Shine Animation Constants (for AnimatedLoadingLabel) ---
const int SHINE_ANIMATION_DURATION_MS = 1500;
const double SHINE_WIDTH_FRACTION = 0.3;
const QColor BASE_TEXT_COLOR = QColor("#AAAAAA");
const QColor SHINE_TEXT_COLOR = QColor("#FFFFFF");

// --- Exit Button Constants (for ShiningButton - gradient needs dynamic setup) ---
const int EXIT_BUTTON_VISIBLE_DURATION_MS = 5000;
const QColor EXIT_BUTTON_BASE_COLOR = QColor("#CCCCCC");
// The QLinearGradient is an object. It's better to construct it where needed
// or through a static function. For now, just its constituent colors:
const QColor EXIT_BUTTON_SHINE_COLOR_START = QColor("#FBBF24");
const QColor EXIT_BUTTON_SHINE_COLOR_MID = QColor("#FDE047");
const QColor EXIT_BUTTON_SHINE_COLOR_END = QColor("#FBBF24"); // Same as start for the Python example
const int EXIT_BUTTON_SHINE_DURATION_MS = 1000;
const double EXIT_BUTTON_SHINE_WIDTH_FRACTION = 0.5;

// --- Module Names (Meaning might change in C++) ---
// In Python, this was for dynamic import. In C++, it might refer to a library or class name.
const QString PROJECT_MANAGER_MODULE_NAME = QStringLiteral("projectmanager");

// --- Application Specific ---
const QString APP_USER_MODEL_ID = QStringLiteral("SOFTUDIO.ProjectManagerCombined.1.1");
const QString APP_NAME = QStringLiteral("SOFTUDIO");
const QString ORG_NAME = QStringLiteral("NXTLVLTECH");

// --- Splash Screen Behavior ---
const int SPLASH_DURATION_AFTER_LOAD_MS = 500; // As used in your splashscreen.py __main__

// Helper function to construct the exit button gradient, as it's an object
// This could be placed in a utility header or with ShiningButton class.
// For now, just as a note that it's more than a simple const.

inline QLinearGradient createExitButtonShineGradient() {
    QLinearGradient gradient(0, 0, 1, 0);
    gradient.setCoordinateMode(QLinearGradient::ObjectBoundingMode);
    gradient.setColorAt(0.0, EXIT_BUTTON_SHINE_COLOR_START);
    gradient.setColorAt(0.5, EXIT_BUTTON_SHINE_COLOR_MID);
    gradient.setColorAt(1.0, EXIT_BUTTON_SHINE_COLOR_END);
    return gradient;
}

#endif // SPLASH_CONSTANTS_H