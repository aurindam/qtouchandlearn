/*
    Touch'n'learn - Fun and easy mobile lessons for kids
    Copyright (C) 2010, 2011 by Alessandro Portale
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
#include "QtCore/qglobal.h"
#include <math.h>
#include <QtGui/QPainter>
#include <QtCore/QDebug>
#include <QtSvg/QSvgRenderer>

#define PI 3.14159265

enum DesignElementType {
    DesignElementTypeButton,
    DesignElementTypeFrame
};

const QString frameString = QLatin1String("frame");
const QString buttonString = QLatin1String("button");
const QString idPrefix = QLatin1String("id_");
static QString dataPath = QLatin1String("data/graphics");

Q_GLOBAL_STATIC_WITH_INITIALIZER(QSvgRenderer, designRenderer, {
    x->load(dataPath + QLatin1String("/design.svg"));
})

Q_GLOBAL_STATIC_WITH_INITIALIZER(QSvgRenderer, objectRenderer, {
    x->load(dataPath + QLatin1String("/objects.svg"));
})

Q_GLOBAL_STATIC_WITH_INITIALIZER(QSvgRenderer, countablesRenderer, {
    x->load(dataPath + QLatin1String("/countables.svg"));
})

Q_GLOBAL_STATIC_WITH_INITIALIZER(QSvgRenderer, clocksRenderer, {
    x->load(dataPath + QLatin1String("/clocks.svg"));
})

Q_GLOBAL_STATIC_WITH_INITIALIZER(QSvgRenderer, notesRenderer, {
    x->load(dataPath + QLatin1String("/notes.svg"));
})

Q_GLOBAL_STATIC_WITH_INITIALIZER(QSvgRenderer, lessonIconsRenderer, {
    x->load(dataPath + QLatin1String("/lessonicons.svg"));
})

QImage gradientImage(DesignElementType type)
{
    QSvgRenderer *renderer = designRenderer();
    const QString gradientId = idPrefix + (type == DesignElementTypeButton ? buttonString : frameString) + QLatin1String("gradient");
    Q_ASSERT(renderer->boundsOnElement(gradientId).size().toSize() == QSize(256, 1));
    QImage result(256, 1, QImage::Format_ARGB32);
    result.fill(0);
    QPainter p(&result);
    renderer->render(&p, gradientId, result.rect());
#if 0
    // for debugging
    p.fillRect(0, 0, 16, 1, Qt::red);
    p.fillRect(120, 0, 16, 1, Qt::green);
    p.fillRect(240, 0, 16, 1, Qt::blue);
#endif
    return result;
}

Q_GLOBAL_STATIC_WITH_INITIALIZER(QImage, buttonGradient, {
    *x = gradientImage(DesignElementTypeButton);
})

Q_GLOBAL_STATIC_WITH_INITIALIZER(QImage, frameGradient, {
    *x = gradientImage(DesignElementTypeFrame);
})

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
        if (!renderer->elementExists(idPrefix + id))
            break;
        const QSizeF size = renderer->boundsOnElement(idPrefix + id).size();
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
})

Q_GLOBAL_STATIC_WITH_INITIALIZER(ElementVariationList, frameVariations, {
    x->append(elementsWithSizes(frameString));
})

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
            renderer->render(&p, idPrefix + itemId, itemRect);
        }
    }
    if (size)
        *size = resultSize;
    return result;
}

inline static int variationsCount(const QSvgRenderer *renderer, const QString &baseName)
{
    int count = 0;
    const QString elementIdBase = baseName + QLatin1Char('_');
    Q_FOREVER {
        const QString elementId = elementIdBase + QString::number(count + 1);
        if (renderer->elementExists(idPrefix + elementId))
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
    renderer->render(p, idPrefix + indicatorId, renderer->boundsOnElement(idPrefix + indicatorId));
}

inline static QPixmap clock(int hour, int minute, int variation, QSize *size, const QSize &requestedSize)
{
    QSvgRenderer *renderer = clocksRenderer();
    const static QString clockBackgroundString = QLatin1String("background");
    const static int variationsCnt = variationsCount(renderer, clockBackgroundString);
    const int actualVariation = (variation % variationsCnt) + 1;
    const QString variationNumber = QLatin1Char('_') + QString::number(actualVariation);
    const QString backgroundElementId = clockBackgroundString + variationNumber;
    const QRectF backgroundRect = renderer->boundsOnElement(idPrefix + backgroundElementId);
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
    renderer->render(&p, idPrefix + backgroundElementId, backgroundRect);

    const int minuteRotation = (minute * 6) % 360;
    renderIndicator(QLatin1String("minute") + variationNumber, minuteRotation,
                    backgroundRect, scaleFactor, renderer, &p);

    const int hoursSkew = 6; // Initial position of hour in the SVG is 6
    renderIndicator(QLatin1String("hour") + variationNumber, (((hour + hoursSkew) * 360 + minuteRotation) / 12) % 360,
                    backgroundRect, scaleFactor, renderer, &p);

    const QString foregroundElementId = QLatin1String("foreground") + variationNumber;
    if (renderer->elementExists(idPrefix + foregroundElementId)) {
        p.setTransform(mainTransform);
        renderer->render(&p, idPrefix + foregroundElementId, renderer->boundsOnElement(idPrefix + foregroundElementId));
    }
    return pixmap;
}

inline static QPixmap notes(const QStringList &notes, QSize *size, const QSize &requestedSize)
{
    QSvgRenderer *renderer = notesRenderer();
    static const QString clefId = QLatin1String("clef");
    static const QRectF clefRect = renderer->boundsOnElement(idPrefix + clefId);
    static const QString staffLinesId = QLatin1String("stafflines");
    static const QRectF staffLinesOriginalRect = renderer->boundsOnElement(idPrefix + staffLinesId);
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

    renderer->render(&p, idPrefix + staffLinesId, pixmapRect);
    renderer->render(&p, idPrefix + clefId, clefRect);

    int currentNoteIndex = 0;
    foreach(const QString &currentNote, notes) {
        const QString trimmedNote = currentNote.trimmed();
        const QString note = trimmedNote.at(0).toLower();
        const QString noteID = QLatin1String("note_") + note;
        QRectF noteRect = renderer->boundsOnElement(idPrefix + noteID);
        const qreal noteCenterX = clefRightY + (currentNoteIndex + 0.125) * linesSpacePerNote + noteRect.width();
        currentNoteIndex++;
        const qreal noteXTranslate = noteCenterX - noteRect.center().x();
        noteRect.translate(noteXTranslate, 0);
        renderer->render(&p, idPrefix + noteID, noteRect);
        if (trimmedNote.length() > 1) {
            static const QString sharpId = QLatin1String("sharp");
            static const QString flatId = QLatin1String("flat");
            static const QRectF noteCHeadRect = renderer->boundsOnElement(idPrefix + QLatin1String("note_c_head"));
            const bool sharp = trimmedNote.endsWith(QLatin1String("sharp"));
            const QString &noteSign = sharp ? sharpId : flatId;
            const QRectF noteHeadRect = renderer->boundsOnElement(idPrefix + QLatin1String("note_") + note + QLatin1String("_head"));
            const QRectF signRect = renderer->boundsOnElement(idPrefix + noteSign)
                    .translated(noteXTranslate, 0)
                    .translated(noteHeadRect.topLeft() - noteCHeadRect.topLeft());
            renderer->render(&p, idPrefix + noteSign, signRect);
        }
    }

    return pixmap;
}

inline static QPixmap renderedSvgElement(const QString &elementId, QSvgRenderer *renderer, Qt::AspectRatioMode aspectRatioMode,
                                         QSize *size, const QSize &requestedSize)
{
    const QString rectId = elementId + QLatin1String("_rect");
    const QRectF rect = renderer->boundsOnElement(idPrefix + (renderer->elementExists(idPrefix + rectId) ? rectId : elementId));
    Q_ASSERT_X(rect.width() >= 1 && rect.height() >= 1, "renderedSvgElement", "SVG bounding rect is NULL");
    QSize pixmapSize = rect.size().toSize();
    if (size)
        *size = pixmapSize;
    pixmapSize.scale(requestedSize, aspectRatioMode);
    Q_ASSERT_X(pixmapSize.width() >= 1 && pixmapSize.height() >= 1, "renderedSvgElement", "pixmapSize is NULL");
    QPixmap pixmap(pixmapSize);
    Q_ASSERT_X(!pixmap.isNull(), "renderedSvgElement", "pixmap is NULL");
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    renderer->render(&p, idPrefix + elementId, QRect(QPoint(), pixmapSize));
    return pixmap;
}

inline static void drawGradient(DesignElementType type, QImage &image)
{
    const int imageWidth = image.width();
    const QImage *gradient = type == DesignElementTypeButton ? buttonGradient() : frameGradient();
    const QRgb *gradientRgb = reinterpret_cast<const QRgb*>(gradient->constBits());
    QRgb *imageRgb = reinterpret_cast<QRgb*>(image.bits());
    const int quarterWidth = imageWidth / 2;
    const int quarterHeight = image.height() / 2;
    // Right triangle with a, b = 181.0193359837561662; c = 256.
    const qreal xScaleFactor = 181.0193359837561662 / quarterWidth;
    const qreal yScaleFactor = 181.0193359837561662 / quarterHeight;

    for (int y = 0; y <= quarterHeight; y++) {
        const int scaledY = yScaleFactor * y;
        const int scaledYSquare = scaledY * scaledY;
        const int offsetYPlusQuarterWidth = quarterWidth + imageWidth * (quarterHeight - y);
        for (int x = 0; x <= quarterWidth; x++) {
            const int scaledX = xScaleFactor * x;
            const int gradientColorIndex = int(sqrt(qreal(scaledYSquare + scaledX * scaledX)));
            const QRgb gradientColor = gradientRgb[gradientColorIndex];
            imageRgb[offsetYPlusQuarterWidth - x] = gradientColor;
            imageRgb[offsetYPlusQuarterWidth + x] = gradientColor;
        }
    }
    const int bytesPerLine = image.bytesPerLine();
    QRgb *dst = imageRgb + imageWidth * image.height() - imageWidth;
    QRgb *src = imageRgb;
    for (int row = 0; row < quarterHeight; row++) {
        memcpy(dst, src, bytesPerLine);
        dst -= imageWidth;
        src += imageWidth;
    }
}

inline static QPixmap renderedDesignElement(DesignElementType type, int variation, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(size)

    const ElementVariationList *elements = type == DesignElementTypeButton ? buttonVariations() : frameVariations();
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
    const QString &elementId = idPrefix + elementWithNearestRatio->elementIds.at(variation % elementWithNearestRatio->elementIds.count());
    static QPixmap cachedGradientButton;
    static QPixmap cachedGradientFrame;
    QPixmap &cachedGradient = type == DesignElementTypeButton ? cachedGradientButton : cachedGradientFrame;
    if (cachedGradient.size() == requestedSize) {
        QPixmap result(cachedGradient);
        QPainter p(&result);
        designRenderer()->render(&p, elementId, result.rect());
        return result;
    } else {
        QImage result(requestedSize, QImage::Format_ARGB32);
        result.fill(0);
        drawGradient(type, result);
        cachedGradient = QPixmap::fromImage(result);
        QPainter p(&result);
        designRenderer()->render(&p, elementId, result.rect());
        return QPixmap::fromImage(result);
    }
}

inline static QPixmap renderedLessonIcon(const QString &iconId, int buttonVariation, QSize *size, const QSize &requestedSize)
{
    QPixmap icon(requestedSize);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    QSvgRenderer *renderer = lessonIconsRenderer();
    const QRectF iconRectOriginal = renderer->boundsOnElement(idPrefix + iconId);
    QSizeF iconSize = iconRectOriginal.size();
    iconSize.scale(requestedSize, Qt::KeepAspectRatio);
    QRectF iconRect(QPointF(), iconSize);
    if (requestedSize.height() > requestedSize.width())
        iconRect.moveBottom(requestedSize.height());
    else
        iconRect.moveTop((requestedSize.height() - iconSize.height()) / 2);
    renderer->render(&p, idPrefix + iconId, iconRect);
    const QPixmap button = renderedDesignElement(DesignElementTypeButton, buttonVariation, size, requestedSize);
    p.drawPixmap(QPointF(), button);
    return icon;
}

inline static QPixmap spectrum(QSize *size, const QSize &requestedSize)
{
    const QSize resultSize(360, requestedSize.height());
    QImage result(resultSize.width(), 1, QImage::Format_ARGB32);
    QRgb *bits = reinterpret_cast<QRgb*>(result.bits());
    for (int i = 0; i < resultSize.width(); ++i)
        *(bits++) = QColor::fromHsl(i, 120, 200).rgb();
    if (size)
        *size = result.size();
    return QPixmap::fromImage(result.scaled(resultSize));
}

inline static QPixmap colorBlot(const QColor &color, int blotVariation, QSize *size, const QSize &requestedSize)
{
    QSvgRenderer *renderer = designRenderer();
    const static QString elementIdBase = QLatin1String("colorblot");
    const static int variationsCnt = variationsCount(renderer, elementIdBase);
    const int actualVariation = (blotVariation % variationsCnt) + 1;
    const QString elementId = elementIdBase + QLatin1Char('_') + QString::number(actualVariation);
    const QString maskElementId = elementId + QLatin1String("_mask");
    const QString highlightElementId = elementId + QLatin1String("_highlight");
    const QRectF backgroundRect = renderer->boundsOnElement(idPrefix + elementId);
    QSize pixmapSize = backgroundRect.size().toSize();
    if (size)
        *size = pixmapSize;
    pixmapSize.scale(requestedSize, Qt::KeepAspectRatio);
    const qreal scaleFactor = pixmapSize.width() / backgroundRect.width();
    QTransform transform =
            QTransform::fromScale(scaleFactor, scaleFactor);
    transform.translate(-backgroundRect.topLeft().x(), -backgroundRect.topLeft().y());
    QImage image(pixmapSize, QImage::Format_ARGB32);
    if (image.isNull())
        qDebug() << "****************** clock pixmap is NULL! Variation:" << blotVariation;
    image.fill(0);
    QPainter p(&image);
    p.setTransform(transform);
    renderer->render(&p, idPrefix + maskElementId, renderer->boundsOnElement(idPrefix + maskElementId));
    p.save();
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(backgroundRect, color);
    p.restore();
    renderer->render(&p, idPrefix + highlightElementId, renderer->boundsOnElement(idPrefix + highlightElementId));
    return QPixmap::fromImage(image);
}

QPixmap ImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    QPixmap result;
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
    } else if (idSegments.first() == QLatin1String("title")) {
        if (elementId == QLatin1String("textmask"))
            result = renderedSvgElement(idSegments.first(), designRenderer(), Qt::KeepAspectRatio, size, requestedSize);
        else
            result = spectrum(size, requestedSize);
    } else if (idSegments.first() == QLatin1String("specialbutton")) {
        result = renderedSvgElement(elementId, designRenderer(), Qt::IgnoreAspectRatio, size, requestedSize);
    } else if (idSegments.first() == buttonString) {
        result = renderedDesignElement(DesignElementTypeButton, elementId.toInt(), size, requestedSize);
    } else if (idSegments.first() == frameString) {
        result = renderedDesignElement(DesignElementTypeFrame, 0, size, requestedSize);
    } else if (idSegments.first() == QLatin1String("object")) {
        result = renderedSvgElement(elementId, objectRenderer(), Qt::KeepAspectRatio, size, requestedSize);
    } else if (idSegments.first() == QLatin1String("clock")) {
        if (idSegments.count() != 4) {
            qDebug() << "Wrong number of parameters for clock images:" << id;
            return QPixmap();
        }
        result = clock(idSegments.at(1).toInt(), idSegments.at(2).toInt(), idSegments.at(3).toInt(), size, requestedSize);
    } else if (idSegments.first() == QLatin1String("notes")) {
        result = notes(elementId.split(QLatin1Char(','), QString::SkipEmptyParts), size, requestedSize);
    } else if (idSegments.first() == QLatin1String("quantity")) {
        if (idSegments.count() != 3) {
            qDebug() << "Wrong number of parameters for quantity images:" << id;
            return QPixmap();
        }
        result = quantity(idSegments.at(1).toInt(), idSegments.at(2), size, requestedSize);
    } else if (idSegments.first() == QLatin1String("lessonicon")) {
        if (idSegments.count() != 3) {
            qDebug() << "Wrong number of parameters for lessonicon:" << id;
            return QPixmap();
        }
        result = renderedLessonIcon(idSegments.at(1), idSegments.at(2).toInt(), size, requestedSize);
    } else if (idSegments.first() == QLatin1String("color")) {
        if (idSegments.count() != 3) {
            qDebug() << "Wrong number of parameters for color:" << id;
            return QPixmap();
        }
        const QColor color(idSegments.at(1));
        result = colorBlot(color, idSegments.at(2).toInt(), size, requestedSize);
    } else {
        qDebug() << "invalid image Id:" << id;
    }
#if 0
    if (idSegments.first() == QLatin1String("object")) {
        QPainter p(&result);
        QPolygon points;
        for (int i = 0; i < result.width(); i += 2)
            for (int j = 0; j < result.height(); j += 2)
                points.append(QPoint(i, j));
        p.drawPoints(points);
        p.setPen(Qt::white);
        p.translate(1, 1);
        p.drawPoints(points);
    }
#endif
    return result;
}

void ImageProvider::init()
{
    designRenderer()->boundsOnElement(QString());
    objectRenderer()->boundsOnElement(QString());
    countablesRenderer()->boundsOnElement(QString());
    clocksRenderer()->boundsOnElement(QString());
    notesRenderer()->boundsOnElement(QString());
    lessonIconsRenderer()->boundsOnElement(QString());
    buttonVariations();
    frameVariations();
}

void ImageProvider::setDataPath(const QString &path)
{
    dataPath = path;
}
