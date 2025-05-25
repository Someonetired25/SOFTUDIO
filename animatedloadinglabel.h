#ifndef ANIMATEDLOADINGLABEL_H
#define ANIMATEDLOADINGLABEL_H

#include <QLabel>
#include <QPropertyAnimation>
#include <QColor> // Required for QColor members if not using global constants directly

// Forward declarations
class QPaintEvent;
class QShowEvent;
class QHideEvent;

class AnimatedLoadingLabel : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(float shinePosition READ shinePosition WRITE setShinePosition)

public:
    explicit AnimatedLoadingLabel(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    explicit AnimatedLoadingLabel(const QString &text, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~AnimatedLoadingLabel() override;

    void setText(const QString &text); // Override to update internal text and trigger repaint
    QString text() const;              // Override if you store text internally differently

    Q_INVOKABLE void start_animation(); // Make it invokable from meta-system if needed
    Q_INVOKABLE void stop_animation();

    float shinePosition() const;
    void setShinePosition(float position);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    float m_shinePosition;
    QString m_text; // Internal storage for text to compare in setText
    QPropertyAnimation *m_animation;
};

#endif // ANIMATEDLOADINGLABEL_H