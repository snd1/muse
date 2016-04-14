#include <QPainter>
#include <QPaintEvent>

#include "colorframe.h"

ColorFrame::ColorFrame(QWidget *parent) :
    QWidget(parent)
{
}

void ColorFrame::paintEvent(QPaintEvent *e)
{
    QRect r(e->rect());
    QPainter p(this);
    p.fillRect(r, _color);
}

void ColorFrame::mousePressEvent(QMouseEvent* e)
{
  e->accept();
  emit clicked();
}
