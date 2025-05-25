#include "scannerdialog.h"
#include "scanworker.h"
#include "projectfilevalidatorworker.h" // Make sure this is correctly included

#include <QCloseEvent>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QProgressBar>
#include <QLabel>
#include <QDialogButtonBox>
#include <QCoreApplication>
#include <QTimer>
#include <QMovie>
#include <QDebug>
#include <QElapsedTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QStorageInfo>
#include <QProcess>     // For Linux /proc/mounts parsing if needed (alternative to QFile)

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ScannerDialog::ScannerDialog(QWidget *parent)
    : FramelessDialogBase(parent),
      m_stackedWidget(nullptr),
      m_initialPromptPage(nullptr),
      m_dontShowPromptAgainCheckBox(nullptr),
      m_configPage(nullptr),
      m_quickScanRadio(nullptr),
      m_deepScanRadio(nullptr),
      m_fullDiskRadio(nullptr),
      m_selectDrivesRadio(nullptr),
      m_selectFolderRadio(nullptr),
      m_drivesListWidget(nullptr),
      m_folderPathEdit(nullptr),
      m_browseFolderButton(nullptr),
      m_folderSelectWidget(nullptr),
      m_drivesListContainerWidget(nullptr),
      m_progressPage(nullptr),
      m_progressStatusLabel(nullptr),
      m_progressCurrentPathLabel(nullptr),
      m_progressBar(nullptr),
      m_progressTimeEtcLabel(nullptr),
      m_progressAnimationLabel(nullptr),
      m_progressCancelButton(nullptr),
      m_logPage(nullptr),
      m_logTableWidget(nullptr),
      m_exportLogButton(nullptr),
      m_resultsPage(nullptr),
      m_resultsTableWidget(nullptr),
      m_resultsSelectAllButton(nullptr),
      m_resultsDeselectAllButton(nullptr),
      m_resultsButtonBox(nullptr),
      m_settings(nullptr),
      m_scanWorker(nullptr),
      m_validatorWorker(nullptr),
      m_scanInProgress(false),
      m_scanCancelled(false),
      m_scanStartTime(0)
{
    setWindowTitle("Project Scanner");
    // It's good practice to set a unique object name for top-level dialogs for styling/testing
    setObjectName("ScannerDialogBase");

    m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "SOFTUDIO", "ProjectScanner", this);

    setupUi();
    loadSettings();

    // Connections for thread cleanup
    connect(&m_scanWorkerThread, &QThread::finished, this, [this]() {
        if (m_scanWorker) {
            qDebug() << "ScannerDialog: Scan worker thread finished, deleting worker.";
            m_scanWorker->deleteLater();
            m_scanWorker = nullptr;
        }
    });
    connect(&m_validatorThread, &QThread::finished, this, [this]() {
        if (m_validatorWorker) {
            qDebug() << "ScannerDialog: Validator thread finished, deleting worker.";
            m_validatorWorker->deleteLater();
            m_validatorWorker = nullptr;
        }
    });
}

ScannerDialog::~ScannerDialog()
{
    qDebug() << "ScannerDialog: Destructor called.";
    stopScanThreadsAndCleanup(); // Ensure threads are stopped before dialog is destroyed
    // m_settings is a child, will be deleted by QObject parent.
}

void ScannerDialog::setKnownProjectUids(const QSet<QString>& knownUids) {
    m_knownProjectUids = knownUids;
    qDebug() << "ScannerDialog: Known project UIDs set. Count:" << m_knownProjectUids.size();
}

void ScannerDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this); // 'this' is the FramelessDialogBase
    mainLayout->setContentsMargins(1, 1, 1, 1); // Minimal margins for the frameless base
    mainLayout->setSpacing(0);                 // No spacing for the stacked widget container

    m_stackedWidget = new QStackedWidget(this);

    setupInitialPromptPage();
    setupConfigPage();
    setupProgressPage();
    setupLogPage();
    setupResultsPage();

    mainLayout->addWidget(m_stackedWidget);
    // setLayout(mainLayout); // Already set by FramelessDialogBase if it calls setLayout in its constructor

    QTimer::singleShot(0, this, [this]() {
        bool dontShow = m_settings->value(SETTING_DONT_SHOW_PROMPT_V2, false).toBool();
        bool isStandalone = QCoreApplication::instance()->property("is_standalone_runner").toBool();

        if (isStandalone) {
            showPage(dontShow ? Configuration : InitialPrompt);
        } else { // Integrated mode
            showPage(dontShow ? Configuration : InitialPrompt);
            // If integrated and "don't show" is true, the parent application
            // might decide not to even 'exec' this dialog, or call a specific method
            // to directly start the configuration or scan.
        }
    });
}

void ScannerDialog::setupInitialPromptPage() {
    m_initialPromptPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_initialPromptPage);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignCenter); // Center all content

    QLabel *titleLabel = new QLabel("Project Scan", m_initialPromptPage);
    titleLabel->setObjectName("promptTitleLabel");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(13);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);

    QLabel *infoLabel = new QLabel("Would you like to perform a scan for projects?\nThis can help you quickly add existing projects.", m_initialPromptPage);
    infoLabel->setObjectName("promptInformativeLabel");
    infoLabel->setWordWrap(true);
    infoLabel->setAlignment(Qt::AlignCenter);

    m_dontShowPromptAgainCheckBox = new QCheckBox("Don't show this message again.", m_initialPromptPage);
    m_dontShowPromptAgainCheckBox->setToolTip("If checked, this prompt will not appear automatically next time.");

    QDialogButtonBox *buttonBox = new QDialogButtonBox(m_initialPromptPage);
    QPushButton *scanNowButton = buttonBox->addButton("Scan Now", QDialogButtonBox::AcceptRole);
    QPushButton *laterButton = buttonBox->addButton("Later", QDialogButtonBox::RejectRole);
    scanNowButton->setDefault(true);

    connect(scanNowButton, &QPushButton::clicked, this, &ScannerDialog::onInitialPromptScanNow);
    connect(laterButton, &QPushButton::clicked, this, &ScannerDialog::onInitialPromptLater);

    // Group content with proper spacing
    layout->addSpacing(20);
    layout->addWidget(titleLabel);
    layout->addSpacing(15);
    layout->addWidget(infoLabel);
    layout->addSpacing(20);
    layout->addWidget(m_dontShowPromptAgainCheckBox, 0, Qt::AlignCenter);
    layout->addSpacing(15);
    layout->addWidget(buttonBox);
    layout->addSpacing(20);

    m_stackedWidget->addWidget(m_initialPromptPage);
}

void ScannerDialog::onInitialPromptScanNow() {
    if (m_dontShowPromptAgainCheckBox->isChecked()) {
        m_settings->setValue(SETTING_DONT_SHOW_PROMPT_V2, true);
    }
    showPage(Configuration);
}

void ScannerDialog::onInitialPromptLater() {
    if (m_dontShowPromptAgainCheckBox->isChecked()) {
        m_settings->setValue(SETTING_DONT_SHOW_PROMPT_V2, true);
    }
    // If running standalone and user clicks "Later", it should probably exit.
    if (QCoreApplication::instance()->property("is_standalone_runner").toBool()) {
        QTimer::singleShot(0, this, &ScannerDialog::reject); // reject will close dialog, then app can quit
    } else {
        reject(); // For integrated mode, just reject.
    }
}

