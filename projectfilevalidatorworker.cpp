#include "projectfilevalidatorworker.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QtConcurrent/QtConcurrentRun> // For QtConcurrent::run
#include <QThreadPool> // Good to include when using QtConcurrent

ProjectFileValidatorWorker::ProjectFileValidatorWorker(QObject *parent)
    : QObject(parent), m_isBusy(false)
{
    // qRegisterMetaType for ValidationResult is done in .h with Q_DECLARE_METATYPE
    connect(&m_validationWatcher, &QFutureWatcher<ValidationResult>::finished,
            this, &ProjectFileValidatorWorker::handleValidationFinished);

    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout,
            this, &ProjectFileValidatorWorker::handleValidationTimeout);
}

ProjectFileValidatorWorker::~ProjectFileValidatorWorker()
{
    if (m_validationWatcher.isRunning()) {
        m_validationWatcher.cancel(); // Request cancellation
        m_validationWatcher.waitForFinished(); // Wait for it to finish
    }
    if (m_timeoutTimer.isActive()) {
        m_timeoutTimer.stop();
    }
}

void ProjectFileValidatorWorker::validateProject(const ProjectInfo &projectToValidate) {
    if (m_isBusy) {
        qWarning() << "ProjectFileValidatorWorker is busy. Skipping validation for:" << projectToValidate.path;
        emit projectValidated(projectToValidate, false, "", "", true, "Validator was busy, request skipped.");
        return;
    }

    m_isBusy = true;
    m_currentProjectInfo = projectToValidate; // Store for timeout case

    // --- THIS IS THE CORRECTED QtConcurrent::run CALL using a LAMBDA ---
    QFuture<ValidationResult> future = QtConcurrent::run([this, projectToValidate]() {
        // 'projectToValidate' is captured by value for use in the thread
        // 'this' is captured to call the member function
        return this->performActualValidation(projectToValidate);
    });
    // --- END OF CORRECTION ---

    m_validationWatcher.setFuture(future);
    m_timeoutTimer.start(VALIDATION_TIMEOUT_MILLISECONDS);
    qDebug() << "ProjectFileValidatorWorker: Started validation for" << projectToValidate.path;
}

