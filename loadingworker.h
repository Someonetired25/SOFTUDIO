#ifndef LOADINGWORKER_H
#define LOADINGWORKER_H

#include <QObject>
#include <QString>
#include <QVariantMap> // For loaded_project_data and loaded_images
#include <QList>       // For tasks
#include <QPixmap>     // For loaded_images value type, though QVariantMap handles QVariant

// Forward declaration if ProjectManagerWidget becomes a known C++ type
// class ProjectManagerWidget;

// Define a structure for tasks, similar to the Python tuples
struct TaskDefinition {
    QString userMessage;
    QString detailMessage;
    QString methodName; // Name of the method to call in LoadingWorker
    QString errorContext;
};

class LoadingWorker : public QObject
{
    Q_OBJECT

public:
    explicit LoadingWorker(const QList<TaskDefinition> &tasks, QObject *parent = nullptr);
    ~LoadingWorker() override;

signals:
    void task_started(const QString &user_msg, const QString &detail_msg);
    void progress_updated(int progress_value);

    void loading_complete(const QString &main_window_class_placeholder,
                          const QVariantMap &project_data,
                          const QVariantMap &images);
    void loading_error(const QString &error_context, const QString &error_message);

public slots:
    void run(); // This will be called when the thread starts

private:
    // Task methods corresponding to Python _task_... methods
    bool task_import_projectmanager();
    bool task_load_project_data();
    bool task_load_icons();
    bool task_load_template_images();

    QList<TaskDefinition> m_tasks;
    QString m_projectManagerClassPlaceholder; // To store the "type" of ProjectManagerWidget
    QVariantMap m_loadedProjectData;
    QVariantMap m_loadedImages; // Stores QPixmap by name (QVariant can hold QPixmap)
    QString m_errorMessage;
    QString m_workerBasePath;
};

#endif // LOADINGWORKER_H