void ScannerDialog::setupConfigPage() {
    m_configPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_configPage);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignTop); // Keep content at top, allow natural sizing
    
    QLabel *configTitleLabel = new QLabel("Configure Project Scan", m_configPage);
    configTitleLabel->setObjectName("dialogTitleLabel");
    QFont configTitleFont = configTitleLabel->font();
    configTitleFont.setPointSize(12);
    configTitleFont.setBold(true);
    configTitleLabel->setFont(configTitleFont);
    configTitleLabel->setAlignment(Qt::AlignCenter);

    QGroupBox *scanTypeGroup = new QGroupBox("Scan Type", m_configPage);
    QVBoxLayout *scanTypeLayout = new QVBoxLayout(scanTypeGroup);
    m_quickScanRadio = new QRadioButton(SCAN_TYPE_QUICK, scanTypeGroup);
    m_deepScanRadio = new QRadioButton(SCAN_TYPE_DEEP, scanTypeGroup);
    m_quickScanRadio->setToolTip("Scans only the top few levels of folders. Faster.");
    m_deepScanRadio->setToolTip("Scans every subfolder. Slower but more thorough.");
    scanTypeLayout->addWidget(m_quickScanRadio);
    scanTypeLayout->addWidget(m_deepScanRadio);
    scanTypeGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    QGroupBox *scanScopeGroup = new QGroupBox("Scan Scope", m_configPage);
    QVBoxLayout *scanScopeLayout = new QVBoxLayout(scanScopeGroup);
    m_fullDiskRadio = new QRadioButton(SCAN_SCOPE_FULL_DISK, scanScopeGroup);
    m_selectDrivesRadio = new QRadioButton(SCAN_SCOPE_DRIVES, scanScopeGroup);

    m_drivesListContainerWidget = new QWidget(scanScopeGroup);
    QVBoxLayout* drivesListLayout = new QVBoxLayout(m_drivesListContainerWidget);
    drivesListLayout->setContentsMargins(0,0,0,0);
    m_drivesListWidget = new QListWidget(m_drivesListContainerWidget);
    m_drivesListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_drivesListWidget->setMinimumHeight(80); // Set reasonable minimum instead of 0
    m_drivesListWidget->setMaximumHeight(120); // Limit maximum height
    m_drivesListWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    drivesListLayout->addWidget(m_drivesListWidget);

    m_selectFolderRadio = new QRadioButton(SCAN_SCOPE_FOLDER, scanScopeGroup);

    m_folderSelectWidget = new QWidget(scanScopeGroup);
    QHBoxLayout *folderLayout = new QHBoxLayout(m_folderSelectWidget);
    folderLayout->setContentsMargins(0,0,0,0);
    m_folderPathEdit = new QLineEdit(m_folderSelectWidget);
    m_folderPathEdit->setPlaceholderText("Select a folder to scan...");
    m_folderPathEdit->setReadOnly(true);
    m_browseFolderButton = new QPushButton("Browse...", m_folderSelectWidget);
    folderLayout->addWidget(m_folderPathEdit, 1);
    folderLayout->addWidget(m_browseFolderButton);

    scanScopeLayout->addWidget(m_fullDiskRadio);
    scanScopeLayout->addWidget(m_selectDrivesRadio);
    scanScopeLayout->addWidget(m_drivesListContainerWidget);
    scanScopeLayout->addWidget(m_selectFolderRadio);
    scanScopeLayout->addWidget(m_folderSelectWidget);
    scanScopeGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    connect(m_browseFolderButton, &QPushButton::clicked, this, &ScannerDialog::browseDirectory);
    connect(m_fullDiskRadio, &QRadioButton::toggled, this, &ScannerDialog::onScanScopeChanged);
    connect(m_selectDrivesRadio, &QRadioButton::toggled, this, &ScannerDialog::onScanScopeChanged);
    connect(m_selectFolderRadio, &QRadioButton::toggled, this, &ScannerDialog::onScanScopeChanged);
    connect(m_drivesListWidget, &QListWidget::itemChanged, this, &ScannerDialog::onDrivesListItemChanged);
    connect(m_quickScanRadio, &QRadioButton::toggled, this, &ScannerDialog::onScanTypeChanged);
    connect(m_deepScanRadio, &QRadioButton::toggled, this, &ScannerDialog::onScanTypeChanged);

    QDialogButtonBox *configButtonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, m_configPage);
    QPushButton* nextButton = configButtonBox->addButton("Next", QDialogButtonBox::AcceptRole);
    nextButton->setDefault(true);
    connect(nextButton, &QPushButton::clicked, this, &ScannerDialog::onConfigNextClicked);
    connect(configButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Add content with proper spacing - no stretches
    layout->addSpacing(10);
    layout->addWidget(configTitleLabel);
    layout->addSpacing(15);
    layout->addWidget(scanTypeGroup);
    layout->addSpacing(12);
    layout->addWidget(scanScopeGroup); // Removed stretch factor
    layout->addSpacing(15);
    layout->addWidget(configButtonBox);
    layout->addSpacing(10);

    m_stackedWidget->addWidget(m_configPage);
    populateDrivesList();
}

void ScannerDialog::onConfigNextClicked() {
    QStringList paths = getSelectedScanPaths();
    if (paths.isEmpty()) {
        QMessageBox::warning(this, "Configuration Incomplete", "Please select at least one drive/folder to scan, or choose 'Scan Full Computer'.");
        return;
    }
    saveSettings();
    startActualScan();
}

void ScannerDialog::browseDirectory() {
    QString lastPath = m_settings->value(SETTING_LAST_SCAN_PATH, QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).toString();
    QString dir = QFileDialog::getExistingDirectory(this, "Select Folder to Scan", lastPath);
    if (!dir.isEmpty()) {
        m_folderPathEdit->setText(QDir::toNativeSeparators(dir));
        // If selecting a folder, also update the radio button
        m_selectFolderRadio->setChecked(true);
    }
}

void ScannerDialog::onScanScopeChanged() {
    m_drivesListContainerWidget->setVisible(m_selectDrivesRadio->isChecked());
    m_folderSelectWidget->setVisible(m_selectFolderRadio->isChecked());

    if (m_configPage->layout()) {
        m_configPage->layout()->invalidate();
        m_configPage->layout()->activate();
    }
    // Keep adjustSize as a general layout hint, though invalidate/activate is more targeted.
    QTimer::singleShot(0, this, &QWidget::adjustSize);
}

void ScannerDialog::onDrivesListItemChanged(QListWidgetItem* item) {
    if (item && item->listWidget() == m_drivesListWidget) {
        if(item->checkState() != Qt::Unchecked && !m_selectDrivesRadio->isChecked()){
            // m_selectDrivesRadio->setChecked(true); // This might be too aggressive, user might be exploring
        }
    }
}

void ScannerDialog::onScanTypeChanged()
{
    qDebug() << "ScannerDialog: Scan type changed to"
             << (m_quickScanRadio->isChecked() ? "Quick" : "Deep");
    saveSettings(); // Update settings with the new scan type
}

