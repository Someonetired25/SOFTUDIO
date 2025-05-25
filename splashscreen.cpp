#include "splashscreen.h"
#include "splash_constants.h"
#include "animatedloadinglabel.h"
#include "shiningbutton.h"
#include "loadingworker.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QProgressBar>
#include <QTimer>
#include <QPainter>
#include <QFontMetrics>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QScreen>
#include <QGuiApplication>
#include <QEvent>
#include <QDebug>
#include <QThread>
#include <QCloseEvent>

SplashScreen::SplashScreen(int splashDurationAfterLoadMs, QWidget *parent)
    : QWidget(parent),
      m_splashDurationAfterLoadMs(splashDurationAfterLoadMs),
      m_loadSuccessful(false),
      m_loadingFileLabel(nullptr),
      m_loadingProgressBar(nullptr),
      m_exitButton(nullptr),
      m_loadingContainer(nullptr),
      m_mainLayout(nullptr),
      m_loadingLayout(nullptr),
      m_initialDelayTimer(new QTimer(this)),
      m_thread(nullptr),
      m_worker(nullptr),
      m_totalProgressTasks(0)
{
    m_loadingTasks = {
        { "Importing core modules...", "Project Manager UI", "_task_import_projectmanager", "importing the main application module" },
        { "Loading user preferences...", "Project History & Settings", "_task_load_project_data", "loading project data" },
        { "Loading UI assets...", "Icons", "_task_load_icons", "loading UI icons" },
        { "Loading UI assets...", "Template Images", "_task_load_template_images", "loading template images" },
        { "Finalizing...", "", "", "" },
        { "Ready.", "", "", "" }
    };

    for(const auto& task : m_loadingTasks) {
        if(!task.methodName.isEmpty()) {
            m_totalProgressTasks++;
        }
    }

    setup_icon();
    
    m_backgroundPixmap.load(_get_asset_path(BACKGROUND_IMAGE_PATH));
    if (m_backgroundPixmap.isNull()) {
        qWarning() << "Warning: Could not load background image from" << _get_asset_path(BACKGROUND_IMAGE_PATH) << ". Using fallback color.";
        setFixedSize(600, 400);
    } else {
        setFixedSize(m_backgroundPixmap.size());
    }

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setWindowTitle(tr("Loading SOFTUDIO..."));

    setStyleSheet(R"(
        QWidget { background-color: #1b1b1b; }
        AnimatedLoadingLabel#loadingFileLabel { font-size: 10pt; }
        QProgressBar { border: none; border-radius: 0px; background-color: transparent; height: 10px; }
        QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #FDE047, stop:1 #FBBF24); border-radius: 0px; margin: 0px; }
        ShiningButton#exitButton { font-size: 15pt; padding: 2px; }
    )");

    setup_ui();

    m_initialDelayTimer->setSingleShot(true);
    connect(m_initialDelayTimer, &QTimer::timeout, this, &SplashScreen::start_actual_loading);
    m_initialDelayTimer->start(EXIT_BUTTON_VISIBLE_DURATION_MS);
}

SplashScreen::~SplashScreen()
{
    _cleanup_thread();
}

QString SplashScreen::_get_asset_path(const QString &relative_path) const
{
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + QDir::separator() + relative_path);
}

void SplashScreen::setup_icon()
{
    QString iconPathPrimary = _get_asset_path(APP_ICON_PATH_PRIMARY_REL);
    QString iconPathFallback = _get_asset_path(APP_ICON_PATH_FALLBACK_REL);
    QIcon loadedIcon;

    if (QFile::exists(iconPathPrimary)) {
        loadedIcon = QIcon(iconPathPrimary);
    } else if (QFile::exists(iconPathFallback)) {
        loadedIcon = QIcon(iconPathFallback);
    }

    if (!loadedIcon.isNull()) {
        setWindowIcon(loadedIcon);
    } else {
        qWarning() << "Warning: SplashScreen icon could not be loaded from:" << iconPathPrimary << "or" << iconPathFallback;
    }
}

void SplashScreen::setup_ui()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    m_mainLayout->addStretch(1);

    m_loadingContainer = new QWidget(this);
    m_loadingLayout = new QVBoxLayout(m_loadingContainer);
    m_loadingLayout->setContentsMargins(8, 5, 15, 5);
    m_loadingLayout->setSpacing(0);

    m_loadingFileLabel = new AnimatedLoadingLabel("Initializing...", m_loadingContainer);
    m_loadingFileLabel->setObjectName("loadingFileLabel");
    m_loadingLayout->addWidget(m_loadingFileLabel);
    m_loadingFileLabel->hide();

    m_mainLayout->addWidget(m_loadingContainer);

    m_loadingProgressBar = new QProgressBar(this);
    m_loadingProgressBar->setRange(0, m_totalProgressTasks);
    m_loadingProgressBar->setValue(0);
    m_loadingProgressBar->setTextVisible(false);
    m_loadingProgressBar->hide();
    m_mainLayout->addWidget(m_loadingProgressBar);

    m_exitButton = new ShiningButton("X", this);
    m_exitButton->setObjectName("exitButton");
    m_exitButton->setToolTip(tr("Exit Application"));
    connect(m_exitButton, &QPushButton::clicked, this, &QWidget::close);
    m_exitButton->setFixedSize(60, 60);
    m_exitButton->show();

    setLayout(m_mainLayout);
}

