#include "animatedloadinglabel.h"
#include "splash_constants.h" // For animation parameters and colors

#include <QPainter>
#include <QFontMetrics>
#include <QShowEvent>
#include <QHideEvent>
#include <QEasingCurve>
#include <QPoint>

AnimatedLoadingLabel::AnimatedLoadingLabel(QWidget *parent, Qt::WindowFlags f)
    : QLabel(parent, f),
      m_shinePosition(0.0f),
      m_text(""), // Initialize m_text
      m_animation(new QPropertyAnimation(this, "shinePosition", this)) // Parent 'this' to m_animation
{
    m_animation->setDuration(SHINE_ANIMATION_DURATION_MS);
    m_animation->setStartValue(0.0f - static_cast<float>(SHINE_WIDTH_FRACTION) / 2.0f);
    m_animation->setEndValue(1.0f + static_cast<float>(SHINE_WIDTH_FRACTION) / 2.0f);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
    m_animation->setLoopCount(-1); // Infinite loop
}

AnimatedLoadingLabel::AnimatedLoadingLabel(const QString &text, QWidget *parent, Qt::WindowFlags f)
    : QLabel(text, parent, f), // Pass text to base QLabel constructor
      m_shinePosition(0.0f),
      m_text(text), // Initialize m_text with the provided text
      m_animation(new QPropertyAnimation(this, "shinePosition", this))
{
    m_animation->setDuration(SHINE_ANIMATION_DURATION_MS);
    m_animation->setStartValue(0.0f - static_cast<float>(SHINE_WIDTH_FRACTION) / 2.0f);
    m_animation->setEndValue(1.0f + static_cast<float>(SHINE_WIDTH_FRACTION) / 2.0f);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
    m_animation->setLoopCount(-1);
    // Note: QLabel::setText is already called by the base constructor.
    // If you need custom logic beyond QLabel's setText, you'd call your own setText here.
}

AnimatedLoadingLabel::~AnimatedLoadingLabel()
{
    // m_animation is a child of 'this', Qt's parent-child system will delete it.
    // If it weren't parented, you'd do: delete m_animation;
}


float AnimatedLoadingLabel::shinePosition() const
{
    return m_shinePosition;
}

void AnimatedLoadingLabel::setShinePosition(float position)
{
    if (m_shinePosition != position) {
        m_shinePosition = position;
        update(); // Trigger a repaint when the shinePosition changes
    }
}

// Override QLabel's setText to ensure our internal m_text is updated
// and we can control repaint if necessary.
void AnimatedLoadingLabel::setText(const QString &text)
{
    if (m_text != text) {
        m_text = text;
        QLabel::setText(text); // Call base class setText to actually set the label's text
        update(); // Ensure repaint if text content changes
    }
}

// Override QLabel's text() to return our internal copy if needed,
// or just rely on base class's text(). Here, m_text is mainly for comparison.
QString AnimatedLoadingLabel::text() const
{
    return QLabel::text(); // Or return m_text, consistent with how setText works
}

void AnimatedLoadingLabel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event); // Mark as unused if not directly used

    const QString currentText = QLabel::text(); // Get text from QLabel base
    if (currentText.isEmpty()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QFont currentFont = font(); // Get font from QLabel base
    QFontMetrics metrics(currentFont);

    #if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        const int textWidth = metrics.horizontalAdvance(currentText);
    #else
        const int textWidth = metrics.width(currentText);
    #endif
    const int textHeight = metrics.height();

    QRect currentContentsRect = contentsRect();
    int textY = currentContentsRect.y() + (currentContentsRect.height() - textHeight) / 2 + metrics.ascent();
    int textX = currentContentsRect.x(); // Default to left alignment within contentsRect

    // Draw base text
    painter.setFont(currentFont);
    painter.setPen(BASE_TEXT_COLOR); // From splash_constants.h
    painter.drawText(currentContentsRect, alignment(), currentText); // Use QLabel's alignment

    // Draw shine effect if animation is running
    if (textWidth > 0 && m_animation->state() == QPropertyAnimation::Running) {
        int shineWidthPixels = static_cast<int>(textWidth * SHINE_WIDTH_FRACTION); // From splash_constants.h
        
        // Calculate actual text bounding box based on alignment to correctly position shine
        QRect textBoundingRect = metrics.boundingRect(currentContentsRect, alignment(), currentText);
        int actualTextX = textBoundingRect.x(); // X where the text actually starts drawing

        int shineCenterX = actualTextX + static_cast<int>(textWidth * m_shinePosition);
        int shineStartX = shineCenterX - shineWidthPixels / 2;

        // Clip rectangle should be relative to where the text is actually drawn
        QRect clipRect(shineStartX, textBoundingRect.y(), shineWidthPixels, textBoundingRect.height());

        painter.save();
        painter.setClipRect(clipRect);
        painter.setPen(SHINE_TEXT_COLOR); // From splash_constants.h
        // Redraw the text, but it will only be visible within the clipRect with the shine color
        painter.drawText(currentContentsRect, alignment(), currentText);
        painter.restore();
    }
}


void AnimatedLoadingLabel::start_animation()
{
    if (m_animation->state() != QPropertyAnimation::Running) {
        m_animation->start();
    }
}

void AnimatedLoadingLabel::stop_animation()
{
    if (m_animation->state() == QPropertyAnimation::Running) {
        m_animation->stop();
    }
}

void AnimatedLoadingLabel::showEvent(QShowEvent *event)
{
    start_animation();
    QLabel::showEvent(event); // Call base class implementation
}

void AnimatedLoadingLabel::hideEvent(QHideEvent *event)
{
    stop_animation();
    QLabel::hideEvent(event); // Call base class implementation
}