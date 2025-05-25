#include "loadingworker.h"
#include "splash_constants.h" // For PROJECT_MANAGER_MODULE_NAME, ICON_PATH_REL, etc.

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>       // For qDebug() messages
#include <QThread>      // For QThread::msleep, analogous to time.sleep

QVariantMap load_projects_cpp_equivalent(QString& errorMsg) {

    qDebug() << "[LoadingWorker - Placeholder] _task_load_project_data: Simulating project load.";
    QString projectsFilePath = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/projects.json");
    QFile projectsFile(projectsFilePath);

    if (!projectsFile.exists()) {
        errorMsg = QString("'%1' not found.").arg(QFileInfo(projectsFile).fileName());
        qDebug() << "[LoadingWorker] INFO:" << errorMsg << "- Starting with empty project lists.";
        QVariantMap data;
        data["projects"] = QVariantList();
        data["recent"] = QVariantList();
        data["visited"] = QVariantMap(); // Assuming visited is like {"path": timestamp}
        data["pinned"] = QVariantList();
        return data; // Return empty but valid structure
    }

    if (!projectsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = QString("Could not open '%1' for reading.").arg(QFileInfo(projectsFile).fileName());
        qDebug() << "[LoadingWorker] ERROR:" << errorMsg;
        // Return empty on error to mimic Python's behavior of proceeding with empty lists
        QVariantMap data;
        data["projects"] = QVariantList();
        data["recent"] = QVariantList();
        data["visited"] = QVariantMap();
        data["pinned"] = QVariantList();
        return data;
    }

    QByteArray jsonData = projectsFile.readAll();
    projectsFile.close();
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);

    if (doc.isNull() || !doc.isObject()) {
        errorMsg = QString("Failed to parse '%1' or it's not a JSON object.").arg(QFileInfo(projectsFile).fileName());
        qDebug() << "[LoadingWorker] ERROR:" << errorMsg;
        QVariantMap data; // Return empty
        data["projects"] = QVariantList(); data["recent"] = QVariantList(); data["visited"] = QVariantMap(); data["pinned"] = QVariantList();
        return data;
    }
    // Assuming projects.json has keys: "projects", "recent_projects", "most_visited_folders", "pinned_folders"
    // And their values are suitable for direct QVariant conversion (e.g. lists of strings/objects, map of string to string/number)
    QJsonObject rootObj = doc.object();
    QVariantMap loadedData;
    loadedData["projects"] = rootObj.value("projects").toVariant(); // Example
    loadedData["recent"]   = rootObj.value("recent_projects").toVariant();
    loadedData["visited"]  = rootObj.value("most_visited_folders").toVariant();
    loadedData["pinned"]   = rootObj.value("pinned_folders").toVariant();

    return loadedData;
}
// --- End Placeholder ---


LoadingWorker::LoadingWorker(const QList<TaskDefinition> &tasks, QObject *parent)
    : QObject(parent), m_tasks(tasks)
{
    // Determine base path - equivalent to Python's sys._MEIPASS or os.path.dirname
    // For a deployed application, assets are often relative to the executable.
    m_workerBasePath = QCoreApplication::applicationDirPath();
    qDebug() << "[LoadingWorker] Base path for assets (worker):" << m_workerBasePath;
}

LoadingWorker::~LoadingWorker()
{
    qDebug() << "[LoadingWorker] Destroyed.";
}

void LoadingWorker::run()
{
    qDebug() << "[LoadingWorker] Run started on thread:" << QThread::currentThreadId();
    int totalSteps = 0;
    for (const auto &task : m_tasks) {
        if (!task.methodName.isEmpty()) {
            totalSteps++;
        }
    }

    int currentStep = 0;
    for (const auto &task : m_tasks) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            m_errorMessage = "Loading was cancelled by user.";
            emit loading_error("Cancellation", m_errorMessage);
            return;
        }

        if (!task.methodName.isEmpty()) {
            emit task_started(task.userMessage, task.detailMessage);
            bool success = false;
            try {
                if (task.methodName == "_task_import_projectmanager") {
                    success = task_import_projectmanager();
                } else if (task.methodName == "_task_load_project_data") {
                    success = task_load_project_data();
                } else if (task.methodName == "_task_load_icons") {
                    success = task_load_icons();
                } else if (task.methodName == "_task_load_template_images") {
                    success = task_load_template_images();
                } else {
                    m_errorMessage = QString("Unknown task method: %1").arg(task.methodName);
                    success = false;
                }

                if (!success) {
                    // m_errorMessage should have been set by the task method
                    emit loading_error(task.errorContext, m_errorMessage);
                    return;
                }
            } catch (const std::exception &e) {
                m_errorMessage = QString("Unexpected C++ exception during task '%1': %2")
                                     .arg(task.methodName, QString::fromStdString(e.what()));
                qCritical() << "[LoadingWorker] CRITICAL ERROR:" << m_errorMessage;
                emit loading_error(task.errorContext, m_errorMessage);
                return;
            } catch (...) {
                m_errorMessage = QString("Unknown C++ exception during task '%1'.").arg(task.methodName);
                qCritical() << "[LoadingWorker] CRITICAL ERROR:" << m_errorMessage;
                emit loading_error(task.errorContext, m_errorMessage);
                return;
            }
            currentStep++;
            emit progress_updated(currentStep);
        } else if (task.userMessage == "Finalizing...") { // Handle special tasks from Python list
            emit task_started(task.userMessage, task.detailMessage);
            QThread::msleep(200); // Analogous to time.sleep(0.2)
        }
    }
    emit loading_complete(m_projectManagerClassPlaceholder, m_loadedProjectData, m_loadedImages);
    qDebug() << "[LoadingWorker] Run finished.";
}