void ScannerDialog::setupProgressPage() {
    m_progressPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_progressPage);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignCenter); // Center the progress content

    m_progressStatusLabel = new AnimatedLoadingLabel("Initializing scan...", m_progressPage);
    m_progressStatusLabel->setObjectName("scanStatusAnimatedLabel");
    QFont statusFont = m_progressStatusLabel->font();
    statusFont.setPointSize(11);
    statusFont.setBold(true);
    m_progressStatusLabel->setFont(statusFont);

    m_progressCurrentPathLabel = new QLabel(" ", m_progressPage);
    m_progressCurrentPathLabel->setObjectName("scanDetailPathLabel");
    m_progressCurrentPathLabel->setWordWrap(false);

    m_progressBar = new QProgressBar(m_progressPage);
    m_progressBar->setTextVisible(true);
    m_progressBar->setRange(0,0);
    m_progressBar->setValue(0);
    m_progressBar->setMinimumHeight(24);
    m_progressBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_progressTimeEtcLabel = new QLabel("Elapsed: 00:00:00 | ETA: Calculating...", m_progressPage);
    m_progressTimeEtcLabel->setAlignment(Qt::AlignCenter);

    QHBoxLayout* bottomBarLayout = new QHBoxLayout();
    bottomBarLayout->setContentsMargins(0, 0, 0, 0);
    bottomBarLayout->setSpacing(10);
    m_progressAnimationLabel = new QLabel(m_progressPage);
    m_progressAnimationLabel->setFixedSize(200, 60);
    m_progressAnimationLabel->setScaledContents(true);
    m_progressAnimationLabel->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    bottomBarLayout->addWidget(m_progressAnimationLabel);
    bottomBarLayout->addStretch(); // Keep this stretch for button positioning

    m_progressCancelButton = new QPushButton("Cancel Scan", m_progressPage);
    m_progressCancelButton->setMinimumSize(100, 30);
    connect(m_progressCancelButton, &QPushButton::clicked, this, &ScannerDialog::cancelScanRequestedByProgressPage);
    bottomBarLayout->addWidget(m_progressCancelButton, 0, Qt::AlignBottom | Qt::AlignRight);

    // Add content with proper spacing
    layout->addSpacing(20);
    layout->addWidget(m_progressStatusLabel);
    layout->addSpacing(10);
    layout->addWidget(m_progressCurrentPathLabel);
    layout->addSpacing(10);
    layout->addWidget(m_progressBar);
    layout->addSpacing(10);
    layout->addWidget(m_progressTimeEtcLabel);
    layout->addSpacing(15);
    layout->addLayout(bottomBarLayout);
    layout->addSpacing(20);

    QString animPathPrefix = QCoreApplication::applicationDirPath() + "/Engine/Graphics/Animation/STScan.anim/";
    if (!QDir(animPathPrefix).exists()) {
        animPathPrefix = ":/animations/STScan.anim/";
        qWarning() << "Animation path" << QCoreApplication::applicationDirPath() + "/Engine/Graphics/Animation/STScan.anim/" << "not found. Attempting resource path" << animPathPrefix;
    }

    QStringList movieKeys = {"Initializing", "Scanning", "Finalizing", "Canceling", "Aborting"};
    for(const QString& key : movieKeys) {
        QString filePath = animPathPrefix + key + ".gif";
        QMovie *movie = new QMovie(filePath, QByteArray(), this);
        if(movie->isValid()){
            movie->setCacheMode(QMovie::CacheAll);
            movie->setSpeed(100);
            m_progressMovies[key] = movie;
            qDebug() << "ScannerDialog: Loaded animation" << key << "from" << filePath;
        } else {
            qWarning() << "ScannerDialog: Failed to load or invalid animation" << key << "from" << filePath << "Error:" << movie->lastErrorString();
            delete movie;
        }
    }

    m_stackedWidget->addWidget(m_progressPage);
}

void ScannerDialog::setupLogPage() {
    m_logPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_logPage);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignTop); // Align to top for natural sizing

    QLabel *logTitle = new QLabel("Scan Log", m_logPage);
    logTitle->setObjectName("dialogTitleLabel");
    QFont logTitleFont = logTitle->font();
    logTitleFont.setPointSize(12);
    logTitleFont.setBold(true);
    logTitle->setFont(logTitleFont);
    logTitle->setAlignment(Qt::AlignCenter);

    QLabel *infoLabel = new QLabel("The scan encountered issues with the following paths:", m_logPage);
    infoLabel->setObjectName("promptInformativeLabel");

    m_logTableWidget = new QTableWidget(m_logPage);
    m_logTableWidget->setColumnCount(2);
    m_logTableWidget->setHorizontalHeaderLabels(QStringList() << "Path" << "Reason");
    m_logTableWidget->horizontalHeader()->setStretchLastSection(true);
    m_logTableWidget->verticalHeader()->setVisible(false);
    m_logTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logTableWidget->setAlternatingRowColors(true);
    m_logTableWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_logTableWidget->setMinimumHeight(200); // Set reasonable minimum height

    QDialogButtonBox *logButtonBox = new QDialogButtonBox(m_logPage);
    m_exportLogButton = logButtonBox->addButton("Export Log", QDialogButtonBox::ActionRole);
    QPushButton *nextButton = logButtonBox->addButton("Next", QDialogButtonBox::AcceptRole);
    nextButton->setDefault(true);

    connect(m_exportLogButton, &QPushButton::clicked, this, &ScannerDialog::exportScanLog);
    connect(nextButton, &QPushButton::clicked, this, &ScannerDialog::onLogDialogNextClicked);

    // Add content with proper spacing
    layout->addSpacing(10);
    layout->addWidget(logTitle);
    layout->addSpacing(5);
    layout->addWidget(infoLabel);
    layout->addSpacing(10);
    layout->addWidget(m_logTableWidget); // Removed stretch factor
    layout->addSpacing(15);
    layout->addWidget(logButtonBox);
    layout->addSpacing(10);

    m_stackedWidget->addWidget(m_logPage);
}

void ScannerDialog::setupResultsPage() {
    m_resultsPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_resultsPage);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignTop); // Align to top for natural sizing

    QLabel *resultsTitle = new QLabel("Select Projects to Add", m_resultsPage);
    resultsTitle->setObjectName("dialogTitleLabel");
    QFont resultsTitleFont = resultsTitle->font();
    resultsTitleFont.setPointSize(12);
    resultsTitleFont.setBold(true);
    resultsTitle->setFont(resultsTitleFont);
    resultsTitle->setAlignment(Qt::AlignCenter);

    QLabel *infoLabel = new QLabel("The following potential Softudio projects were found. Select which ones to add:", m_resultsPage);
    infoLabel->setObjectName("promptInformativeLabel");

    m_resultsTableWidget = new QTableWidget(m_resultsPage);
    m_resultsTableWidget->setColumnCount(3);
    m_resultsTableWidget->setHorizontalHeaderLabels(QStringList() << "" << "Project Name" << "Location Path");
    m_resultsTableWidget->horizontalHeader()->setStretchLastSection(true);
    m_resultsTableWidget->verticalHeader()->setVisible(false);
    m_resultsTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_resultsTableWidget->setAlternatingRowColors(true);
    m_resultsTableWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_resultsTableWidget->setMinimumHeight(200); // Set reasonable minimum height
    connect(m_resultsTableWidget, &QTableWidget::cellChanged, this, &ScannerDialog::onResultsSelectionChanged);

    QHBoxLayout *selectionButtonsLayout = new QHBoxLayout();
    m_resultsSelectAllButton = new QPushButton("Select All", m_resultsPage);
    m_resultsDeselectAllButton = new QPushButton("Deselect All", m_resultsPage);
    connect(m_resultsSelectAllButton, &QPushButton::clicked, this, [this](){ selectAllResults(true); });
    connect(m_resultsDeselectAllButton, &QPushButton::clicked, this, [this](){ selectAllResults(false); });
    selectionButtonsLayout->addStretch(); // Keep stretch for button positioning
    selectionButtonsLayout->addWidget(m_resultsSelectAllButton);
    selectionButtonsLayout->addWidget(m_resultsDeselectAllButton);

    m_resultsButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, m_resultsPage);
    m_resultsButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    m_resultsButtonBox->button(QDialogButtonBox::Ok)->setDefault(true);
    connect(m_resultsButtonBox, &QDialogButtonBox::accepted, this, &ScannerDialog::acceptProjectSelection);
    connect(m_resultsButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Add content with proper spacing
    layout->addSpacing(10);
    layout->addWidget(resultsTitle);
    layout->addSpacing(5);
    layout->addWidget(infoLabel);
    layout->addSpacing(10);
    layout->addWidget(m_resultsTableWidget); // Removed stretch factor
    layout->addSpacing(10);
    layout->addLayout(selectionButtonsLayout);
    layout->addSpacing(10);
    layout->addWidget(m_resultsButtonBox);
    layout->addSpacing(10);

    m_stackedWidget->addWidget(m_resultsPage);
}

void ScannerDialog::loadSettings() {
    m_quickScanRadio->setChecked(m_settings->value(SETTING_LAST_SCAN_TYPE, SCAN_TYPE_QUICK).toString() == SCAN_TYPE_QUICK);
    m_deepScanRadio->setChecked(!m_quickScanRadio->isChecked());

    QString lastScope = m_settings->value(SETTING_LAST_SCAN_SCOPE, SCAN_SCOPE_FULL_DISK).toString();
    if (lastScope == SCAN_SCOPE_FULL_DISK) m_fullDiskRadio->setChecked(true);
    else if (lastScope == SCAN_SCOPE_DRIVES) m_selectDrivesRadio->setChecked(true);
    else if (lastScope == SCAN_SCOPE_FOLDER) m_selectFolderRadio->setChecked(true);
    else m_fullDiskRadio->setChecked(true);

    m_folderPathEdit->setText(m_settings->value(SETTING_LAST_SCAN_PATH, QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).toString());

    // Update drives list check states AFTER populating the list
    QStringList lastDrives = m_settings->value(SETTING_LAST_SELECTED_DRIVES).toStringList();
    for (int i = 0; i < m_drivesListWidget->count(); ++i) {
        QListWidgetItem *item = m_drivesListWidget->item(i);
        if (item) { // Ensure item is valid
             item->setCheckState(lastDrives.contains(item->text()) ? Qt::Checked : Qt::Unchecked);
        }
    }
    onScanScopeChanged();
}

