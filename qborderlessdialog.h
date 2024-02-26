#ifndef QBORDERLESSDIALOG_H
#define QBORDERLESSDIALOG_H

#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

class BorderlessDialog : public QDialog
{
public:
    BorderlessDialog(QImage image, QWidget *parent = nullptr);

    int lastKey = 0;

protected:
    void mousePressEvent(QMouseEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;
};

#endif // QBORDERLESSDIALOG_H