bool LoadingWorker::task_import_projectmanager()
{
    qDebug() << "[LoadingWorker] Executing task: Import ProjectManager";
    // Placeholder: Let's say the C++ class will be named "ProjectManagerWidgetCpp"
    m_projectManagerClassPlaceholder = "ProjectManagerWidgetCpp"; // This string is a placeholder.
                                                                 // The main thread will use this info
                                                                 // to decide which main window to create.

    // Simulate check or light initialization
    if (PROJECT_MANAGER_MODULE_NAME.isEmpty()) { // Using constant for consistency
        m_errorMessage = "Project manager module name (constant) is not defined.";
        return false;
    }

    qDebug() << "[LoadingWorker] Placeholder for ProjectManagerWidget C++ class:" << m_projectManagerClassPlaceholder;
    return true;
}

bool LoadingWorker::task_load_project_data()
{
    qDebug() << "[LoadingWorker] Executing task: Load Project Data";
    if (m_projectManagerClassPlaceholder.isEmpty()) { // Check if previous step "succeeded"
        m_errorMessage = "Project manager module not 'loaded' (placeholder not set), cannot load project data.";
        return false;
    }
    // This would call the C++ equivalent of projectmanager.load_projects()
    m_loadedProjectData = load_projects_cpp_equivalent(m_errorMessage); // Pass m_errorMessage by reference

    if (!m_errorMessage.isEmpty() && m_loadedProjectData.isEmpty()){ // Check if a real error prevented even default data
        return false; 
    }
    m_errorMessage.clear(); // Clear if successfully loaded or defaulted.
    qDebug() << "[LoadingWorker] Project data loaded/defaulted. Count:" << m_loadedProjectData.value("projects").toList().count();
    return true;
}

bool LoadingWorker::task_load_icons()
{
    qDebug() << "[LoadingWorker] Executing task: Load Icons";
    QMap<QString, QString> iconsToLoad;
    iconsToLoad["star_icon"] = QDir::cleanPath(m_workerBasePath + "/" + ICON_PATH_REL + "/star.png");
    iconsToLoad["star_outline_icon"] = QDir::cleanPath(m_workerBasePath + "/" + ICON_PATH_REL + "/star_outline.png");

    for (auto it = iconsToLoad.constBegin(); it != iconsToLoad.constEnd(); ++it) {
        const QString &name = it.key();
        const QString &path = it.value();
        if (QFile::exists(path)) {
            QPixmap pixmap(path);
            if (!pixmap.isNull()) {
                m_loadedImages[name] = QVariant::fromValue(pixmap);
            } else {
                qWarning() << "[LoadingWorker] Warning: Failed to load icon (Pixmap isNull):" << path;
            }
        } else {
            qWarning() << "[LoadingWorker] Warning: Icon not found:" << path;
        }
    }
    // Python version always returns True for this task
    return true;
}

bool LoadingWorker::task_load_template_images()
{
    qDebug() << "[LoadingWorker] Executing task: Load Template Images";
    QStringList templateImagesPaths;
    templateImagesPaths << QDir::cleanPath(m_workerBasePath + "/" + TEMPLATE_IMAGE_PATH_REL + "/BlankTemplate.jpg");
    templateImagesPaths << QDir::cleanPath(m_workerBasePath + "/" + TEMPLATE_IMAGE_PATH_REL + "/UIExample.jpg");
    templateImagesPaths << QDir::cleanPath(m_workerBasePath + "/" + TEMPLATE_IMAGE_PATH_REL + "/WebappExample.jpg");
    templateImagesPaths << QDir::cleanPath(m_workerBasePath + "/" + TEMPLATE_IMAGE_PATH_REL + "/BuildExample.jpg");

    for (int i = 0; i < templateImagesPaths.size(); ++i) {
        const QString &path = templateImagesPaths.at(i);
        if (QFile::exists(path)) {
            QPixmap pixmap(path);
            if (!pixmap.isNull()) {
                m_loadedImages[QString("template_%1").arg(i)] = QVariant::fromValue(pixmap);
            } else {
                qWarning() << "[LoadingWorker] Warning: Failed to load template image (Pixmap isNull):" << path;
            }
        } else {
            qWarning() << "[LoadingWorker] Warning: Template image not found:" << path;
        }
    }
    // Python version always returns True for this task
    return true;
}