void ScannerDialog::saveSettings() {
    m_settings->setValue(SETTING_LAST_SCAN_TYPE, getSelectedScanType());
    if(m_fullDiskRadio->isChecked()) m_settings->setValue(SETTING_LAST_SCAN_SCOPE, SCAN_SCOPE_FULL_DISK);
    else if(m_selectDrivesRadio->isChecked()) m_settings->setValue(SETTING_LAST_SCAN_SCOPE, SCAN_SCOPE_DRIVES);
    else if(m_selectFolderRadio->isChecked()) m_settings->setValue(SETTING_LAST_SCAN_SCOPE, SCAN_SCOPE_FOLDER);

    if (!m_folderPathEdit->text().isEmpty()) {
        m_settings->setValue(SETTING_LAST_SCAN_PATH, m_folderPathEdit->text());
    }

    QStringList selectedDrives;
    for(int i=0; i < m_drivesListWidget->count(); ++i) {
        if(m_drivesListWidget->item(i)->checkState() == Qt::Checked) {
            selectedDrives.append(m_drivesListWidget->item(i)->text());
        }
    }
    m_settings->setValue(SETTING_LAST_SELECTED_DRIVES, selectedDrives);
}

void ScannerDialog::startScanThreads() {
    if (m_scanInProgress) {
        qWarning() << "ScannerDialog: Scan already in progress. Ignoring request to start new scan threads.";
        return;
    }

    stopScanThreadsAndCleanup(); // Ensure any previous threads are fully stopped

    m_scanWorker = new ScanWorker();
    m_scanWorker->moveToThread(&m_scanWorkerThread);

    // ScanWorker connections
    connect(this, &ScannerDialog::requestScanWorkerStart, m_scanWorker, &ScanWorker::doScan);
    connect(this, &ScannerDialog::requestScanWorkerStop, m_scanWorker, &ScanWorker::stopScan, Qt::DirectConnection); // Direct for immediate effect
    connect(m_scanWorker, &ScanWorker::scanProgress, this, &ScannerDialog::updateScanProgressUI);
    connect(m_scanWorker, &ScanWorker::projectFound, this, &ScannerDialog::addFoundProjectToInternalList);
    connect(m_scanWorker, &ScanWorker::scanFinished, this, &ScannerDialog::onScanWorkerFinished);
    connect(m_scanWorker, &ScanWorker::validationRequested, this, &ScannerDialog::requestValidateProject); // Connect to new signal

    m_validatorWorker = new ProjectFileValidatorWorker();
    m_validatorWorker->moveToThread(&m_validatorThread);

    // ValidatorWorker connections
    connect(this, &ScannerDialog::requestValidateProject, m_validatorWorker, &ProjectFileValidatorWorker::validateProject);
    connect(m_validatorWorker, &ProjectFileValidatorWorker::projectValidated, this, &ScannerDialog::onProjectFileValidated);

    m_scanWorkerThread.setObjectName("ScanWorkerThread");
    m_validatorThread.setObjectName("ValidatorWorkerThread");

    m_scanWorkerThread.start();
    m_validatorThread.start();
    m_scanInProgress = true;
    m_scanCancelled = false;
    qDebug() << "ScannerDialog: Scan and validator threads started.";
}

void ScannerDialog::stopScanThreadsAndCleanup() {
    qDebug() << "ScannerDialog: Stopping scan threads and cleaning up...";
    m_scanInProgress = false; // Set this early

    if (m_scanWorker && m_scanWorkerThread.isRunning()) {
        qDebug() << "ScannerDialog: Requesting scan worker to stop.";
        emit requestScanWorkerStop(); // Signal the worker to stop
    }
    if (m_scanWorkerThread.isRunning()) {
        qDebug() << "ScannerDialog: Quitting scan worker thread.";
        m_scanWorkerThread.quit();
        if (!m_scanWorkerThread.wait(3000)) { // Wait for graceful quit
            qWarning() << "ScannerDialog: Scan worker thread did not quit gracefully, terminating.";
            m_scanWorkerThread.terminate();
            m_scanWorkerThread.wait(); // Wait for termination
        }
    }
    if (m_validatorThread.isRunning()) {
        qDebug() << "ScannerDialog: Quitting validator worker thread.";
        m_validatorThread.quit();
        if(!m_validatorThread.wait(1000)){
            qWarning() << "ScannerDialog: Validator worker thread did not quit gracefully, terminating.";
            m_validatorThread.terminate();
            m_validatorThread.wait();
        }
    }
    qDebug() << "ScannerDialog: Scan threads cleanup attempt finished.";
}

void ScannerDialog::showPage(int index) {
    if (index >= 0 && index < m_stackedWidget->count()) {
        m_stackedWidget->setCurrentIndex(index);
        adjustSize();
    } else {
        qWarning() << "ScannerDialog: Attempted to show invalid page index:" << index;
    }
}

QStringList ScannerDialog::getAvailableScanLocations() {
    QSet<QString> drivesSet;

#if defined(Q_OS_WIN)
    DWORD logicalDrives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if ((logicalDrives >> i) & 1) {
            QString driveLetter = QString(QChar('A' + i));
            QString drivePath = driveLetter + ":\\";
            // Check if drive is ready and accessible (GetDriveType can be more specific)
            UINT driveType = GetDriveTypeA((drivePath.toStdString() + "\\").c_str()); // Needs trailing backslash for GetDriveType
             if (driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE || driveType == DRIVE_REMOTE) {
                QFileInfo fileInfo(drivePath);
                if (fileInfo.exists() && fileInfo.isReadable()) {
                    drivesSet.insert(QDir::toNativeSeparators(drivePath));
                }
            }
        }
    }
    if (!drivesSet.contains("C:\\") && QFileInfo("C:\\").exists() && QFileInfo("C:\\").isReadable()) {
        drivesSet.insert("C:\\");
    }

#elif defined(Q_OS_LINUX) || defined(Q_OS_MACX) // Q_OS_MACX for macOS specifically
    QStringList stdRoots;
    stdRoots << QDir::rootPath() << QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (QSysInfo::productType().toLower() == "osx" || QSysInfo::productType().toLower() == "macos") { // More robust macOS check
        stdRoots << "/Users";
    }

    for (const QString& rPath : stdRoots) {
        QFileInfo fi(rPath);
        if (fi.exists() && fi.isDir() && fi.isReadable()) {
            drivesSet.insert(QDir::toNativeSeparators(fi.canonicalFilePath()));
        }
    }

    QStringList commonMountParents;
    commonMountParents << "/mnt" << "/media";
     // /run/media is often user-specific, check if it exists
    if (QDir("/run/media").exists()) commonMountParents << "/run/media";

    if (QSysInfo::productType().toLower() == "osx" || QSysInfo::productType().toLower() == "macos") {
        commonMountParents << "/Volumes";
    }

    for (const QString& parentMount : commonMountParents) {
        QDir mountDir(parentMount);
        if (mountDir.exists() && mountDir.isReadable()) {
            QFileInfoList entries = mountDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden, QDir::Name);
            for (const QFileInfo& entry : entries) {
                if (entry.isReadable()) {
                    // QStorageInfo is better for checking if it's a real mount
                    QStorageInfo storage(entry.filePath());
                    if (storage.isValid() && storage.isReady() && !storage.rootPath().isEmpty()) {
                         drivesSet.insert(QDir::toNativeSeparators(storage.rootPath()));
                    } else if (entry.isDir()) { // Fallback for older Qt or non-obvious mounts
                         drivesSet.insert(QDir::toNativeSeparators(entry.canonicalFilePath()));
                    }
                }
            }
        }
    }

