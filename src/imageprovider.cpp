/*
    Touch'n'learn - Fun and easy mobile lessons for kids
    Copyright (C) 2010 by Alessandro Portale
    http://touchandlearn.sourceforge.net

    This file is part of Touch'n'learn

    Touch'n'learn is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Touch'n'learn is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Touch'n'learn; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "imageprovider.h"
#include "qglobal.h"
#include <math.h>
#include <QSvgRenderer>
#include <QPainter>
#include <QtDebug>

const QString frameString = QLatin1String("frame");
const QString buttonString = QLatin1String("button");
const QString clockBackgroundString = QLatin1String("background");
static QString dataPath = QLatin1String("data");

Q_GLOBAL_STATIC_WITH_INITIALIZER(QSvgRenderer, designRenderer, {
    x->load(dataPath + QLatin1String("/design.svg"));
});

Q_GLOBAL_STATIC_WITH_INITIALIZER(QSvgRenderer, objectRenderer, {
    x->load(dataPath + QLatin1String("/objects.svg"));
});

Q_GLOBAL_STATIC_WITH_INITIALIZER(QSvgRenderer, countablesRenderer, {
    x->load(dataPath + QLatin1String("/countables.svg"));
});

Q_GLOBAL_STATIC_WITH_INITIALIZER(QSvgRenderer, clocksRenderer, {
    x->load(dataPath + QLatin1String("/clocks.svg"));
});

Q_GLOBAL_STATIC_WITH_INITIALIZER(QSvgRenderer, notesRenderer, {
    x->load(dataPath + QLatin1String("/notes.svg"));
});

Q_GLOBAL_STATIC_WITH_INITIALIZER(QSvgRenderer, lessonIconsRenderer, {
    x->load(dataPath + QLatin1String("/lessonicons.svg"));
});

struct ElementVariations
{
    QStringList elementIds;
    qreal widthToHeightRatio;
    inline bool operator<(const ElementVariations &other) const
    {
        return widthToHeightRatio < other.widthToHeightRatio;
    }
};

typedef QList<ElementVariations> ElementVariationList;

ElementVariationList elementsWithSizes(const QString &elementBase)
{
    ElementVariationList result;
    QSvgRenderer *renderer = designRenderer();
    ElementVariations element;
    element.widthToHeightRatio = -1;
    for (int i = 1; ; i++) {
        const QString id = elementBase + QLatin1Char('_') + QString::number(i);
        if (!renderer->elementExists(id))
            break;
        const QSizeF size = renderer->boundsOnElement(id).size();
        const qreal widthToHeightRatio = size.width() / size.height();
        if (!qFuzzyCompare(widthToHeightRatio, element.widthToHeightRatio)) {
            if (element.widthToHeightRatio > 0) // Check, is it is the first element
                result.append(element);
            element.widthToHeightRatio = widthToHeightRatio;
            element.elementIds.clear();
        }
        element.elementIds.append(id);
    }
    if (!element.elementIds.isEmpty())
        result.append(element);
    qSort(result);
    return result;
}

Q_GLOBAL_STATIC_WITH_INITIALIZER(ElementVariationList, buttonVariations, {
    x->append(elementsWithSizes(buttonString));
});

Q_GLOBAL_STATIC_WITH_INITIALIZER(ElementVariationList, frameVariations, {
    x->append(elementsWithSizes(frameString));
});

ImageProvider::ImageProvider()
    : QDeclarativeImageProvider(QDeclarativeImageProvider::Pixmap)
{
}

inline static QPixmap quantity(int quantity, const QString &item, QSize *size, const QSize &requestedSize)
{
    QSvgRenderer *renderer = countablesRenderer();
    const int columns = ceil(sqrt(qreal(quantity)));
    const int rows = ceil(quantity / qreal(columns));
    const int columnsInLastRow = quantity % columns == 0 ? columns : quantity % columns;
    const int itemSize = qMin((requestedSize.width() / qMax(3, columns)), (requestedSize.height() / qMax(3, rows)));
    const QSize resultSize(itemSize * columns, itemSize * rows);
    QPixmap result(resultSize);
    result.fill(Qt::transparent);
    QPainter p(&result);
    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            if (columns * row + column >= quantity)
                break;
            const QString itemId = item + QLatin1Char('_') + QString::number((qrand() % 8 + 1));
            const QRect itemRect(column * itemSize + (row == rows-1 ? (columns - columnsInLastRow) * itemSize / 2 : 0),
                                 row * itemSize, itemSize, itemSize);
            renderer->render(&p, itemId, itemRect);
        }
    }
    if (size)
        *size = resultSize;
    return result;
}

inline static int clockVariationsCount()
{
    int count = 0;
    QSvgRenderer *renderer = clocksRenderer();
    const QString elementIdBase = clockBackgroundString + QLatin1Char('_');
    Q_FOREVER {
        const QString elementId = elementIdBase + QString::number(count + 1);
        if (renderer->elementExists(elementId))
            count++;
        else
            break;
    }
    return count;
}

inline static void renderIndicator(const QString &indicatorId, int rotation, const QRectF &background,
                                   qreal scaleFactor, QSvgRenderer *renderer, QPainter *p)
{
    const QPointF bgCenter = background.center();
    QTransform transform;
    transform
            .scale(scaleFactor, scaleFactor)
            .translate(bgCenter.x() - background.x(), bgCenter.y() - background.y())
            .rotate(rotation)
            .translate(-bgCenter.x(), -bgCenter.y());
    p->setTransform(transform);
    renderer->render(p, indicatorId, renderer->boundsOnElement(indicatorId));
}

inline static QPixmap clock(int hour, int minute, int variation, QSize *size, const QSize &requestedSize)
{
    const static int variationsCount = clockVariationsCount();
    const int actualVariation = (variation % variationsCount) + 1;
    QSvgRenderer *renderer = clocksRenderer();
    const QString variationNumber = QLatin1Char('_') + QString::number(actualVariation);
    const QString backgroundElementId = clockBackgroundString + variationNumber;
    const QRectF backgroundRect = renderer->boundsOnElement(backgroundElementId);
    QSize pixmapSize = backgroundRect.size().toSize();
    if (size)
        *size = pixmapSize;
    pixmapSize.scale(requestedSize, Qt::KeepAspectRatio);
    QPixmap pixmap(pixmapSize);
    if (pixmap.isNull())
        qDebug() << "****************** clock pixmap is NULL! Variation:" << variation;
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    const qreal scaleFactor = pixmapSize.width() / backgroundRect.width();
    QTransform mainTransform;
    mainTransform
            .scale(scaleFactor, scaleFactor)
            .translate(-backgroundRect.left(), -backgroundRect.top());
    p.setTransform(mainTransform);
    renderer->render(&p, backgroundElementId, backgroundRect);

    const int minuteRotation = (minute * 6) % 360;
    renderIndicator(QLatin1String("minute") + variationNumber, minuteRotation,
                    backgroundRect, scaleFactor, renderer, &p);

    const int hoursSkew = 6; // Initial position of hour in the SVG is 6
    renderIndicator(QLatin1String("hour") + variationNumber, (((hour + hoursSkew) * 360 + minuteRotation) / 12) % 360,
                    backgroundRect, scaleFactor, renderer, &p);

    const QString foregroundElementId = QLatin1String("foreground") + variationNumber;
    if (renderer->elementExists(foregroundElementId)) {
        p.setTransform(mainTransform);
        renderer->render(&p, foregroundElementId, renderer->boundsOnElement(foregroundElementId));
    }
    return pixmap;
}

inline static QPixmap notes(const QStringList &notes, QSize *size, const QSize &requestedSize)
{
    QSvgRenderer *renderer = notesRenderer();
    static const QString clefId = QLatin1String("clef");
    static const QRectF clefRect = renderer->boundsOnElement(clefId);
    static const QString staffLinesId = QLatin1String("stafflines");
    static const QRectF staffLinesOriginalRect = renderer->boundsOnElement(staffLinesId);
    static const qreal clefRightY = clefRect.right() - staffLinesOriginalRect.left();
    static const qreal linesSpacePerNote = clefRect.width() * 1.75;
    const qreal linesSpaceForNotes = notes.count() * linesSpacePerNote;
    QRectF pixmapRect = staffLinesOriginalRect;
    pixmapRect.setWidth(clefRightY + linesSpaceForNotes);
    QSize pixmapSize = pixmapRect.size().toSize();
    if (size)
        *size = pixmapSize;
    pixmapSize.scale(requestedSize, Qt::KeepAspectRatio);
    QPixmap pixmap(pixmapSize);
    if (pixmap.isNull())
        qDebug() << "****************** notes pixmap is NULL! Notes:" << notes;
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    const qreal scaleFactor = pixmapSize.width() / pixmapRect.width();
    p.scale(scaleFactor, scaleFactor);
    p.translate(-pixmapRect.topLeft());

    renderer->render(&p, staffLinesId, pixmapRect);
    renderer->render(&p, clefId, clefRect);

    int currentNoteIndex = 0;
    foreach(const QString &currentNote, notes) {
        const QString trimmedNote = currentNote.trimmed();
        const QString note = trimmedNote.at(0).toLower();
        const QString noteID = QLatin1String("note_") + note;
        QRectF noteRect = renderer->boundsOnElement(noteID);
        const qreal noteCenterX = clefRightY + (currentNoteIndex + 0.125) * linesSpacePerNote + noteRect.width();
        currentNoteIndex++;
        const qreal noteXTranslate = noteCenterX - noteRect.center().x();
        noteRect.translate(noteXTranslate, 0);
        renderer->render(&p, noteID, noteRect);
        if (trimmedNote.length() > 1) {
            static const QString sharpId = QLatin1String("sharp");
            static const QString flatId = QLatin1String("flat");
            static const QRectF noteCHeadRect = renderer->boundsOnElement(QLatin1String("note_c_head"));
            const bool sharp = trimmedNote.endsWith(QLatin1String("sharp"));
            const QString &noteSign = sharp ? sharpId : flatId;
            const QRectF noteHeadRect = renderer->boundsOnElement(QLatin1String("note_") + note + QLatin1String("_head"));
            const QRectF signRect = renderer->boundsOnElement(noteSign)
                    .translated(noteXTranslate, 0)
                    .translated(noteHeadRect.topLeft() - noteCHeadRect.topLeft());
            renderer->render(&p, noteSign, signRect);
        }
    }

    return pixmap;
}

inline static QPixmap renderedSvgElement(const QString &elementId, QSvgRenderer *renderer, Qt::AspectRatioMode aspectRatioMode,
                                         QSize *size, const QSize &requestedSize)
{
    const QString rectId = elementId + QLatin1String("_rect");
    const QRectF rect = renderer->boundsOnElement(renderer->elementExists(rectId) ? rectId : elementId);
    if (rect.width() < 1 || rect.height() < 1) {
        qDebug() << "****************** SVG bounding rect is NULL!" << rect << elementId;
        return QPixmap();
    }
    QSize pixmapSize = rect.size().toSize();
    if (size)
        *size = pixmapSize;
    pixmapSize.scale(requestedSize, aspectRatioMode);
    if (pixmapSize.width() < 1 || pixmapSize.height() < 1) {
        qDebug() << "****************** pixmapSize is NULL!" << pixmapSize << requestedSize << elementId;
        return QPixmap();
    }
    QPixmap pixmap(pixmapSize);
    if (pixmap.isNull())
        qDebug() << "****************** pixmap is NULL!" << elementId;
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    renderer->render(&p, elementId, QRect(QPoint(), pixmapSize));
    return pixmap;
}

inline static QPixmap renderedDesignElement(const ElementVariationList *elements, int variation, QSize *size, const QSize &requestedSize)
{
    const qreal requestedRatio = requestedSize.width() / qreal(requestedSize.height());
    const ElementVariations *elementWithNearestRatio = &elements->last();
    foreach (const ElementVariations &element, *elements) {
        if (qAbs(requestedRatio - element.widthToHeightRatio)
                < qAbs(requestedRatio - elementWithNearestRatio->widthToHeightRatio)) {
            elementWithNearestRatio = &element;
        } else if (element.widthToHeightRatio > elementWithNearestRatio->widthToHeightRatio) {
            break;
        }
    }
    return renderedSvgElement(elementWithNearestRatio->elementIds.at(variation % elementWithNearestRatio->elementIds.count()),
                              designRenderer(), Qt::IgnoreAspectRatio, size, requestedSize);
}

inline static QPixmap renderedLessonIcon(const QString &iconId, int buttonVariation, QSize *size, const QSize &requestedSize)
{
    QPixmap icon(requestedSize);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    QSvgRenderer *renderer = lessonIconsRenderer();
    const QRectF iconRectOriginal = renderer->boundsOnElement(iconId);
    QSizeF iconSize = iconRectOriginal.size();
    iconSize.scale(requestedSize, Qt::KeepAspectRatio);
    QRectF iconRect(QPointF(), iconSize);
    if (requestedSize.height() > requestedSize.width())
        iconRect.moveBottom(requestedSize.height());
    else
        iconRect.moveTop((requestedSize.height() - iconSize.height()) / 2);
    renderer->render(&p, iconId, iconRect);
    const QPixmap button = renderedDesignElement(buttonVariations(), buttonVariation, size, requestedSize);
    p.drawPixmap(QPointF(), button);
    return icon;
}

QPixmap ImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QStringList idSegments = id.split(QLatin1Char('/'));
    if (requestedSize.width() < 1 && requestedSize.height() < 1) {
        qDebug() << "****************** requestedSize is NULL!" << requestedSize << id;
        return QPixmap();
    }
    if (idSegments.count() < 2) {
        qDebug() << "Not enough parameters for the image provider: " << id;
        return QPixmap();
    }
    const QString &elementId = idSegments.at(1);
    if (idSegments.first() == QLatin1String("background")) {
        return renderedSvgElement(elementId, designRenderer(), Qt::KeepAspectRatioByExpanding, size, requestedSize);
    } else if (idSegments.first() == QLatin1String("specialbutton")) {
        return renderedSvgElement(elementId, designRenderer(), Qt::IgnoreAspectRatio, size, requestedSize);
    } else if (idSegments.first() == buttonString) {
        return renderedDesignElement(buttonVariations(), elementId.toInt(), size, requestedSize);
    } else if (idSegments.first() == frameString) {
        return renderedDesignElement(frameVariations(), 0, size, requestedSize);
    } else if (idSegments.first() == QLatin1String("object")) {
        return renderedSvgElement(elementId, objectRenderer(), Qt::KeepAspectRatio, size, requestedSize);
    } else if (idSegments.first() == QLatin1String("clock")) {
        if (idSegments.count() != 4) {
            qDebug() << "Wrong number of parameters for clock images:" << id;
            return QPixmap();
        }
        return clock(idSegments.at(1).toInt(), idSegments.at(2).toInt(), idSegments.at(3).toInt(), size, requestedSize);
    } else if (idSegments.first() == QLatin1String("notes")) {
        return notes(elementId.split(QLatin1Char(','), QString::SkipEmptyParts), size, requestedSize);
    } else if (idSegments.first() == QLatin1String("quantity")) {
        if (idSegments.count() != 3) {
            qDebug() << "Wrong number of parameters for quantity images:" << id;
            return QPixmap();
        }
        return quantity(idSegments.at(1).toInt(), idSegments.at(2), size, requestedSize);
    } else if (idSegments.first() == QLatin1String("lessonicon")) {
        if (idSegments.count() != 3) {
            qDebug() << "Wrong number of parameters for lessonicon:" << id;
            return QPixmap();
        }
        return renderedLessonIcon(idSegments.at(1), idSegments.at(2).toInt(), size, requestedSize);
    }
    return QPixmap();
}

void ImageProvider::setDataPath(const QString &path)
{
    dataPath = path;
}
