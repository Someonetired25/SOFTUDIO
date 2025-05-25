#include "scanworker.h"
#include <QDirIterator>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QCoreApplication>
#include <QThread>
#include <QElapsedTimer> 
#include <QDir>
#include <QFileInfo>
#include <QTimer> // <<< Added for QTimer

ScanWorker::ScanWorker(QObject *parent)
    : QObject(parent),
      m_stopRequested(false),
      m_totalFoldersEstimate(0),
      m_foldersScannedCount(0),
      m_currentScanRootIndex(0),
      m_totalScanRoots(0),
      m_isCurrentlyEstimatingForPeriodicEmit(false)
{
    m_progressUpdateTimer = new QTimer(this);
    connect(m_progressUpdateTimer, &QTimer::timeout, this, &ScanWorker::_emitPeriodicProgress);
    m_progressUpdateTimer->setInterval(750); // Emit progress every 750ms if nothing else triggered
}

ScanWorker::~ScanWorker()
{
    // m_progressUpdateTimer is a child of ScanWorker, will be deleted automatically
}

void ScanWorker::stopScan() {
    m_stopRequested = true;
    if (m_progressUpdateTimer->isActive()) {
        m_progressUpdateTimer->stop();
    }
}

void ScanWorker::_emitPeriodicProgress() {
    if (m_stopRequested) return;

    emit scanProgress(
        m_lastProcessedPathForPeriodicEmit,
        m_scanType == SCAN_TYPE_DEEP ? m_totalFoldersEstimate : 0, // Only provide total estimate for deep scan
        m_foldersScannedCount,
        m_scanTimer.elapsed() / 1000.0,
        m_isCurrentlyEstimatingForPeriodicEmit
    );
}

void ScanWorker::doScan(const QList<QString> &scanRoots, const QString &scanType) {
    m_scanRoots = scanRoots;
    m_scanType = scanType;
    m_stopRequested = false;
    m_totalFoldersEstimate = 0;
    m_foldersScannedCount = 0;
    m_foundProjectsList.clear();
    m_scanErrors.clear();
    m_currentScanRootIndex = 0;
    m_totalScanRoots = m_scanRoots.size();
    m_scanTimer.start();
    m_lastProcessedPathForPeriodicEmit = "Initializing scan...";
    m_isCurrentlyEstimatingForPeriodicEmit = (m_scanType == SCAN_TYPE_DEEP);


    if (!m_scanRoots.isEmpty()) { // Start timer only if there's work to do
        m_progressUpdateTimer->start();
    } else {
        qWarning() << "ScanWorker: No valid scan roots provided.";
        emit scanFinished(m_foundProjectsList, "error", {{"error_message", "No valid scan roots provided."}}, m_scanErrors);
        return;
    }

    performScan();

    if (m_progressUpdateTimer->isActive()) {
        m_progressUpdateTimer->stop();
    }

    QString outcome = m_stopRequested ? "canceled" : "completed";
    QVariantMap extra;
    if(m_stopRequested) {
        QVariantMap stoppedDetails;
        stoppedDetails["time_elapsed_ms"] = m_scanTimer.elapsed();
        extra["stop_details"] = stoppedDetails;
         // Emit one last progress update to reflect cancellation state
        emit scanProgress("Scan canceled.", m_totalFoldersEstimate, m_foldersScannedCount, m_scanTimer.elapsed() / 1000.0, false);
    } else {
        // Emit final progress for completion
        emit scanProgress("Scan complete.", m_totalFoldersEstimate, m_foldersScannedCount, m_scanTimer.elapsed() / 1000.0, false);
    }


    emit scanFinished(m_foundProjectsList, outcome, extra, m_scanErrors);
}