#if defined(Q_OS_LINUX)
    QFile procMounts("/proc/mounts");
    if (procMounts.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&procMounts);
        QStringList excludedFsTypes = {"proc", "sysfs", "devtmpfs", "devpts", "tmpfs", "securityfs",
                                     "cgroup", "pstore", "debugfs", "hugetlbfs", "mqueue",
                                     "fuse.gvfsd-fuse", "fusectl", "tracefs", "binfmt_misc",
                                     "configfs", "efivarfs", "snapfuse", "squashfs", "autofs", "rpc_pipefs", "overlay", "nsfs"};
        QStringList excludedPathPrefixes = {"/dev", "/proc", "/sys", "/run/user", "/run/lock", "/boot", "/snap", "/tmp",
                                           "/var/lib/docker", "/var/lib/snapd", "/var/tmp"};
        while(!in.atEnd()) {
            QString line = in.readLine().trimmed();
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                QString device = parts[0];
                QString mountPoint = parts[1];
                QString fsType = parts[2];

                bool isExcluded = excludedFsTypes.contains(fsType);
                if (device.startsWith("/dev/loop") || device.startsWith("/dev/snap")) isExcluded = true; // Squashfs often via loop

                for(const QString& prefix : excludedPathPrefixes) {
                    if (mountPoint.startsWith(prefix)) {
                        isExcluded = true;
                        break;
                    }
                }

                if (!isExcluded && mountPoint.startsWith("/")) {
                    QFileInfo fi(mountPoint);
                    QStorageInfo storage(mountPoint);
                    if (storage.isValid() && storage.isReady() && !storage.isReadOnly() && (storage.bytesTotal() > 0 || fsType.contains("nfs") || fsType.contains("cifs")) ) { // Check if real storage
                         drivesSet.insert(QDir::toNativeSeparators(storage.rootPath()));
                    } else if (fi.exists() && fi.isDir() && fi.isReadable() && !storage.isValid()) { // Fallback for non-storageinfo recognized mounts
                         drivesSet.insert(QDir::toNativeSeparators(fi.canonicalFilePath()));
                    }
                }
            }
        }
        procMounts.close();
    }
#endif // Q_OS_LINUX

#else // Other OS (very basic fallback)
    QFileInfoList qDrives = QDir::drives();
    for (const QFileInfo &drive : qDrives) {
        if(drive.isReadable()){
            drivesSet.insert(QDir::toNativeSeparators(drive.canonicalFilePath()));
        }
    }
#endif

    if (drivesSet.isEmpty()) {
        QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        QFileInfo fiHome(homePath);
        if (fiHome.exists() && fiHome.isDir() && fiHome.isReadable()) {
            drivesSet.insert(QDir::toNativeSeparators(fiHome.canonicalFilePath()));
        } else {
            drivesSet.insert(QDir::toNativeSeparators(QDir::currentPath()));
        }
    }
    
    QStringList driveList = drivesSet.values();
    std::sort(driveList.begin(), driveList.end());
    qDebug() << "ScannerDialog: Detected scannable locations:" << driveList;
    return driveList;
}

