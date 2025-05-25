#include "shiningbutton.h"
#include "splash_constants.h" // For button constants

#include <QPainter>
#include <QFontMetrics>
#include <QEasingCurve>
#include <QEvent> // For QEvent in leaveEvent

ShiningButton::ShiningButton(QWidget *parent)
    : QPushButton(parent),
      m_shinePosition(0.0f),
      m_hovering(false),
      m_animation(new QPropertyAnimation(this, "shinePosition", this))
{
    initAnimation();
}

ShiningButton::ShiningButton(const QString &text, QWidget *parent)
    : QPushButton(text, parent),
      m_shinePosition(0.0f),
      m_hovering(false),
      m_animation(new QPropertyAnimation(this, "shinePosition", this))
{
    initAnimation();
}

ShiningButton::~ShiningButton()
{
    // m_animation is child of 'this', will be auto-deleted
}

void ShiningButton::initAnimation()
{
    setFlat(true); // Make button flat, background is transparent
    setStyleSheet("QPushButton { background-color: transparent; border: none; }");

    m_animation->setDuration(EXIT_BUTTON_SHINE_DURATION_MS);
    m_animation->setStartValue(0.0f - static_cast<float>(EXIT_BUTTON_SHINE_WIDTH_FRACTION) / 2.0f);
    m_animation->setEndValue(1.0f + static_cast<float>(EXIT_BUTTON_SHINE_WIDTH_FRACTION) / 2.0f);
    m_animation->setEasingCurve(QEasingCurve::InOutSine); // Python used InOutSine
    m_animation->setLoopCount(-1); // Infinite loop

    // Initialize the gradient
    m_shineGradient.setStart(0, 0); // Will use ObjectBoundingMode
    m_shineGradient.setFinalStop(1, 0); // Horizontal gradient
    m_shineGradient.setCoordinateMode(QLinearGradient::ObjectBoundingMode);
    m_shineGradient.setColorAt(0.0, EXIT_BUTTON_SHINE_COLOR_START);
    m_shineGradient.setColorAt(0.5, EXIT_BUTTON_SHINE_COLOR_MID);
    m_shineGradient.setColorAt(1.0, EXIT_BUTTON_SHINE_COLOR_END);
}

float ShiningButton::shinePosition() const
{
    return m_shinePosition;
}

void ShiningButton::setShinePosition(float position)
{
    if (m_shinePosition != position) {
        m_shinePosition = position;
        if (m_hovering) { // Only update visually if hovering and animation is meant to be active
            update();
        }
    }
}

void ShiningButton::enterEvent(QEnterEvent *event)
{
    m_hovering = true;
    start_animation();
    QPushButton::enterEvent(event); // Call base implementation
}

void ShiningButton::leaveEvent(QEvent *event)
{
    m_hovering = false;
    stop_animation();
    update(); // Repaint to remove shine if it was partially visible
    QPushButton::leaveEvent(event); // Call base implementation
}

void ShiningButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QString currentText = text(); // Get text from QPushButton base
    QFont currentFont = font();
    QFontMetrics metrics(currentFont);

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    const int textWidth = metrics.horizontalAdvance(currentText);
#else
    const int textWidth = metrics.width(currentText);
#endif
    const int textHeight = metrics.height();

    QRect currentContentsRect = contentsRect(); // Use contentsRect for available drawing area

    // Calculate text position for centering (QPushButton default behavior can be complex,
    // but for a flat button with custom paint, we often center manually).
    int textX = currentContentsRect.x() + (currentContentsRect.width() - textWidth) / 2;
    int textY = currentContentsRect.y() + (currentContentsRect.height() - textHeight) / 2 + metrics.ascent();

    // Draw base text
    painter.setFont(currentFont);
    painter.setPen(EXIT_BUTTON_BASE_COLOR); // From splash_constants.h
    painter.drawText(textX, textY, currentText);

    // Draw shine effect if hovering and animation is running
    if (m_hovering && textWidth > 0 && m_animation->state() == QPropertyAnimation::Running) {
        int shineWidthPixels = static_cast<int>(textWidth * EXIT_BUTTON_SHINE_WIDTH_FRACTION);
        
        // shine_center_x_relative is relative to the start of the text block
        int shineCenterX_withinText = static_cast<int>(textWidth * m_shinePosition); 
        // shine_start_x is the absolute x-coordinate for the clip rect
        int shineStartX_absolute = textX + shineCenterX_withinText - shineWidthPixels / 2;

        // Clip rectangle should be around the text area
        // Python: clip_rect = QRect(shine_start_x, text_y - metrics.ascent(), shine_width_pixels, text_height)
        // text_y - metrics.ascent() is the top of the text.
        QRect clipRect(shineStartX_absolute, textY - metrics.ascent(), shineWidthPixels, textHeight);

        painter.save();
        painter.setClipRect(clipRect);
        
        QBrush gradientBrush(m_shineGradient); // Use the member gradient
        painter.setPen(QPen(gradientBrush, 0)); // Use brush for pen to "fill" text
        painter.drawText(textX, textY, currentText); // Redraw text, clipped and with gradient
        
        painter.restore();
    }
}

void ShiningButton::start_animation()
{
    if (m_animation->state() != QPropertyAnimation::Running) {
        m_animation->start();
    }
}

void ShiningButton::stop_animation()
{
    if (m_animation->state() == QPropertyAnimation::Running) {
        m_animation->stop();
    }
    m_shinePosition = 0.0f; // Reset position as in Python
    // No explicit update() here, leaveEvent calls it.
}