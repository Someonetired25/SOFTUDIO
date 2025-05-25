#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>
#include <QPixmap>
#include <QList>
#include <QVariantMap>

class QVBoxLayout;
class AnimatedLoadingLabel;
class QProgressBar;
class ShiningButton;
class QTimer;
class LoadingWorker;
class QThread;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QCloseEvent;
struct TaskDefinition;

class SplashScreen : public QWidget
{
    Q_OBJECT

public:
    explicit SplashScreen(int splashDurationAfterLoadMs = 1000, QWidget *parent = nullptr);
    ~SplashScreen() override;

signals:
    void loading_finished(const QString &main_window_class_placeholder,
                          const QVariantMap &project_data,
                          const QVariantMap &images);
    void loading_failed(const QString &error_msg);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void start_actual_loading();
    void update_status_text(const QString &user_msg, const QString &detail_msg);
    void update_progress(int value);
    void handle_loading_complete(const QString &main_window_class_placeholder,
                                 const QVariantMap &project_data,
                                 const QVariantMap &images);
    void handle_loading_error(const QString &error_context, const QString &error_message);
    void _cleanup_thread();
    void _finish_and_close(const QString &main_window_class_placeholder,
                           const QVariantMap &project_data,
                           const QVariantMap &images);

private:
    void setup_ui();
    void setup_icon();
    QString _get_asset_path(const QString &relative_path) const;

    int m_splashDurationAfterLoadMs;
    bool m_loadSuccessful;

    QPixmap m_backgroundPixmap;
    AnimatedLoadingLabel *m_loadingFileLabel;
    QProgressBar *m_loadingProgressBar;
    ShiningButton *m_exitButton;
    QWidget *m_loadingContainer;
    QVBoxLayout *m_mainLayout;
    QVBoxLayout *m_loadingLayout;

    QTimer *m_initialDelayTimer;

    QThread *m_thread;
    LoadingWorker *m_worker;
    QList<TaskDefinition> m_loadingTasks;
    int m_totalProgressTasks;
};

#endif // SPLASHSCREEN_H