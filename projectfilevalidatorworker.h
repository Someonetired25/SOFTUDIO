#ifndef PROJECTFILEVALIDATORWORKER_H
#define PROJECTFILEVALIDATORWORKER_H

#include <QObject>
#include <QString>
#include <QMetaType>
#include <QFutureWatcher> // For QtConcurrent
#include <QTimer>
#include "projectinfo.h" // Ensure this is included and defines ProjectInfo struct

// Define ValidationResult struct if not already globally available
// It's good practice to have such structs in a shared header or within the class that uses them.
struct ValidationResult {
    ProjectInfo originalInfo;
    bool isValid = false;
    QString validatedName;
    QString validatedUid;
    bool timedOut = false;
    QString errorMessage;

    ValidationResult() = default; // Default constructor

    // Constructor for convenience
    ValidationResult(ProjectInfo info,
                     bool valid,
                     QString name,
                     QString uid,
                     bool timeout,
                     QString error)
        : originalInfo(std::move(info)), // Use std::move for efficiency if applicable
          isValid(valid),
          validatedName(std::move(name)),
          validatedUid(std::move(uid)),
          timedOut(timeout),
          errorMessage(std::move(error)) {}
};
Q_DECLARE_METATYPE(ValidationResult); // Required for QueuedConnection with this custom type

class ProjectFileValidatorWorker : public QObject {
    Q_OBJECT

public:
    explicit ProjectFileValidatorWorker(QObject *parent = nullptr);
    ~ProjectFileValidatorWorker() override;

public slots:
    void validateProject(const ProjectInfo &projectToValidate);

private slots:
    void handleValidationFinished();
    void handleValidationTimeout();

signals:
    void projectValidated(const ProjectInfo& originalInfo,
                          bool isValid,
                          const QString& validatedName,
                          const QString& validatedUid,
                          bool timedOut, 
                          const QString& errorMessage); 

private:
    // This method will run in a separate thread via QtConcurrent
    ValidationResult performActualValidation(ProjectInfo projectToValidate);

    QFutureWatcher<ValidationResult> m_validationWatcher;
    QTimer m_timeoutTimer; // For managing validation timeout
    ProjectInfo m_currentProjectInfo; // Store info of project being validated
    bool m_isBusy; // To prevent concurrent validation requests on the same worker instance

    // Constants for validation (mirroring scanner.py and scanworker.cpp)
    const QString SOFTUDIO_FILE_EXTENSION = ".softudio";
    const QString SOFTUDIO_FILE_SIGNATURE = "SOFTUDIO_PROJECT_FILE_V1.0";
    const QStringList SOFTUDIO_NESTED_PATH_PARTS = {
        "softudio", "engine", "built-in", "core", "project", "packages",
        "assets", "system", "system-binaries", "data", "engine-core-files",
        "genetic-identifier", "project-data"
    };

    const int VALIDATION_TIMEOUT_MILLISECONDS = 15000; // 15 seconds
};

#endif // PROJECTFILEVALIDATORWORKER_H