void ScanWorker::countTotalFolders() {
    m_totalFoldersEstimate = 0;
    m_isCurrentlyEstimatingForPeriodicEmit = true;
    m_lastProcessedPathForPeriodicEmit = "Counting folders (Phase 1/2)...";

    for (const QString& rootPath : m_scanRoots) {
        if (m_stopRequested) return;
        
        QDirIterator it(rootPath, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (m_stopRequested) return;
            QString currentPath = it.next();
            m_totalFoldersEstimate++;
            m_lastProcessedPathForPeriodicEmit = currentPath; // Update for timer based emit
            if (m_totalFoldersEstimate % 200 == 0) { 
                emit scanProgress(currentPath, 0, m_totalFoldersEstimate, m_scanTimer.elapsed() / 1000.0, true);
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents); 
            }
        }
    }
    if(!m_stopRequested) {
        m_lastProcessedPathForPeriodicEmit = QString("Counted %L1 folders. Starting scan...").arg(m_totalFoldersEstimate);
        emit scanProgress(m_lastProcessedPathForPeriodicEmit, 0, m_totalFoldersEstimate, m_scanTimer.elapsed() / 1000.0, true);
    }
    m_isCurrentlyEstimatingForPeriodicEmit = false; // Estimation phase finished or skipped
}


void ScanWorker::performScan() {
    if (m_scanType == SCAN_TYPE_DEEP) {
        m_lastProcessedPathForPeriodicEmit = "Phase 1/2: Counting total folders...";
        emit scanProgress(m_lastProcessedPathForPeriodicEmit, 0, 0, m_scanTimer.elapsed() / 1000.0, true);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        countTotalFolders();
        if (m_stopRequested) return;
    } else {
        m_isCurrentlyEstimatingForPeriodicEmit = false; // No estimation for quick scan
    }


    m_foldersScannedCount = 0; // Reset for actual scan phase
    m_lastProcessedPathForPeriodicEmit = "Phase 2/2: Scanning for projects...";
    if (m_scanType == SCAN_TYPE_QUICK) m_lastProcessedPathForPeriodicEmit = "Quick Scan: Scanning for projects...";


    for (m_currentScanRootIndex = 0; m_currentScanRootIndex < m_totalScanRoots; ++m_currentScanRootIndex) {
        if (m_stopRequested) break;
        const QString& rootPath = m_scanRoots.at(m_currentScanRootIndex);
        m_currentScanRootForProgress = rootPath;
        QString scanPhaseMsg = (m_scanType == SCAN_TYPE_DEEP) ? "Phase 2/2: " : "";
        m_lastProcessedPathForPeriodicEmit = QString("%1Scanning in: %2 (%3/%4)")
                                             .arg(scanPhaseMsg)
                                             .arg(QDir(rootPath).dirName())
                                             .arg(m_currentScanRootIndex + 1)
                                             .arg(m_totalScanRoots);

        emit scanProgress(m_lastProcessedPathForPeriodicEmit,
                          m_scanType == SCAN_TYPE_DEEP ? m_totalFoldersEstimate : 0,
                          m_foldersScannedCount,
                          m_scanTimer.elapsed() / 1000.0,
                          false); // Not estimating anymore
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        processDirectory(QDir::toNativeSeparators(rootPath), 0);
    }
}