void SplashScreen::start_actual_loading()
{
    if (m_exitButton) m_exitButton->hide();
    if (m_loadingFileLabel) m_loadingFileLabel->show();
    if (m_loadingProgressBar) m_loadingProgressBar->show();

    if (m_thread) {
        qWarning() << "Warning: Loading thread already exists. Aborting new start.";
        return;
    }

    m_thread = new QThread(this);
    m_worker = new LoadingWorker(m_loadingTasks);
    m_worker->moveToThread(m_thread);

    connect(m_worker, &LoadingWorker::task_started, this, &SplashScreen::update_status_text);
    connect(m_worker, &LoadingWorker::progress_updated, this, &SplashScreen::update_progress);
    connect(m_worker, &LoadingWorker::loading_complete, this, &SplashScreen::handle_loading_complete);
    connect(m_worker, &LoadingWorker::loading_error, this, &SplashScreen::handle_loading_error);
    connect(m_thread, &QThread::started, m_worker, &LoadingWorker::run);
    connect(m_worker, &LoadingWorker::loading_complete, this, &SplashScreen::_cleanup_thread);
    connect(m_worker, &LoadingWorker::loading_error, this, &SplashScreen::_cleanup_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    qDebug() << "Starting loading thread...";
    m_thread->start();
}

void SplashScreen::update_status_text(const QString &user_msg, const QString &detail_msg)
{
    QString display_text = detail_msg.isEmpty() ? user_msg : QString("%1 | %2").arg(user_msg, detail_msg);
    if (m_loadingFileLabel) m_loadingFileLabel->setText(display_text);
}

void SplashScreen::update_progress(int value)
{
    if (m_loadingProgressBar) m_loadingProgressBar->setValue(value);
}

void SplashScreen::handle_loading_complete(
    const QString &main_window_class_placeholder,
    const QVariantMap &project_data,
    const QVariantMap &images)
{
    qDebug() << "Worker finished successfully in SplashScreen.";
    m_loadSuccessful = true;
    if (m_loadingFileLabel) {
        m_loadingFileLabel->stop_animation();
        m_loadingFileLabel->setText(tr("Ready."));
    }
    if (m_loadingProgressBar) m_loadingProgressBar->setValue(m_totalProgressTasks);

    QTimer::singleShot(m_splashDurationAfterLoadMs, this, [this, main_window_class_placeholder, project_data, images]() {
        _finish_and_close(main_window_class_placeholder, project_data, images);
    });
}

void SplashScreen::handle_loading_error(const QString &error_context, const QString &error_message)
{
    qDebug() << "Worker reported error in SplashScreen:" << error_context << "-" << error_message;
    m_loadSuccessful = false;
    if (m_loadingFileLabel) m_loadingFileLabel->stop_animation();

    QString full_error = tr("Application failed to load while %1.\n\nDetails: %2")
                             .arg(error_context, error_message);
    QMessageBox::critical(this, tr("Loading Error"), full_error);
    emit loading_failed(full_error);
    close();
}

void SplashScreen::_cleanup_thread()
{
    qDebug() << "Cleaning up loading thread from SplashScreen...";
    if (m_thread) {
        if (m_thread->isRunning()) {
            m_thread->requestInterruption();
            m_thread->quit();
            if (!m_thread->wait(1500)) {
                qWarning() << "Warning: Loading thread did not quit gracefully, terminating.";
                m_thread->terminate();
                m_thread->wait();
            }
        }
        m_thread->deleteLater();
        m_thread = nullptr;
    }
    qDebug() << "Loading thread cleanup attempt finished from SplashScreen.";
}

void SplashScreen::_finish_and_close(
    const QString &main_window_class_placeholder,
    const QVariantMap &project_data,
    const QVariantMap &images)
{
    if (m_loadSuccessful) {
        qDebug() << "Emitting loading_finished signal from SplashScreen.";
        emit loading_finished(main_window_class_placeholder, project_data, images);
    }
    close();
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!m_backgroundPixmap.isNull()) {
        QPixmap scaledPixmap = m_backgroundPixmap.scaled(this->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        QPoint point((this->width() - scaledPixmap.width()) / 2, (this->height() - scaledPixmap.height()) / 2);
        painter.drawPixmap(point, scaledPixmap);
    } else {
        painter.fillRect(this->rect(), QColor("#1b1b1b"));
    }

    int padding = 10;
    QFont textFont("Arial", 9);
    painter.setFont(textFont);
    painter.setPen(QColor("#DDDDDD"));
    QFontMetrics metrics(textFont);
    int lineHeight = metrics.height();
    int currentY = padding + lineHeight;

    QString versionText = "v1.0.0";
    painter.drawText(padding, currentY, versionText);

    currentY += lineHeight + 2;
    QString nxtlvlText = "NXTLVLTECH 2023/2025";
    painter.drawText(padding, currentY, nxtlvlText);
}

void SplashScreen::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_exitButton) {
        int padding = 10;
        m_exitButton->move(this->width() - m_exitButton->width() - padding, padding);
    }
}

void SplashScreen::showEvent(QShowEvent *event)
{
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        move((screenGeometry.width() - width()) / 2, (screenGeometry.height() - height()) / 2);
    }
    
    if (m_exitButton) {
         int padding = 10;
         m_exitButton->move(this->width() - m_exitButton->width() - padding, padding);
    }

    QWidget::showEvent(event);
}

void SplashScreen::closeEvent(QCloseEvent *event)
{
    qDebug() << "SplashScreen closeEvent called.";
    if(m_initialDelayTimer) m_initialDelayTimer->stop();
    _cleanup_thread();
    if(m_loadingFileLabel) m_loadingFileLabel->stop_animation();
    if(m_exitButton) m_exitButton->stop_animation();
    event->accept();
    QWidget::closeEvent(event);
}