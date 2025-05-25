#ifndef SCANNERDIALOG_H
#define SCANNERDIALOG_H

#include "framelessdialogbase.h"
#include "projectinfo.h"
#include "animatedloadinglabel.h" // From SOFTUDIO project
#include <QThread>
#include <QList>
#include <QVariantMap>
#include <QListWidgetItem>
#include <QSet> // <<< Added for known UIDs

class QLineEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QCheckBox;
class QRadioButton;
class QGroupBox;
class QTableWidget;
class QDialogButtonBox;
class QSettings;
class QStackedWidget;
class QListWidget;
class QMovie;

class ScanWorker;
class ProjectFileValidatorWorker;


class ScannerDialog : public FramelessDialogBase {
    Q_OBJECT

public:
    explicit ScannerDialog(QWidget *parent = nullptr);
    ~ScannerDialog() override;

    // <<< NEW PUBLIC METHOD for setting known UIDs
    void setKnownProjectUids(const QSet<QString>& knownUids);

signals:
    void projectsSelectedForImport(const QList<ProjectInfo> &selectedProjects);

    void requestScanWorkerStart(const QList<QString> &scanRoots, const QString &scanType);
    void requestScanWorkerStop();
    void requestValidateProject(const ProjectInfo& projectToValidate);


protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void browseDirectory();
    void onConfigNextClicked();

    void onInitialPromptScanNow();
    void onInitialPromptLater();

    void onScanTypeChanged();
    void onScanScopeChanged();
    void onDrivesListItemChanged(QListWidgetItem* item);


    void startActualScan();
    void cancelScanRequestedByProgressPage();
    void onScanWorkerFinished(const QList<ProjectInfo>& allFoundProjects, const QString& outcome, const QVariantMap& extra, const QList<QPair<QString, QString>>& errors);
    void updateScanProgressUI(const QString& pathMsg, int totalFoldersEst, int foldersScanned, double elapsedTime, bool isEstimating);
    void addFoundProjectToInternalList(const ProjectInfo& project);
    void onProjectFileValidated(const ProjectInfo& originalInfo, bool isValid, const QString& validatedName, const QString& validatedUid, bool timedOut, const QString& errorMessage);
    void onLogDialogNextClicked();
    void exportScanLog();

    void onResultsSelectionChanged();
    void acceptProjectSelection();
    void selectAllResults(bool select);

private:
    void setupUi();
    void setupInitialPromptPage();
    void setupConfigPage();
    void setupProgressPage();
    void setupLogPage();
    void setupResultsPage();

    void loadSettings();
    void saveSettings();

    void startScanThreads();
    void stopScanThreadsAndCleanup();

    void showPage(int index);
    void populateDrivesList(); // <<< Will be enhanced
    QStringList getAvailableScanLocations(); // <<< NEW helper for advanced drive detection
    QStringList getSelectedScanPaths();
    QString getSelectedScanType();

    void updateProgressETA(double elapsedTimeSec, int itemsProcessed, int itemsTotal, bool isEstimatingPhase);
    void setProgressAnimation(const QString& stateKey);

    void populateLogTable(const QList<QPair<QString, QString>>& errors);
    void populateResultsTable();
    void updateResultsOkButtonState();


    QStackedWidget *m_stackedWidget;

    QWidget *m_initialPromptPage;
    QCheckBox *m_dontShowPromptAgainCheckBox;

    QWidget *m_configPage;
    QRadioButton *m_quickScanRadio;
    QRadioButton *m_deepScanRadio;
    QRadioButton *m_fullDiskRadio;
    QRadioButton *m_selectDrivesRadio;
    QRadioButton *m_selectFolderRadio;
    QListWidget *m_drivesListWidget;
    QLineEdit *m_folderPathEdit;
    QPushButton *m_browseFolderButton;
    QWidget *m_folderSelectWidget;
    QWidget *m_drivesListContainerWidget;


    QWidget *m_progressPage;
    AnimatedLoadingLabel *m_progressStatusLabel;
    QLabel *m_progressCurrentPathLabel;
    QProgressBar *m_progressBar;
    QLabel *m_progressTimeEtcLabel;
    QLabel *m_progressAnimationLabel;
    QPushButton *m_progressCancelButton;
    QMap<QString, QMovie*> m_progressMovies;


    QWidget *m_logPage;
    QTableWidget *m_logTableWidget;
    QPushButton *m_exportLogButton;


    QWidget *m_resultsPage;
    QTableWidget *m_resultsTableWidget;
    QPushButton *m_resultsSelectAllButton;
    QPushButton *m_resultsDeselectAllButton;
    QDialogButtonBox *m_resultsButtonBox;


    QSettings *m_settings;

    QThread m_scanWorkerThread;
    ScanWorker *m_scanWorker;

    QThread m_validatorThread;
    ProjectFileValidatorWorker *m_validatorWorker;

    QList<ProjectInfo> m_allFoundProjectsInternalList;
    QList<QPair<QString, QString>> m_currentScanErrors;
    QList<ProjectInfo> m_validatedProjectsForResultsTable;
    QSet<QString> m_knownProjectUids; // <<< Added member for known UIDs

    bool m_scanInProgress;
    bool m_scanCancelled;
    qint64 m_scanStartTime;


    const QString SOFTUDIO_SETTINGS_GROUP = "ProjectScanner";
    const QString SETTING_DONT_SHOW_PROMPT_V2 = "dontShowInitialPromptV2";
    const QString SETTING_LAST_SCAN_PATH = "LastScannedPath";
    const QString SETTING_LAST_SCAN_TYPE = "LastScanType";
    const QString SETTING_LAST_SCAN_SCOPE = "LastScanScope";
    const QString SETTING_LAST_SELECTED_DRIVES = "LastSelectedDrives";

    const QString SCAN_TYPE_QUICK = "Quick Scan (Faster, checks top levels)";
    const QString SCAN_TYPE_DEEP = "Deep Scan (Slower, checks all subfolders)";
    const QString SCAN_SCOPE_FULL_DISK = "Scan Full Computer";
    const QString SCAN_SCOPE_DRIVES = "Select Drives/Partitions";
    const QString SCAN_SCOPE_FOLDER = "Select Specific Folder";

    enum Page {
        InitialPrompt = 0,
        Configuration = 1,
        Progress = 2,
        Log = 3,
        Results = 4
    };
};

#endif // SCANNERDIALOG_H