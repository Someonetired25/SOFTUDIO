#ifndef FRAMELESSDIALOGBASE_H
#define FRAMELESSDIALOGBASE_H

#include <QDialog>
#include <QPoint>
#include <QColor>

// Forward declarations
class QMouseEvent;
class QPaintEvent;
class QWidget; 

class FramelessDialogBase : public QDialog
{
    Q_OBJECT

public:
    explicit FramelessDialogBase(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

    // Allow derived classes to customize colors and radius if needed
    void setBorderColor(const QColor &color);
    void setBackgroundColor(const QColor &color);
    void setBorderRadius(int radius);

private:
    bool m_isDragging;
    QPoint m_dragStartPosition;

    QColor m_borderColor;
    QColor m_backgroundColor;
    int m_borderRadius;

    bool isWidgetInteractive(QWidget *widget) const;
};

#endif // FRAMELESSDIALOGBASE_H