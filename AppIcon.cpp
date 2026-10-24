#include "AppIcon.h"

#include <QPainter>
#include <QPixmap>

namespace {

void drawWindow(QPainter &painter, const QRectF &rect, const QColor &frame, const QColor &body)
{
    const qreal titleHeight = rect.height() * 0.28;
    painter.setPen(Qt::NoPen);
    painter.setBrush(body);
    painter.drawRect(rect);
    painter.setBrush(frame);
    painter.drawRect(QRectF(rect.topLeft(), QSizeF(rect.width(), titleHeight)));
    painter.setPen(QPen(frame, qMax(1.0, rect.width() / 16.0)));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect);
}

QPixmap drawIcon(int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal s = size;
    drawWindow(painter, QRectF(s * 0.06, s * 0.08, s * 0.62, s * 0.52),
               QColor(0x2b, 0x57, 0x9a), QColor(0xdd, 0xe8, 0xf6));
    drawWindow(painter, QRectF(s * 0.32, s * 0.40, s * 0.62, s * 0.52),
               QColor(0x1e, 0x88, 0x5a), QColor(0xe2, 0xf4, 0xea));
    return pixmap;
}

} // namespace

QIcon appIcon()
{
    QIcon icon;
    for (int size : {16, 20, 24, 32, 48, 64, 128, 256})
        icon.addPixmap(drawIcon(size));
    return icon;
}
