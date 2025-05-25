#ifndef SHININGBUTTON_H
#define SHININGBUTTON_H

#include <QPushButton>
#include <QPropertyAnimation>
#include <QLinearGradient> // For the shine gradient

// Forward declarations
class QPaintEvent;
class QEnterEvent; // Changed from QHoverEvent as Python uses enterEvent/leaveEvent
class QMouseEvent; // For leaveEvent, which is a QMouseEvent in C++ for QWidget, but QPushButton has specific leaveEvent(QEvent*)

class ShiningButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(float shinePosition READ shinePosition WRITE setShinePosition)

public:
    explicit ShiningButton(QWidget *parent = nullptr);
    explicit ShiningButton(const QString &text, QWidget *parent = nullptr);
    ~ShiningButton() override;

    float shinePosition() const;
    void setShinePosition(float position);

    // Public methods to control animation if needed externally, though typically internal
    Q_INVOKABLE void start_animation();
    Q_INVOKABLE void stop_animation();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override; // QEnterEvent is for Qt::WA_Hover
    void leaveEvent(QEvent *event) override;     // QEvent for generic leave

private:
    void initAnimation();

    float m_shinePosition;
    bool m_hovering;
    QPropertyAnimation *m_animation;
    QLinearGradient m_shineGradient; // Store the gradient as a member
};

#endif // SHININGBUTTON_H