ValidationResult ProjectFileValidatorWorker::performActualValidation(ProjectInfo projectToValidate) {
    QString projectRootPath = projectToValidate.path;
    QString validatedNameOut;
    QString validatedUidOut;
    bool isValidOut = false;
    QString errorMessageOut;

    QDir rootDir(projectRootPath);
    if (!rootDir.exists() || !rootDir.isReadable()) {
        errorMessageOut = "Root directory does not exist or is not readable: " + projectRootPath;
        qDebug() << "Validation Error (" << projectRootPath << "):" << errorMessageOut;
        return ValidationResult(projectToValidate, isValidOut, validatedNameOut, validatedUidOut, false, errorMessageOut);
    }

    QString folderName = rootDir.dirName();
    if (folderName.isEmpty() && (projectRootPath.endsWith('/') || projectRootPath.endsWith('\\'))){
         QDir tempDir(projectRootPath);
         if (tempDir.cdUp()) folderName = tempDir.dirName(); 
    }
     if (folderName.isEmpty() || folderName == "." || folderName == "..") { 
        QFileInfo fi(projectRootPath);
        folderName = fi.fileName(); 
        if(folderName.isEmpty() || folderName == "." || folderName == "..") folderName = fi.absoluteDir().dirName();
    }

    QString validFolderNameForFile = folderName;
    validFolderNameForFile.removeIf([](QChar c){ return !c.isLetterOrNumber() && c != '_'; });
    if (validFolderNameForFile.isEmpty() && !folderName.isEmpty()){
         qDebug() << "Validation Info (" << projectRootPath << "): Sanitized folder name became empty. Original:" << folderName;
    }

    QDir nestedDir(projectRootPath); 
    for(const QString& part : SOFTUDIO_NESTED_PATH_PARTS){
        if(!nestedDir.cd(part)){
            errorMessageOut = "Required Softudio nested directory structure part not found: " + part + " within " + nestedDir.path();
            qDebug() << "Validation Error (" << projectRootPath << "):" << errorMessageOut;
            return ValidationResult(projectToValidate, isValidOut, validatedNameOut, validatedUidOut, false, errorMessageOut);
        }
    }
    QString fullNestedDirPathStr = nestedDir.absolutePath(); 

    if (!nestedDir.exists()) { 
        errorMessageOut = "Final required Softudio nested directory does not exist: " + fullNestedDirPathStr;
        qDebug() << "Validation Error (" << projectRootPath << "):" << errorMessageOut;
        return ValidationResult(projectToValidate, isValidOut, validatedNameOut, validatedUidOut, false, errorMessageOut);
    }

    QString expectedFileName = "." + validFolderNameForFile + SOFTUDIO_FILE_EXTENSION;
    QString expectedFilePath = nestedDir.filePath(expectedFileName);

    QFileInfo projectFileInfo(expectedFilePath);
    if (!projectFileInfo.exists() || !projectFileInfo.isFile()) {
        errorMessageOut = "Expected Softudio project file not found: " + QDir::toNativeSeparators(expectedFilePath);
        qDebug() << "Validation Info (" << projectRootPath << "):" << errorMessageOut;
        return ValidationResult(projectToValidate, isValidOut, validatedNameOut, validatedUidOut, false, errorMessageOut);
    }
     if(!projectFileInfo.isReadable()){
        errorMessageOut = "Softudio project file is not readable: " + QDir::toNativeSeparators(expectedFilePath);
        qWarning() << "Validation Error (" << projectRootPath << "):" << errorMessageOut; 
        return ValidationResult(projectToValidate, isValidOut, validatedNameOut, validatedUidOut, false, errorMessageOut);
    }

    QFile projectFile(expectedFilePath);
    if (!projectFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessageOut = "Could not open Softudio project file for reading: " + QDir::toNativeSeparators(expectedFilePath) + " Error: " + projectFile.errorString();
        qWarning() << "Validation Error (" << projectRootPath << "):" << errorMessageOut;
        return ValidationResult(projectToValidate, isValidOut, validatedNameOut, validatedUidOut, false, errorMessageOut);
    }

    QTextStream in(&projectFile);
    QString foundSignature;
    QString foundUid;
    QString foundProjectNameInFile; 
    int lineCount = 0;
    const int maxLinesToRead = 1000; 

    while (!in.atEnd() && lineCount < maxLinesToRead) {
        if (QThread::currentThread()->isInterruptionRequested()) { // Check for cancellation
            errorMessageOut = "Validation interrupted.";
            qDebug() << "Validation for" << projectRootPath << "was interrupted.";
            projectFile.close();
            return ValidationResult(projectToValidate, false, "", "", false, errorMessageOut); // Not a timeout
        }

        QString line = in.readLine().trimmed();
        if (line.startsWith("Signature: ")) {
            foundSignature = line.mid(QString("Signature: ").length());
        } else if (line.startsWith("UID: ")) {
            foundUid = line.mid(QString("UID: ").length());
        } else if (line.startsWith("ProjectName: ")) {
            foundProjectNameInFile = line.mid(QString("ProjectName: ").length());
        }
        lineCount++;
    }
    projectFile.close();

    if (lineCount >= maxLinesToRead && !in.atEnd()) {
        qWarning() << "Validation Info (" << projectRootPath << "): Stopped reading project file" << QDir::toNativeSeparators(expectedFilePath) << "after" << maxLinesToRead << "lines. File might be too large or malformed.";
    }

    if (foundSignature == SOFTUDIO_FILE_SIGNATURE && !foundUid.isEmpty()) {
        isValidOut = true;
        validatedUidOut = foundUid;
        validatedNameOut = foundProjectNameInFile.isEmpty() ? folderName : foundProjectNameInFile;
        qDebug() << "Validation SUCCESS (" << projectRootPath << "): Name:" << validatedNameOut << "UID:" << validatedUidOut;
    } else {
        if (errorMessageOut.isEmpty()) { 
            if (foundSignature != SOFTUDIO_FILE_SIGNATURE) errorMessageOut = "Signature mismatch in project file. Expected: '" + SOFTUDIO_FILE_SIGNATURE + "', Found: '" + foundSignature + "'.";
            else if (foundUid.isEmpty()) errorMessageOut = "UID not found in project file.";
            else errorMessageOut = "Validation failed due to content mismatch (unknown reason).";
        }
        qDebug() << "Validation FAILURE (" << projectRootPath << "):" << errorMessageOut;
    }
    
    return ValidationResult(projectToValidate, isValidOut, validatedNameOut, validatedUidOut, false, errorMessageOut);
}

void ProjectFileValidatorWorker::handleValidationFinished() {
    if (!m_isBusy) { 
        qWarning() << "ProjectFileValidatorWorker: handleValidationFinished called but not busy.";
        return;
    }
    if (m_timeoutTimer.isActive()) { 
        m_timeoutTimer.stop(); 
    }

    ValidationResult result = m_validationWatcher.result(); 

    emit projectValidated(result.originalInfo, result.isValid, result.validatedName, result.validatedUid, result.timedOut, result.errorMessage);
    
    m_isBusy = false; 
    qDebug() << "ProjectFileValidatorWorker: Finished validation for" << result.originalInfo.path << "Valid:" << result.isValid << "TimedOut:" << result.timedOut;
}

void ProjectFileValidatorWorker::handleValidationTimeout() {
    if (!m_isBusy) {
        qWarning() << "ProjectFileValidatorWorker: handleValidationTimeout called but not busy.";
        return; 
    }
    
    if (m_validationWatcher.isRunning()) {
        qDebug() << "ProjectFileValidatorWorker: Validation timed out for" << m_currentProjectInfo.path << ". Attempting to cancel future.";
        m_validationWatcher.cancel();
    }
    
    QString timeoutMessage = "Validation timed out after " + QString::number(VALIDATION_TIMEOUT_MILLISECONDS / 1000) + "s.";
    emit projectValidated(m_currentProjectInfo, false, "", "", true, timeoutMessage); 
    
    m_isBusy = false; 
    qWarning() << "ProjectFileValidatorWorker: Validation TIMED OUT for:" << m_currentProjectInfo.path;
}