void ScannerDialog::populateDrivesList() {
    m_drivesListWidget->clear();
    QStringList availableLocations = getAvailableScanLocations();

    if (availableLocations.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem("No scannable drives/locations found.", m_drivesListWidget);
        item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable)); // Non-interactive
        m_selectDrivesRadio->setEnabled(false);
        m_fullDiskRadio->setChecked(false); // Cannot scan full disk if no drives
        m_fullDiskRadio->setEnabled(false);
        if (!m_selectFolderRadio->isChecked()) { // If nothing else, default to folder selection
            m_selectFolderRadio->setChecked(true);
        }
        return;
    }
    m_selectDrivesRadio->setEnabled(true);
    m_fullDiskRadio->setEnabled(true);

    for (const QString &locationPath : availableLocations) {
        QListWidgetItem *item = new QListWidgetItem(locationPath, m_drivesListWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    // After populating, re-apply settings for checked drives
    QStringList lastDrives = m_settings->value(SETTING_LAST_SELECTED_DRIVES).toStringList();
    for (int i = 0; i < m_drivesListWidget->count(); ++i) {
        QListWidgetItem *item = m_drivesListWidget->item(i);
        if (item && lastDrives.contains(item->text())) {
             item->setCheckState(Qt::Checked);
        }
    }
}

QStringList ScannerDialog::getSelectedScanPaths() {
    QStringList paths;
    if (m_fullDiskRadio->isChecked()) {
        for(int i = 0; i < m_drivesListWidget->count(); ++i) {
            // Ensure item is valid and not the "no locations found" message
            if (m_drivesListWidget->item(i) && (m_drivesListWidget->item(i)->flags() & Qt::ItemIsUserCheckable)) {
                 paths.append(m_drivesListWidget->item(i)->text());
            }
        }
        if (paths.isEmpty()) { // Should not happen if populateDrivesList enables fullDiskRadio
            qWarning() << "Full Disk scan selected but no drives available in the list.";
        }
    } else if (m_selectDrivesRadio->isChecked()) {
        for (int i = 0; i < m_drivesListWidget->count(); ++i) {
            if (m_drivesListWidget->item(i) && m_drivesListWidget->item(i)->checkState() == Qt::Checked) {
                paths.append(m_drivesListWidget->item(i)->text());
            }
        }
    } else if (m_selectFolderRadio->isChecked()) {
        QString folder = QDir::toNativeSeparators(m_folderPathEdit->text());
        if (!folder.isEmpty() && QDir(folder).exists()) {
            paths.append(folder);
        }
    }
    qDebug() << "ScannerDialog: Selected scan paths:" << paths;
    return paths;
}

QString ScannerDialog::getSelectedScanType() {
    return m_quickScanRadio->isChecked() ? SCAN_TYPE_QUICK : SCAN_TYPE_DEEP;
}

void ScannerDialog::startActualScan() {
    m_allFoundProjectsInternalList.clear();
    m_validatedProjectsForResultsTable.clear();
    m_currentScanErrors.clear();
    m_scanCancelled = false;
    m_scanInProgress = true; // Set before starting threads
    m_scanStartTime = QDateTime::currentMSecsSinceEpoch();

    if(m_progressStatusLabel) m_progressStatusLabel->setText("Initializing scan...");
    if(m_progressStatusLabel) m_progressStatusLabel->start_animation();
    if(m_progressCurrentPathLabel) m_progressCurrentPathLabel->setText(" ");
    if(m_progressBar) {
        m_progressBar->setRange(0,0); // Indeterminate
        m_progressBar->setValue(0);
        m_progressBar->setFormat("Initializing...");
    }
    if(m_progressTimeEtcLabel) m_progressTimeEtcLabel->setText("Elapsed: 00:00:00 | ETA: Calculating...");
    if(m_progressCancelButton) {
        m_progressCancelButton->setText("Cancel Scan");
        m_progressCancelButton->setEnabled(true);
        // Ensure previous connections for "Next" or "Close" are removed
        disconnect(m_progressCancelButton, &QPushButton::clicked, nullptr, nullptr);
        connect(m_progressCancelButton, &QPushButton::clicked, this, &ScannerDialog::cancelScanRequestedByProgressPage);
    }
    setProgressAnimation("Initializing");

    showPage(Progress);
    startScanThreads(); // This will also start validator thread
    emit requestScanWorkerStart(getSelectedScanPaths(), getSelectedScanType());
}

void ScannerDialog::cancelScanRequestedByProgressPage() {
    if (!m_scanInProgress || m_scanCancelled) { // Prevent multiple cancel actions
        qDebug() << "ScannerDialog: Cancel request ignored, scan not in progress or already cancelled.";
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm Cancel", "Are you sure you want to cancel the scan?",
                                  QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        qDebug() << "ScannerDialog: User confirmed scan cancellation.";
        m_scanCancelled = true; // Set before emitting stop to worker
        if(m_progressStatusLabel) m_progressStatusLabel->setText("Cancelling scan...");
        if(m_progressStatusLabel) m_progressStatusLabel->start_animation(); // Restart animation if stopped
        if(m_progressCurrentPathLabel) m_progressCurrentPathLabel->setText("Waiting for operations to stop.");
        if(m_progressBar) m_progressBar->setFormat("Cancelling...");
        if(m_progressCancelButton) m_progressCancelButton->setEnabled(false); // Disable while cancelling
        setProgressAnimation("Canceling");

        emit requestScanWorkerStop(); // Signal worker to stop
        // Worker's finished signal will handle final UI updates and thread cleanup.
    } else {
        qDebug() << "ScannerDialog: User aborted scan cancellation.";
    }
}

void ScannerDialog::onScanWorkerFinished(const QList<ProjectInfo>& allFoundProjectsDuringScan, const QString& outcome, const QVariantMap& extra, const QList<QPair<QString, QString>>& errors) {
    Q_UNUSED(allFoundProjectsDuringScan);
    Q_UNUSED(extra); // Might use this for error messages from worker later

    qDebug() << "ScannerDialog: Scan worker processing finished signal. Outcome:" << outcome
             << "Errors:" << errors.size()
             << "Validated Projects (before this signal):" << m_validatedProjectsForResultsTable.size()
             << "Scan Cancelled Flag:" << m_scanCancelled;

    m_scanInProgress = false; // Scan operations are done

    if(m_progressStatusLabel) m_progressStatusLabel->stop_animation();
    m_currentScanErrors.clear(); // Clear previous errors for this scan display
    m_currentScanErrors.append(errors); // Append errors from this scan

    // Ensure the cancel button is re-enabled and setup for next action
    if(m_progressCancelButton) {
        m_progressCancelButton->setEnabled(true);
        disconnect(m_progressCancelButton, &QPushButton::clicked, nullptr, nullptr); // Clear previous connections
    }

    if (m_scanCancelled || outcome == "canceled") {
        qDebug() << "ScannerDialog: Handling CANCELED outcome.";
        if(m_progressStatusLabel) m_progressStatusLabel->setText("Scan Canceled");
        if(m_progressBar) { m_progressBar->setRange(0,100); m_progressBar->setValue(0); m_progressBar->setFormat("Canceled"); }
        setProgressAnimation("Canceling");
        if(m_progressCancelButton) {
            m_progressCancelButton->setText("Close");
            connect(m_progressCancelButton, &QPushButton::clicked, this, &QDialog::reject);
        }
        // No further processing of results if canceled.
        return;
    }

    if (outcome == "error") {
        qDebug() << "ScannerDialog: Handling ERROR outcome.";
        QString errorMessage = extra.value("error_message", "An unspecified error occurred during the scan.").toString();
        if(m_progressStatusLabel) m_progressStatusLabel->setText("Scan Error: " + errorMessage);
        if(m_progressBar) { m_progressBar->setRange(0,100); m_progressBar->setValue(0); m_progressBar->setFormat("Error"); }
        setProgressAnimation("Aborting");
        if(m_progressCancelButton) {
            if (!m_currentScanErrors.isEmpty()) {
                m_progressCancelButton->setText("View Log");
                connect(m_progressCancelButton, &QPushButton::clicked, this, [this](){
                    populateLogTable(m_currentScanErrors);
                    showPage(Log);
                });
            } else {
                m_progressCancelButton->setText("Close");
                connect(m_progressCancelButton, &QPushButton::clicked, this, &QDialog::reject);
                QMessageBox::critical(this, "Scan Error", errorMessage);
            }
        }
        return;
    }

    // Outcome is "completed"
    qDebug() << "ScannerDialog: Handling COMPLETED outcome.";
    if(m_progressStatusLabel) m_progressStatusLabel->setText("Scan Complete");
    if(m_progressBar) {
        m_progressBar->setRange(0,1); m_progressBar->setValue(1);
        m_progressBar->setFormat("Scan Finished");
    }
    setProgressAnimation("Finalizing");

    bool hasNewProjectsToShow = !m_validatedProjectsForResultsTable.isEmpty(); // Check after filtering known UIDs

    if (!m_currentScanErrors.isEmpty()) {
        qDebug() << "ScannerDialog: Scan completed with errors. Progress page will offer 'View Log'.";
        if(m_progressCancelButton) {
             m_progressCancelButton->setText("View Log");
             connect(m_progressCancelButton, &QPushButton::clicked, this, [this](){
                populateLogTable(m_currentScanErrors);
                showPage(Log);
            });
        }
    } else if (!hasNewProjectsToShow) {
         qDebug() << "ScannerDialog: Scan completed, no errors, no new projects to show.";
         if(m_progressCancelButton) {
            m_progressCancelButton->setText("Close");
            connect(m_progressCancelButton, &QPushButton::clicked, this, &QDialog::accept); // Use accept for "completed no new projects"
         }
         QMessageBox::information(this, "Scan Complete", "No new potential projects found.");
    }
    else { // No errors, and new projects to show
        qDebug() << "ScannerDialog: Scan completed, no errors, new projects found. Progress page will offer 'View Results'.";
        if(m_progressCancelButton) {
            m_progressCancelButton->setText("View Results");
            connect(m_progressCancelButton, &QPushButton::clicked, this, [this](){
                populateResultsTable(); // Ensure table is populated before showing
                showPage(Results);
            });
        }
    }
}

void ScannerDialog::updateScanProgressUI(const QString& pathMsg, int totalFoldersEst, int foldersScanned, double elapsedTime, bool isEstimating) {
    if (m_scanCancelled || !m_scanInProgress) return; 

    if (m_progressCurrentPathLabel) {
        QFontMetrics fm(m_progressCurrentPathLabel->font());
        QString elidedText = fm.elidedText(pathMsg, Qt::ElideLeft, m_progressCurrentPathLabel->width() - 5); 
        m_progressCurrentPathLabel->setText(elidedText);
        m_progressCurrentPathLabel->setToolTip(pathMsg);
    }

    if (isEstimating) {
        if(m_progressStatusLabel) m_progressStatusLabel->setText("Phase 1 of 2: Counting folders...");
        setProgressAnimation("Initializing"); 
        if(m_progressBar) {
            m_progressBar->setRange(0,0); 
            m_progressBar->setFormat(QString("Counted: %L1 folders").arg(foldersScanned));
        }
    } else {
        QString statusText = (getSelectedScanType() == SCAN_TYPE_DEEP) ? "Phase 2 of 2: Scanning for projects..." : "Quick Scan: Scanning for projects...";
        if(m_progressStatusLabel) m_progressStatusLabel->setText(statusText);
        setProgressAnimation("Scanning");
        if (m_progressBar) {
            if (totalFoldersEst > 0 && getSelectedScanType() == SCAN_TYPE_DEEP) {
                m_progressBar->setRange(0, totalFoldersEst);
                m_progressBar->setValue(qMin(foldersScanned, totalFoldersEst)); 
                double percentage = (static_cast<double>(qMin(foldersScanned, totalFoldersEst)) / totalFoldersEst) * 100.0;
                m_progressBar->setFormat(QString("%1% (%L2/%L3)").arg(static_cast<int>(percentage)).arg(foldersScanned).arg(totalFoldersEst));
            } else { 
                m_progressBar->setRange(0,0); 
                m_progressBar->setFormat(QString("Scanned: %L1 folders").arg(foldersScanned));
            }
        }
    }
    // UPDATED CALL to updateProgressETA, passing 'isEstimating'
    updateProgressETA(elapsedTime, foldersScanned, isEstimating ? 0 : totalFoldersEst, isEstimating);
}

void ScannerDialog::updateProgressETA(double elapsedTimeSec, int itemsProcessed, int itemsTotal, bool isEstimatingPhase) {
    QString elapsedStr = QTime(0,0,0).addSecs(static_cast<int>(elapsedTimeSec)).toString("HH:mm:ss");
    QString etaStr = "Calculating...";

    if (itemsProcessed > 20 && elapsedTimeSec > 1 && itemsTotal > 0 && getSelectedScanType() == SCAN_TYPE_DEEP && !isEstimatingPhase) { // Check !isEstimatingPhase for deep scan phase 2
        double timePerItem = elapsedTimeSec / itemsProcessed;
        int remainingItems = itemsTotal - itemsProcessed;
        if (remainingItems > 0) {
            double etaSec = timePerItem * remainingItems;
             if (etaSec > 86400 * 2) etaStr = QString("%1+ days").arg(static_cast<int>(etaSec / 86400.0));
             else if (etaSec > 86400) etaStr = QString("%1 day(s)").arg(etaSec / 86400.0, 0, 'f', 1);
             else if (etaSec > 0.1) etaStr = QTime(0,0,0).addSecs(static_cast<int>(etaSec)).toString("HH:mm:ss");
             else etaStr = "Almost done...";
        } else if (itemsProcessed >= itemsTotal) { 
             etaStr = "Finalizing...";
        }
    // CORRECTED LINE: Use !isEstimatingPhase instead of the non-existent member
    } else if (itemsProcessed > 0 && (m_progressBar && m_progressBar->maximum() == 0) && !isEstimatingPhase) { 
        etaStr = "Scanning...";
    } else if (isEstimatingPhase){ // Explicitly check if it's the estimation phase
        etaStr = "Counting...";
    }

    if(m_progressTimeEtcLabel) m_progressTimeEtcLabel->setText(QString("Elapsed: %1 | ETA: %2").arg(elapsedStr, etaStr));
}

void ScannerDialog::setProgressAnimation(const QString& stateKey) {
    QMovie* movie = m_progressMovies.value(stateKey);
    if (m_progressAnimationLabel) { // Ensure label exists
        if (movie && movie->isValid()) {
            if (m_progressAnimationLabel->movie() != movie) {
                if (m_progressAnimationLabel->movie()) {
                    m_progressAnimationLabel->movie()->stop();
                }
                m_progressAnimationLabel->setMovie(movie);
            }
            if(movie->state() != QMovie::Running) movie->start();
            m_progressAnimationLabel->show();
        } else {
            if (m_progressAnimationLabel->movie()) {
                m_progressAnimationLabel->movie()->stop();
                m_progressAnimationLabel->setMovie(nullptr);
            }
            m_progressAnimationLabel->hide();
            if (!movie && !stateKey.isEmpty()) qWarning() << "ScannerDialog: Animation for state" << stateKey << "not found or invalid.";
        }
    }
}

void ScannerDialog::addFoundProjectToInternalList(const ProjectInfo& project) {
    // This list collects all unique paths reported by ScanWorker before validation
    bool exists = false;
    for(const auto& p : m_allFoundProjectsInternalList) {
        if(p.path == project.path) {
            exists = true;
            break;
        }
    }
    if(!exists) {
        m_allFoundProjectsInternalList.append(project);
        qDebug() << "ScannerDialog: Added to internal pre-validation list:" << project.path << "Type:" << project.type;
    }
}

void ScannerDialog::onProjectFileValidated(const ProjectInfo& originalInfo, bool isValid, const QString& validatedName, const QString& validatedUid, bool timedOut, const QString& error)
{
    ProjectInfo updatedInfo = originalInfo; // Copy original info
    updatedInfo.isValidatedSoftudioProject = isValid;

    if (isValid) {
        updatedInfo.name = validatedName.isEmpty() ? QFileInfo(originalInfo.path).fileName() : validatedName; // Fallback to folder name if validatedName is empty
        updatedInfo.uid = validatedUid;
        updatedInfo.type = "softudio_project"; // Mark as a fully validated Softudio project
        qDebug() << "ScannerDialog: Project VALIDATED:" << updatedInfo.path << "Name:" << updatedInfo.name << "UID:" << updatedInfo.uid;
    } else {
        qDebug() << "ScannerDialog: Project NOT validated:" << updatedInfo.path << "TimedOut:" << timedOut << "Error:" << error;
        if (timedOut) {
            m_currentScanErrors.append(QPair<QString, QString>(originalInfo.path, "Validation timed out."));
        } else if (!error.isEmpty()) {
            m_currentScanErrors.append(QPair<QString, QString>(originalInfo.path, "Validation failed: " + error));
        }
        // Keep its heuristic type if it was heuristically found but failed Softudio validation.
        // If it wasn't even heuristically found and failed (e.g. direct validation attempt), mark as failed.
        if (!updatedInfo.heuristicallyFound) updatedInfo.type = "validation_failed";
    }

    // Update the master list (m_allFoundProjectsInternalList) with this more detailed info
    bool foundInAllList = false;
    for(int i = 0; i < m_allFoundProjectsInternalList.size(); ++i) {
        if(m_allFoundProjectsInternalList[i].path == updatedInfo.path) {
            // Preserve original heuristic flag, then overwrite with new info
            bool originalHeuristic = m_allFoundProjectsInternalList[i].heuristicallyFound;
            m_allFoundProjectsInternalList[i] = updatedInfo;
            m_allFoundProjectsInternalList[i].heuristicallyFound = originalHeuristic || updatedInfo.heuristicallyFound; // Ensure heuristic flag isn't lost
            foundInAllList = true;
            break;
        }
    }
    if (!foundInAllList) { // Should ideally not happen if projectFound was emitted first
        m_allFoundProjectsInternalList.append(updatedInfo);
    }

    // Add to the list for the results table (m_validatedProjectsForResultsTable)
    // Only add if it's a valid Softudio project AND not in the known UIDs list.
    if(updatedInfo.isValidatedSoftudioProject) {
        if (!updatedInfo.uid.isEmpty() && m_knownProjectUids.contains(updatedInfo.uid)) {
            qDebug() << "ScannerDialog: Skipping ADD to results (already known UID):" << updatedInfo.name << "(UID:" << updatedInfo.uid << ")";
            return; // Do not add to results table if UID is known
        }

        // Check for duplicates in m_validatedProjectsForResultsTable by path or UID before adding
        bool existsInResults = false;
        for(const auto& p_res : m_validatedProjectsForResultsTable){
            if(p_res.path == updatedInfo.path || (!p_res.uid.isEmpty() && !updatedInfo.uid.isEmpty() && p_res.uid == updatedInfo.uid)) {
                existsInResults = true;
                break;
            }
        }
        if(!existsInResults) {
            m_validatedProjectsForResultsTable.append(updatedInfo);
            qDebug() << "ScannerDialog: Added to results table list:" << updatedInfo.path << "Name:" << updatedInfo.name;
        }
    }
    // Decide if you want to add purely heuristic (non-Softudio) projects to m_validatedProjectsForResultsTable here.
    // For now, it only adds validated Softudio projects.
}

void ScannerDialog::populateLogTable(const QList<QPair<QString, QString>>& errors) {
    m_logTableWidget->setSortingEnabled(false);
    m_logTableWidget->clearContents();
    m_logTableWidget->setRowCount(errors.size());
    for (int i = 0; i < errors.size(); ++i) {
        m_logTableWidget->setItem(i, 0, new QTableWidgetItem(QDir::toNativeSeparators(errors.at(i).first)));
        m_logTableWidget->setItem(i, 1, new QTableWidgetItem(errors.at(i).second));
    }
    m_logTableWidget->resizeColumnsToContents();
    if (m_logTableWidget->horizontalHeader()->count() == 2) { // Ensure columns exist
        int totalWidth = m_logTableWidget->viewport()->width();
        int pathWidth = static_cast<int>(totalWidth * 0.6); // 60% for path
        int reasonWidth = totalWidth - pathWidth - m_logTableWidget->verticalHeader()->width() - 5; // Account for scrollbar etc.
        
        if (m_logTableWidget->columnWidth(0) + m_logTableWidget->columnWidth(1) < totalWidth) {
             m_logTableWidget->setColumnWidth(0, pathWidth);
             m_logTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        } else {
             m_logTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
             m_logTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        }
    }
    m_logTableWidget->setSortingEnabled(true);
}

void ScannerDialog::onLogDialogNextClicked(){
    // After viewing logs, decide whether to show results or close
    bool hasNewProjectsToShow = !m_validatedProjectsForResultsTable.isEmpty();

    if (!hasNewProjectsToShow) { // No new projects even after log
         qDebug() << "ScannerDialog: Log 'Next' clicked, no new projects to show.";
         QMessageBox::information(this, "Scan Complete", "No new potential projects found to add.");
         accept(); // Or reject() depending on desired flow for "nothing found"
    } else {
        qDebug() << "ScannerDialog: Log 'Next' clicked, proceeding to results page.";
        populateResultsTable();
        showPage(Results);
    }
}

void ScannerDialog::exportScanLog() {
    QString defaultFileName = "scan_log_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt";
    QString fileName = QFileDialog::getSaveFileName(this, "Export Scan Log",
                                                    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + defaultFileName,
                                                    "Text Files (*.txt);;All Files (*)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "SOFTUDIO Project Scan Log - " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        out << "-----------------------------------------------------------------------\n\n";
        if (m_currentScanErrors.isEmpty()) {
            out << "No issues reported during the scan.\n";
        } else {
            for (const auto& errorPair : m_currentScanErrors) {
                out << "Path: " << errorPair.first << "\n";
                out << "Reason: " << errorPair.second << "\n\n";
            }
        }
        out << "-----------------------------------------------------------------------\n";
        out << "Scan process finished.\n";
        file.close();
        QMessageBox::information(this, "Export Complete", "Log exported successfully to:\n" + QDir::toNativeSeparators(fileName));
    } else {
        QMessageBox::warning(this, "Export Failed", "Could not write to the specified file.\nError: " + file.errorString());
    }
}

void ScannerDialog::populateResultsTable() {
    m_resultsTableWidget->setSortingEnabled(false);
    m_resultsTableWidget->clearContents();
    m_resultsTableWidget->setRowCount(0);

    // m_validatedProjectsForResultsTable should already be filtered by known UIDs in onProjectFileValidated
    if (m_validatedProjectsForResultsTable.isEmpty()) {
        qDebug() << "ScannerDialog: PopulateResultsTable called, but no validated projects to show.";
        // This state should ideally be handled before showing the results page,
        // but if shown, it will just be an empty table.
        onResultsSelectionChanged(); // Ensure OK button is disabled
        return;
    }
    
    m_resultsTableWidget->setRowCount(m_validatedProjectsForResultsTable.size());

    for (int i = 0; i < m_validatedProjectsForResultsTable.size(); ++i) {
        const ProjectInfo &proj = m_validatedProjectsForResultsTable.at(i);

        QTableWidgetItem *checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        checkItem->setCheckState(Qt::Checked); // Default to checked for new projects
        m_resultsTableWidget->setItem(i, 0, checkItem);

        QTableWidgetItem *nameItem = new QTableWidgetItem(proj.name);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable); // Read-only
        nameItem->setData(Qt::UserRole, proj.uid); // Store UID in UserRole for easy access
        m_resultsTableWidget->setItem(i, 1, nameItem);

        QTableWidgetItem *pathItem = new QTableWidgetItem(QDir::toNativeSeparators(proj.path));
        pathItem->setFlags(pathItem->flags() & ~Qt::ItemIsEditable); // Read-only
        pathItem->setToolTip(QDir::toNativeSeparators(proj.path));
        m_resultsTableWidget->setItem(i, 2, pathItem);
    }
    m_resultsTableWidget->resizeColumnsToContents();
    if (m_resultsTableWidget->columnCount() > 0) m_resultsTableWidget->setColumnWidth(0, 35); // Checkbox column width
    if (m_resultsTableWidget->columnCount() > 1) m_resultsTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive); // Name column
    if (m_resultsTableWidget->columnCount() > 2) m_resultsTableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch); // Path column stretches
    m_resultsTableWidget->setSortingEnabled(true);
    onResultsSelectionChanged(); // Update OK button state based on checks
}

