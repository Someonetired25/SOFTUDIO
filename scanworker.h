#ifndef SCANWORKER_H
#define SCANWORKER_H

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QElapsedTimer>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QList>
#include "projectinfo.h"    

class QTimer; // <<< Forward declaration

class ScanWorker : public QObject {
    Q_OBJECT

public:
    explicit ScanWorker(QObject *parent = nullptr);
    ~ScanWorker() override;

public slots:
    void doScan(const QList<QString> &scanRoots, const QString &scanType);
    void stopScan();

private slots: // <<< New private slot
    void _emitPeriodicProgress();

signals:
    void scanProgress(const QString& pathMsg, int totalFoldersEst, int foldersScanned, double elapsedTime, bool isEstimating);
    void projectFound(const ProjectInfo &project);
    void validationRequested(const ProjectInfo &projectToValidate);
    void scanFinished(const QList<ProjectInfo>& allFoundProjectsDuringScan, const QString& outcome, const QVariantMap& extra, const QList<QPair<QString, QString>>& errors);


private:
    void performScan();
    void countTotalFolders();
    void processDirectory(const QString& directoryPath, int currentDepth);
    void handleWalkError(const QString& path, const QString& errorMsg);
    bool checkForSoftudioProject(const QString& dirPath, ProjectInfo& projectInfo);
    void checkForHeuristicProjects(const QString& dirPath, ProjectInfo& projectInfo);


    QList<QString> m_scanRoots;
    QString m_scanType;
    volatile bool m_stopRequested;

    qint64 m_totalFoldersEstimate;
    qint64 m_foldersScannedCount;
    QElapsedTimer m_scanTimer;
    QList<ProjectInfo> m_foundProjectsList;
    QList<QPair<QString, QString>> m_scanErrors;

    QString m_currentScanRootForProgress;
    QString m_lastProcessedPathForPeriodicEmit; // <<< For periodic emit
    int m_currentScanRootIndex;
    int m_totalScanRoots;
    bool m_isCurrentlyEstimatingForPeriodicEmit; // <<< For periodic emit

    QTimer *m_progressUpdateTimer; // <<< Added QTimer

    const QString SCAN_TYPE_QUICK = "Quick Scan (Faster, checks top levels)";
    const QString SCAN_TYPE_DEEP = "Deep Scan (Slower, checks all subfolders)";
    const int QUICK_SCAN_DEPTH_LIMIT = 3;

    const QString SOFTUDIO_FILE_EXTENSION = ".softudio";
    const QString SOFTUDIO_FILE_SIGNATURE = "SOFTUDIO_PROJECT_FILE_V1.0";
    // ... (other SOFTUDIO_NESTED_PATH constants) ...
    const QStringList SOFTUDIO_NESTED_PATH_PARTS = { // Combined for easier use
        "softudio", "engine", "built-in", "core", "project", "packages",
        "assets", "system", "system-binaries", "data", "engine-core-files",
        "genetic-identifier", "project-data"
    };


    const QMap<QString, QString> HEURISTIC_FILES_MAP = { // Using QMap for type association
        {"CMakeLists.txt", "cmake"}, {"package.json", "npm_yarn"}, {".git", "git_repo"},
        {".sln", "vs_solution"}, {".uproject", "unreal"}, {"*.csproj", "csharp_proj"},
        {"Makefile", "make"}, {"pom.xml", "maven"}, {"build.gradle", "gradle"},
        {"setup.py", "python_setup"}
    };
    const QMap<QString, QString> HEURISTIC_DIRS_MAP = { // Using QMap for type association
        {"src", "source_dir"}, {"include", "include_dir"}, {"lib", "library_dir"},
        {"source", "source_dir"}, {"Sources", "source_dir"}, {"Source", "source_dir"},
        {"includes", "include_dir"}, {"headers", "include_dir"}
    };
};

#endif // SCANWORKER_H