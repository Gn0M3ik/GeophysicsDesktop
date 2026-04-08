#include "PlotWidget.h"

#include <QImage>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr double kInteractiveSupersample = 1.5;
constexpr qint64 kMaxInteractiveRenderPixels = 7'500'000;
constexpr double kExportSupersample = 2.0;
constexpr qint64 kMaxExportRenderPixels = 90'000'000;

double wrapTo360Local(double degrees) {
    double normalized = std::fmod(degrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

double wrapTo180Local(double degrees) {
    double normalized = wrapTo360Local(degrees);
    if (normalized > 180.0) {
        normalized -= 360.0;
    }
    return normalized;
}

double circularDistanceDeg(double lonA, double lonB) {
    return std::abs(wrapTo180Local(lonA - lonB));
}

QSize supersampledSize(const QSize& logicalSize, double preferredScale, qint64 maxPixels) {
    if (!logicalSize.isValid() || logicalSize.width() <= 0 || logicalSize.height() <= 0) {
        return QSize(1, 1);
    }

    double scale = preferredScale;
    const qint64 logicalPixels = static_cast<qint64>(logicalSize.width()) * static_cast<qint64>(logicalSize.height());
    if (logicalPixels <= 0) {
        return QSize(1, 1);
    }

    const double limitedScale = std::sqrt(static_cast<double>(maxPixels) / static_cast<double>(logicalPixels));
    if (std::isfinite(limitedScale)) {
        scale = std::min(scale, limitedScale);
    }
    scale = qBound(1.0, scale, preferredScale);

    return QSize(
        std::max(1, static_cast<int>(std::ceil(static_cast<double>(logicalSize.width()) * scale))),
        std::max(1, static_cast<int>(std::ceil(static_cast<double>(logicalSize.height()) * scale))));
}
} // namespace

PlotWidget::PlotWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(760, 520);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

double PlotWidget::wrapTo360(double degrees) {
    double normalized = std::fmod(degrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

double PlotWidget::wrapTo180(double degrees) {
    double normalized = wrapTo360(degrees);
    if (normalized > 180.0) {
        normalized -= 360.0;
    }
    return normalized;
}

double PlotWidget::wrapTo24(double hours) {
    double normalized = std::fmod(hours, 24.0);
    if (normalized < 0.0) {
        normalized += 24.0;
    }
    return normalized;
}

bool PlotWidget::isHeightProjection() const {
    return m_projectionMode == LatitudeHeightProjection || m_projectionMode == LongitudeHeightProjection;
}

bool PlotWidget::isPolarProjection() const {
    return m_projectionMode == NorthPolarProjection || m_projectionMode == SouthPolarProjection;
}

void PlotWidget::invalidateCache() {
    m_cacheValid = false;
    m_cachedNoonLongitudeValid = false;
    update();
}

void PlotWidget::setData(const QVector<GridPoint>& points, const GridMetadata& metadata) {
    const bool hadDataBefore = !m_points.isEmpty();
    m_points = points;
    m_metadata = metadata;

    if (!m_points.isEmpty() && !hadDataBefore) {
        m_longitudeSlice = wrapTo180(std::isfinite(m_points.first().longitude) ? m_points.first().longitude : 0.0);
        m_latitudeSlice = qBound(-90.0, std::isfinite(m_points.first().latitude) ? m_points.first().latitude : 0.0, 90.0);
    }

    invalidateCache();
}

void PlotWidget::setShowGrid(bool show) {
    m_showGrid = show;
    invalidateCache();
}

void PlotWidget::setShowVectors(bool show) {
    m_showVectors = show;
    invalidateCache();
}

void PlotWidget::setShowTerminator(bool show) {
    m_showTerminator = show;
    invalidateCache();
}

void PlotWidget::setShowSubsolarPoint(bool show) {
    m_showSubsolarPoint = show;
    invalidateCache();
}

void PlotWidget::setShowSubsolarTrack(bool show) {
    m_showSubsolarTrack = show;
    invalidateCache();
}

void PlotWidget::setShowEquator(bool show) {
    m_showEquator = show;
    invalidateCache();
}

void PlotWidget::setShowContours(bool show) {
    m_showContours = show;
    invalidateCache();
}

void PlotWidget::setShowMltLabels(bool show) {
    m_showMltLabels = show;
    invalidateCache();
}

void PlotWidget::setCenterOnNoonMeridian(bool enabled) {
    m_centerOnNoonMeridian = enabled;
    invalidateCache();
}

void PlotWidget::setMagneticMode(bool enabled) {
    if (m_magneticMode == enabled) {
        return;
    }
    m_magneticMode = enabled;
    invalidateCache();
}

void PlotWidget::setSouthMirror(bool enabled) {
    m_southMirror = enabled;
    invalidateCache();
}

void PlotWidget::setLogScale(bool enabled) {
    m_logScale = enabled;
    invalidateCache();
}

void PlotWidget::setColorScheme(int scheme) {
    m_colorScheme = qBound(0, scheme, 3);
    invalidateCache();
}

void PlotWidget::setContourStep(double step) {
    if (step <= 0.0) {
        return;
    }
    m_contourStep = step;
    invalidateCache();
}

void PlotWidget::setProjectionMode(ProjectionMode mode) {
    if (m_projectionMode == mode) {
        return;
    }
    m_projectionMode = mode;
    invalidateCache();
}

void PlotWidget::setLongitudeSlice(double longitudeDegrees) {
    m_longitudeSlice = wrapTo180(longitudeDegrees);
    invalidateCache();
}

void PlotWidget::setLatitudeSlice(double latitudeDegrees) {
    m_latitudeSlice = qBound(-90.0, latitudeDegrees, 90.0);
    invalidateCache();
}

void PlotWidget::setSliceWindow(double halfWidthDegrees) {
    m_sliceHalfWindow = qBound(0.0, halfWidthDegrees, 180.0);
    invalidateCache();
}

void PlotWidget::setCropRegion(bool enabled, double minLat, double maxLat, double minLon, double maxLon) {
    m_cropEnabled = enabled;
    const double safeMinLat = std::isfinite(minLat) ? minLat : -90.0;
    const double safeMaxLat = std::isfinite(maxLat) ? maxLat : 90.0;
    const double safeMinLon = std::isfinite(minLon) ? minLon : 0.0;
    const double safeMaxLon = std::isfinite(maxLon) ? maxLon : 360.0;
    const bool fullLongitudeRange = std::abs(safeMaxLon - safeMinLon) >= 360.0 - 1e-9;

    m_cropMinLat = qBound(-90.0, std::min(safeMinLat, safeMaxLat), 90.0);
    m_cropMaxLat = qBound(-90.0, std::max(safeMinLat, safeMaxLat), 90.0);
    if (fullLongitudeRange) {
        m_cropMinLon = 0.0;
        m_cropMaxLon = 360.0;
    } else {
        m_cropMinLon = wrapTo360(safeMinLon);
        m_cropMaxLon = wrapTo360(safeMaxLon);
    }
    invalidateCache();
}

void PlotWidget::clearCropRegion() {
    m_cropEnabled = false;
    invalidateCache();
}

void PlotWidget::setManualValueRange(double minValue, double maxValue) {
    m_manualMinValue = std::min(minValue, maxValue);
    m_manualMaxValue = std::max(minValue, maxValue);
    m_useManualValueRange = true;
    invalidateCache();
}

void PlotWidget::clearManualValueRange() {
    m_useManualValueRange = false;
    invalidateCache();
}

void PlotWidget::resetView() {
    m_scaleFactor = 1.0;
    m_offsetX = 0.0;
    m_offsetY = 0.0;
    invalidateCache();
}

bool PlotWidget::exportImage(const QString& filePath,
                             const QSize& imageSize,
                             int quality,
                             bool grayscale) {
    const QSize targetSize = imageSize.isValid() ? imageSize : size();
    const QSize renderSize = supersampledSize(targetSize, kExportSupersample, kMaxExportRenderPixels);

    QImage renderImage(renderSize, QImage::Format_ARGB32_Premultiplied);
    renderImage.fill(Qt::white);

    QPainter painter(&renderImage);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const double scaleX = static_cast<double>(renderSize.width()) / static_cast<double>(std::max(1, targetSize.width()));
    const double scaleY = static_cast<double>(renderSize.height()) / static_cast<double>(std::max(1, targetSize.height()));
    painter.scale(scaleX, scaleY);
    drawScene(painter, targetSize);
    painter.end();

    QImage outputImage = renderImage;
    if (renderSize != targetSize) {
        outputImage = renderImage.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    constexpr double kInchInMeters = 0.0254;
    constexpr int kDpi = 300;
    const int dotsPerMeter = static_cast<int>(kDpi / kInchInMeters);
    outputImage.setDotsPerMeterX(dotsPerMeter);
    outputImage.setDotsPerMeterY(dotsPerMeter);

    if (grayscale) {
        outputImage = outputImage.convertToFormat(QImage::Format_Grayscale8);
    }

    return outputImage.save(filePath, nullptr, quality);
}

void PlotWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

    const QSize logicalSize = size();
    const QSize renderSize = supersampledSize(logicalSize, kInteractiveSupersample, kMaxInteractiveRenderPixels);

    if (!m_cacheValid || m_cachePixmap.size() != renderSize) {
        m_cachePixmap = QPixmap(renderSize);
        m_cachePixmap.fill(Qt::white);

        QPainter cachePainter(&m_cachePixmap);
        cachePainter.setRenderHint(QPainter::Antialiasing, true);
        cachePainter.setRenderHint(QPainter::TextAntialiasing, true);
        cachePainter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const double scaleX =
            static_cast<double>(renderSize.width()) / static_cast<double>(std::max(1, logicalSize.width()));
        const double scaleY =
            static_cast<double>(renderSize.height()) / static_cast<double>(std::max(1, logicalSize.height()));
        cachePainter.scale(scaleX, scaleY);
        drawScene(cachePainter, logicalSize);
        m_cacheValid = true;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawPixmap(rect(), m_cachePixmap, m_cachePixmap.rect());
}

void PlotWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_cacheValid = false;
}

void PlotWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void PlotWidget::mouseReleaseEvent(QMouseEvent* event) {
    if ((event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) && m_isPanning) {
        m_isPanning = false;
        unsetCursor();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void PlotWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning) {
        const QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        m_offsetX += delta.x();
        m_offsetY += delta.y();
        invalidateCache();
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void PlotWidget::wheelEvent(QWheelEvent* event) {
    const QPointF cursorPos = event->position();
    const double baseX = (cursorPos.x() - m_offsetX) / m_scaleFactor;
    const double baseY = (cursorPos.y() - m_offsetY) / m_scaleFactor;

    const double zoom = event->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
    m_scaleFactor = qBound(0.2, m_scaleFactor * zoom, 12.0);

    m_offsetX = cursorPos.x() - baseX * m_scaleFactor;
    m_offsetY = cursorPos.y() - baseY * m_scaleFactor;
    invalidateCache();
    event->accept();
}

bool PlotWidget::isPointInsideCrop(const GridPoint& point) const {
    if (!std::isfinite(point.latitude) || !std::isfinite(point.longitude)) {
        return false;
    }

    if (!m_cropEnabled) {
        return true;
    }

    if (point.latitude < m_cropMinLat || point.latitude > m_cropMaxLat) {
        return false;
    }

    if ((m_cropMaxLon - m_cropMinLon) >= 360.0 - 1e-9) {
        return true;
    }

    const double lon = wrapTo360(point.longitude);
    const double minLon = m_cropMinLon;
    const double maxLon = m_cropMaxLon;
    if (minLon <= maxLon) {
        return lon >= minLon && lon <= maxLon;
    }

    return lon >= minLon || lon <= maxLon;
}

double PlotWidget::resolveNearestLongitudeSlice(const QVector<GridPoint>& points) const {
    if (points.isEmpty()) {
        return m_longitudeSlice;
    }

    double nearestLon = points.first().longitude;
    double bestDistance = circularDistanceDeg(nearestLon, m_longitudeSlice);

    for (const GridPoint& point : points) {
        const double distance = circularDistanceDeg(point.longitude, m_longitudeSlice);
        if (distance < bestDistance) {
            bestDistance = distance;
            nearestLon = point.longitude;
        }
    }

    return wrapTo180(nearestLon);
}

double PlotWidget::resolveNearestLatitudeSlice(const QVector<GridPoint>& points) const {
    if (points.isEmpty()) {
        return m_latitudeSlice;
    }

    double nearestLat = points.first().latitude;
    double bestDistance = std::abs(nearestLat - m_latitudeSlice);

    for (const GridPoint& point : points) {
        const double distance = std::abs(point.latitude - m_latitudeSlice);
        if (distance < bestDistance) {
            bestDistance = distance;
            nearestLat = point.latitude;
        }
    }

    return qBound(-90.0, nearestLat, 90.0);
}

bool PlotWidget::isPointInCurrentSlice(const GridPoint& point) const {
    if (m_projectionMode == LatitudeHeightProjection) {
        const double tolerance = (m_sliceHalfWindow > 0.0)
            ? m_sliceHalfWindow
            : std::max(1e-4, std::abs(m_metadata.lonStep) * 0.35);
        return circularDistanceDeg(point.longitude, m_resolvedLongitudeSlice) <= tolerance;
    }

    if (m_projectionMode == LongitudeHeightProjection) {
        const double tolerance = (m_sliceHalfWindow > 0.0)
            ? m_sliceHalfWindow
            : std::max(1e-4, std::abs(m_metadata.latStep) * 0.35);
        return std::abs(point.latitude - m_resolvedLatitudeSlice) <= tolerance;
    }

    return true;
}

QVector<GridPoint> PlotWidget::visiblePoints() const {
    QVector<GridPoint> cropped;
    cropped.reserve(m_points.size());

    for (const GridPoint& point : m_points) {
        if (!std::isfinite(point.latitude) ||
            !std::isfinite(point.longitude) ||
            !std::isfinite(point.height) ||
            !std::isfinite(point.value)) {
            continue;
        }

        if (!isPointInsideCrop(point)) {
            continue;
        }

        if (m_projectionMode == NorthPolarProjection && point.latitude < 0.0) {
            continue;
        }

        if (m_projectionMode == SouthPolarProjection && point.latitude > 0.0) {
            continue;
        }

        cropped.append(point);
    }

    if (!isHeightProjection()) {
        m_resolvedLongitudeSlice = m_longitudeSlice;
        m_resolvedLatitudeSlice = m_latitudeSlice;
        return cropped;
    }

    if (cropped.isEmpty()) {
        return cropped;
    }

    QVector<GridPoint> result;
    result.reserve(cropped.size());

    if (m_projectionMode == LatitudeHeightProjection) {
        m_resolvedLongitudeSlice = resolveNearestLongitudeSlice(cropped);
        for (const GridPoint& point : cropped) {
            if (isPointInCurrentSlice(point)) {
                result.append(point);
            }
        }

        if (result.isEmpty()) {
            double bestDistance = std::numeric_limits<double>::max();
            for (const GridPoint& point : cropped) {
                bestDistance = std::min(bestDistance, circularDistanceDeg(point.longitude, m_resolvedLongitudeSlice));
            }
            for (const GridPoint& point : cropped) {
                if (std::abs(circularDistanceDeg(point.longitude, m_resolvedLongitudeSlice) - bestDistance) <= 1e-6) {
                    result.append(point);
                }
            }
        }
    } else if (m_projectionMode == LongitudeHeightProjection) {
        m_resolvedLatitudeSlice = resolveNearestLatitudeSlice(cropped);
        for (const GridPoint& point : cropped) {
            if (isPointInCurrentSlice(point)) {
                result.append(point);
            }
        }

        if (result.isEmpty()) {
            double bestDistance = std::numeric_limits<double>::max();
            for (const GridPoint& point : cropped) {
                bestDistance = std::min(bestDistance, std::abs(point.latitude - m_resolvedLatitudeSlice));
            }
            for (const GridPoint& point : cropped) {
                if (std::abs(std::abs(point.latitude - m_resolvedLatitudeSlice) - bestDistance) <= 1e-6) {
                    result.append(point);
                }
            }
        }
    }

    return result;
}

double PlotWidget::sectionX(const GridPoint& point) const {
    if (m_projectionMode == LatitudeHeightProjection) {
        return point.latitude;
    }

    if (m_centerOnNoonMeridian) {
        return wrapTo180(point.longitude - m_cachedNoonLongitude);
    }
    return wrapTo360(point.longitude);
}

double PlotWidget::sectionY(const GridPoint& point) const {
    return point.height;
}

PlotWidget::SectionDomain PlotWidget::computeSectionDomain(const QVector<GridPoint>& points) const {
    SectionDomain domain;
    if (points.isEmpty()) {
        return domain;
    }

    domain.xMin = sectionX(points.first());
    domain.xMax = domain.xMin;
    domain.yMin = sectionY(points.first());
    domain.yMax = domain.yMin;

    for (const GridPoint& point : points) {
        const double x = sectionX(point);
        const double y = sectionY(point);
        domain.xMin = std::min(domain.xMin, x);
        domain.xMax = std::max(domain.xMax, x);
        domain.yMin = std::min(domain.yMin, y);
        domain.yMax = std::max(domain.yMax, y);
    }

    if (qFuzzyCompare(domain.xMin, domain.xMax)) {
        domain.xMin -= 1.0;
        domain.xMax += 1.0;
    }
    if (qFuzzyCompare(domain.yMin, domain.yMax)) {
        domain.yMin -= 1.0;
        domain.yMax += 1.0;
    }

    return domain;
}

QPointF PlotWidget::mapSectionToScreen(double x, double y, const RenderArea& area, const SectionDomain& domain) const {
    const double xSpan = std::max(1e-6, domain.xMax - domain.xMin);
    const double ySpan = std::max(1e-6, domain.yMax - domain.yMin);

    double px = area.plotRect.left() + (x - domain.xMin) / xSpan * area.plotRect.width();
    double py = area.plotRect.top() + (domain.yMax - y) / ySpan * area.plotRect.height();

    px = px * m_scaleFactor + m_offsetX;
    py = py * m_scaleFactor + m_offsetY;
    return QPointF(px, py);
}