void ScannerDialog::onResultsSelectionChanged() {
    bool anyChecked = false;
    for (int i = 0; i < m_resultsTableWidget->rowCount(); ++i) {
        QTableWidgetItem* item = m_resultsTableWidget->item(i, 0); // Checkbox item
        if (item && item->checkState() == Qt::Checked) {
            anyChecked = true;
            break;
        }
    }
    if(m_resultsButtonBox) m_resultsButtonBox->button(QDialogButtonBox::Ok)->setEnabled(anyChecked);
}

void ScannerDialog::selectAllResults(bool select) {
    for (int i = 0; i < m_resultsTableWidget->rowCount(); ++i) {
        QTableWidgetItem* item = m_resultsTableWidget->item(i,0);
        if(item) { // Ensure item exists
            item->setCheckState(select ? Qt::Checked : Qt::Unchecked);
        }
    }
    onResultsSelectionChanged(); // Update OK button state
}

void ScannerDialog::acceptProjectSelection() {
    QList<ProjectInfo> selectedProjectsList;
    for (int i = 0; i < m_resultsTableWidget->rowCount(); ++i) {
        QTableWidgetItem* checkItem = m_resultsTableWidget->item(i, 0);
        if (checkItem && checkItem->checkState() == Qt::Checked) {
             if (i < m_validatedProjectsForResultsTable.size()) {
                 selectedProjectsList.append(m_validatedProjectsForResultsTable.at(i));
             } else {
                 qWarning() << "ScannerDialog: Row index out of bounds for m_validatedProjectsForResultsTable during selection.";
             }
        }
    }

    if (!selectedProjectsList.isEmpty()) {
        qDebug() << "ScannerDialog: User selected" << selectedProjectsList.size() << "project(s). Emitting signal.";
        emit projectsSelectedForImport(selectedProjectsList);
        // Update known UIDs with the newly selected/imported projects
        for(const auto& proj : selectedProjectsList) {
            if(!proj.uid.isEmpty()) m_knownProjectUids.insert(proj.uid);
        }
    } else {
        qDebug() << "ScannerDialog: OK clicked on results page, but no projects were selected.";
    }
    accept(); // Close the dialog with QDialog::Accepted
}