void ScanWorker::processDirectory(const QString& directoryPath, int currentDepth) {
    if (m_stopRequested) return;

    QDir dir(directoryPath);
    // Skip non-existent or unreadable directories. Links could also be an issue.
    // Using QFileInfo to better handle symlinks and permissions.
    QFileInfo dirInfo(directoryPath);
    if (!dirInfo.exists() || !dirInfo.isDir() || !dirInfo.isReadable()) {
        // Silently skip or log as minor issue if strict POSIX permissions block access often
        // For now, let's log it if it's not just a symlink pointing nowhere critical
        if(dirInfo.exists() && !dirInfo.isReadable()){ // Exists but not readable
             handleWalkError(directoryPath, "Directory not readable.");
        } else if (!dirInfo.exists()){ // Does not exist (e.g. broken symlink)
            // Often fine to ignore broken symlinks
        }
        return;
    }


    m_foldersScannedCount++;
    m_lastProcessedPathForPeriodicEmit = QDir::toNativeSeparators(directoryPath); // Update for timer

    // Emit progress based on count milestone
    if (m_foldersScannedCount % 50 == 0 ) {
         emit scanProgress(m_lastProcessedPathForPeriodicEmit,
                           m_scanType == SCAN_TYPE_DEEP ? m_totalFoldersEstimate : 0,
                           m_foldersScannedCount,
                           m_scanTimer.elapsed() / 1000.0,
                           false); // isEstimating is false here
         QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    ProjectInfo projectInfo(QDir::toNativeSeparators(directoryPath));
    bool isPotentialSoftudio = checkForSoftudioProject(directoryPath, projectInfo);

    if (isPotentialSoftudio) {
        projectInfo.isSoftudioProjectFlag = true; // Mark as potential, validation will confirm
        projectInfo.type = "softudio_potential"; // Intermediate type
        
        bool alreadyFound = false;
        for(const auto& p : m_foundProjectsList) { // Check against m_foundProjectsList
            if(p.path == projectInfo.path) {
                alreadyFound = true;
                break;
            }
        }
        
        if(!alreadyFound) {
            m_foundProjectsList.append(projectInfo);
            emit projectFound(projectInfo); // Let dialog know about raw find
            emit validationRequested(projectInfo); // Request full validation
        }
        // For Softudio projects, we typically don't need to scan subdirs further for other projects
        return; 
    }

    // Heuristic check only if not a Softudio project at this level
    // And only if within depth limits for quick scan
    if (m_scanType == SCAN_TYPE_DEEP || (m_scanType == SCAN_TYPE_QUICK && currentDepth < QUICK_SCAN_DEPTH_LIMIT)) {
        checkForHeuristicProjects(directoryPath, projectInfo); // projectInfo might be updated
        if(projectInfo.heuristicallyFound) {
             bool alreadyFound = false;
             for(const auto& p : m_foundProjectsList) if(p.path == projectInfo.path) alreadyFound = true;
             if(!alreadyFound) {
                m_foundProjectsList.append(projectInfo); // Add heuristic find to master list
                emit projectFound(projectInfo); // Let dialog know (it might ignore if validation is pending for same path)
                // No validationRequested for purely heuristic finds unless you decide to
             }
             // If heuristic project is found, often we don't need to go deeper in this branch either
             // depending on desired behavior (e.g. a .git folder implies the root of that project type)
             // For now, let it continue to find nested Softudio projects if any.
        }
    }

    // Recurse if deep scan or quick scan within depth
    if (m_scanType == SCAN_TYPE_DEEP || (m_scanType == SCAN_TYPE_QUICK && currentDepth < QUICK_SCAN_DEPTH_LIMIT)) {
        QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable | QDir::Hidden | QDir::System, QDir::Name | QDir::DirsFirst);
        for (const QFileInfo &entry : entries) {
            if (m_stopRequested) return;
            // Basic protection against re-entrant symlinks, though QDirIterator is often better.
            // Here, we rely on QFileInfo::isDir() and that QDir doesn't list "." or ".." by default with NoDotAndDotDot.
            if (entry.isDir() && !entry.isSymbolicLink()) { // Avoid following symlinks to dirs for now to prevent loops/massive scans
                processDirectory(entry.absoluteFilePath(), currentDepth + 1);
            }
            // If you want to follow symlinks to directories:
            // else if (entry.isDir()) { // This would include symlinks to dirs
            //    processDirectory(entry.canonicalFilePath(), currentDepth + 1); // canonicalFilePath resolves symlink
            // }
        }
    }
}

