#include "qborderlessdialog.h"

#include <QLabel>

BorderlessDialog::BorderlessDialog(QImage image, QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *label = new QLabel();
    label->setPixmap(QPixmap::fromImage(image));

    layout->addWidget(label);
}
void BorderlessDialog::mousePressEvent(QMouseEvent *event)
{
    // Close the window on mouse press
    Q_UNUSED(event);
    accept();
}
void BorderlessDialog::keyPressEvent(QKeyEvent *event)
{
    // Close the window on key press
    lastKey = event->key();
    Q_UNUSED(event);

    accept();
}