void ScannerDialog::closeEvent(QCloseEvent *event) {
    qDebug() << "ScannerDialog: closeEvent triggered. Scan in progress:" << m_scanInProgress << "Scan cancelled:" << m_scanCancelled;
    if (m_scanInProgress && !m_scanCancelled) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Scan in Progress",
                                      "A scan is currently in progress. Are you sure you want to cancel and close?",
                                      QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            qDebug() << "ScannerDialog: User chose to cancel and close during active scan.";
            m_scanCancelled = true; // Mark as cancelled
            emit requestScanWorkerStop(); // Signal worker
            event->accept(); // Allow dialog to close, cleanup will happen via worker signals or destructor
        } else {
            qDebug() << "ScannerDialog: User chose not to close during active scan.";
            if(event) event->ignore(); // Prevent closing
            return;
        }
    } else {
         qDebug() << "ScannerDialog: Closing normally (scan not in progress or already cancelled).";
    }
    stopScanThreadsAndCleanup(); // Final cleanup attempt before dialog fully closes
    FramelessDialogBase::closeEvent(event); // Call base class event handler
}

void ScannerDialog::showEvent(QShowEvent *event) {
    FramelessDialogBase::showEvent(event); // Call base first
    qDebug() << "ScannerDialog: showEvent. Current page:" << m_stackedWidget->currentIndex()
             << "Scan in progress:" << m_scanInProgress << "Scan cancelled:" << m_scanCancelled;

    if(m_stackedWidget->currentIndex() == Progress && m_scanInProgress && !m_scanCancelled) {
        if(m_progressStatusLabel) m_progressStatusLabel->start_animation();
        if(m_progressAnimationLabel && m_progressAnimationLabel->movie() && m_progressAnimationLabel->movie()->isValid()) {
            if(m_progressAnimationLabel->movie()->state() != QMovie::Running) {
                m_progressAnimationLabel->movie()->start();
            }
        }
    } else if (m_stackedWidget->currentIndex() == InitialPrompt){
        //possible future logic.
    }
    // Ensure the dialog resizes to its content on show (removed adjustSize for fixed size behavior)
}