bool ScanWorker::checkForSoftudioProject(const QString& dirPath, ProjectInfo& projectInfo) {
    QDir projectDir(dirPath);
    QString originalFolderName = projectDir.dirName();

    // Handle cases like "." or ".." path segments if dirPath is not absolute/cleaned
    if (originalFolderName.isEmpty() || originalFolderName == "." || originalFolderName == "..") {
         QFileInfo fi(dirPath);
         originalFolderName = fi.fileName(); // More reliable for paths like "C:/MyProjects/ProjectA/."
         if (originalFolderName.isEmpty()) originalFolderName = fi.dir().dirName(); // If path ends with slash
    }
    
    QString sanitizedFolderName = originalFolderName;
    sanitizedFolderName.removeIf([](QChar c){ return !c.isLetterOrNumber() && c != '_'; });

    QDir nestedDir(dirPath);
    for(const QString& part : SOFTUDIO_NESTED_PATH_PARTS){ // Use the new list
        if(!nestedDir.cd(part)){
            return false; 
        }
    }
    // After cd'ing through all parts, nestedDir.absolutePath() is the target genetic-identifier/project-data path
    
    if (!nestedDir.exists()) { // Check existence of the final nested path
        return false;
    }

    QString expectedFileName = "." + (sanitizedFolderName.isEmpty() ? "" : sanitizedFolderName) + SOFTUDIO_FILE_EXTENSION;
    QString expectedFilePath = nestedDir.filePath(expectedFileName);

    QFileInfo fileInfo(expectedFilePath);
    if (fileInfo.exists() && fileInfo.isFile() && fileInfo.isReadable()) {
        projectInfo.name = originalFolderName; // Set initial name
        return true; 
    }
    return false;
}

// ... (checkForHeuristicProjects and handleWalkError can remain similar to your existing refined versions, ensure they use HEURISTIC_FILES_MAP and HEURISTIC_DIRS_MAP) ...

// Example of how checkForHeuristicProjects might look with QMap:
void ScanWorker::checkForHeuristicProjects(const QString& dirPath, ProjectInfo& projectInfo) {
    if (projectInfo.heuristicallyFound || projectInfo.isSoftudioProjectFlag) {
        return; // Already identified
    }

    QDir dir(dirPath);
    QString currentDirName = dir.dirName();
    if (currentDirName.isEmpty() || currentDirName == "." || currentDirName == "..") {
        QFileInfo fi(dirPath);
        currentDirName = fi.fileName();
        if(currentDirName.isEmpty()) currentDirName = fi.dir().dirName();
    }


    QMapIterator<QString, QString> fileIter(HEURISTIC_FILES_MAP);
    while (fileIter.hasNext()) {
        if (m_stopRequested) return;
        fileIter.next();
        const QString& heuristicPattern = fileIter.key();
        const QString& heuristicType = fileIter.value();

        if (heuristicPattern.startsWith("*.")) { 
            QStringList nameFilters;
            nameFilters << heuristicPattern;
            if (!dir.entryList(nameFilters, QDir::Files | QDir::Hidden | QDir::System | QDir::Readable).isEmpty()) {
                projectInfo.heuristicallyFound = true;
                projectInfo.type = QString("heuristic_%1").arg(heuristicType);
                projectInfo.name = currentDirName; 
                return; 
            }
        } else { 
            QFileInfo fileInfo(dir.filePath(heuristicPattern));
             if (fileInfo.exists() && (fileInfo.isFile() || (heuristicPattern == ".git" && fileInfo.isDir())) && fileInfo.isReadable() ) {
                projectInfo.heuristicallyFound = true;
                projectInfo.type = QString("heuristic_%1").arg(heuristicType);
                projectInfo.name = currentDirName; 
                return; 
            }
        }
    }

    QMapIterator<QString, QString> dirIter(HEURISTIC_DIRS_MAP);
    while (dirIter.hasNext()) {
        if (m_stopRequested) return;
        dirIter.next();
        const QString& heuristicDirName = dirIter.key();
        const QString& heuristicType = dirIter.value();
        
        QDir subDir(dir.filePath(heuristicDirName));
        if (subDir.exists() && subDir.isReadable()) { // Check readability of subdir as well
            projectInfo.heuristicallyFound = true;
            projectInfo.type = QString("heuristic_%1").arg(heuristicType); // e.g. heuristic_source_dir
            projectInfo.name = currentDirName;
            return; 
        }
    }
}

void ScanWorker::handleWalkError(const QString& path, const QString& errorMsg) {
    // Only add if not already stopped, to avoid flooding errors during cancellation
    if (!m_stopRequested) {
        m_scanErrors.append({QDir::toNativeSeparators(path), errorMsg});
        qDebug() << "ScanWorker Error:" << path << "-" << errorMsg;
    }
}