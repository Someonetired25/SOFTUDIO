#include "framelessdialogbase.h" // This connects the header to the source file
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QApplication> 

// For interactive widget checks
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QCheckBox>
#include <QRadioButton>
#include <QGroupBox>
#include <QProgressBar>
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QScrollBar> 

FramelessDialogBase::FramelessDialogBase(QWidget *parent)
    : QDialog(parent),
      m_isDragging(false),
      m_borderColor(QColor("#404040")),      // Default border color
      m_backgroundColor(QColor("#1f2022")),  // Default background color
      m_borderRadius(5)                       // Default border radius
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                   Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void FramelessDialogBase::setBorderColor(const QColor &color)
{
    m_borderColor = color;
    update(); 
}

void FramelessDialogBase::setBackgroundColor(const QColor &color)
{
    m_backgroundColor = color;
    update(); 
}

void FramelessDialogBase::setBorderRadius(int radius)
{
    m_borderRadius = radius > 0 ? radius : 0;
    update(); 
}

bool FramelessDialogBase::isWidgetInteractive(QWidget *widget) const
{
    if (!widget) {
        return false;
    }

    if (qobject_cast<QPushButton*>(widget) ||
        qobject_cast<QLineEdit*>(widget) ||
        qobject_cast<QListWidget*>(widget) || 
        qobject_cast<QCheckBox*>(widget) ||
        qobject_cast<QRadioButton*>(widget) ||
        qobject_cast<QGroupBox*>(widget) || 
        qobject_cast<QProgressBar*>(widget) || 
        qobject_cast<QAbstractItemView*>(widget) || 
        qobject_cast<QDialogButtonBox*>(widget) ||
        qobject_cast<QTableWidget*>(widget) ||
        qobject_cast<QScrollBar*>(widget)) { 
        return true;
    }

    QWidget *parent = widget->parentWidget();
    while(parent) {
        if (qobject_cast<QListWidget*>(parent) || qobject_cast<QTableWidget*>(parent)) {
            return true;
        }
        if (parent->metaObject()->className() == QStringLiteral("QDialogButtonBox")) {
            return true;
        }
        parent = parent->parentWidget();
    }

    return false;
}


void FramelessDialogBase::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *clickedWidget = childAt(event->position().toPoint());
        if (isWidgetInteractive(clickedWidget)) {
            QDialog::mousePressEvent(event);
        } else {
            m_isDragging = true;
            m_dragStartPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    } else {
        QDialog::mousePressEvent(event);
    }
}

void FramelessDialogBase::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && m_isDragging) {
        move(event->globalPosition().toPoint() - m_dragStartPosition);
        event->accept();
    } else {
        QDialog::mouseMoveEvent(event);
    }
}

void FramelessDialogBase::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        event->accept();
    }
    QDialog::mouseReleaseEvent(event);
}

void FramelessDialogBase::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event); 

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing); 

    painter.setBrush(m_backgroundColor);
    painter.setPen(Qt::NoPen); 
    painter.drawRoundedRect(this->rect(), m_borderRadius, m_borderRadius);

    int penWidth = 1; 
    QRectF borderRect(this->rect().x() + penWidth / 2.0,
                      this->rect().y() + penWidth / 2.0,
                      this->width() - penWidth,
                      this->height() - penWidth);

    painter.setBrush(Qt::NoBrush); 
    QPen borderPen(m_borderColor, penWidth);
    painter.setPen(borderPen);
    painter.drawRoundedRect(borderRect, m_borderRadius, m_borderRadius);
}