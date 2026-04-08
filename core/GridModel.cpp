#include "core/GridModel.h"

#include <QSet>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <limits>

namespace dataviz::core {

namespace {
constexpr double kQuantizeScale = 1e6;

qint64 quantizeToMicro(double value) {
    return static_cast<qint64>(std::llround(value * kQuantizeScale));
}

bool isFinitePointCoordinates(const GridPoint& point) {
    return std::isfinite(point.latitude) && std::isfinite(point.longitude) && std::isfinite(point.height);
}
} // namespace

void GridModel::clear() {
    m_points.clear();
    m_metadata = GridMetadata();
}

void GridModel::setData(const QVector<GridPoint>& points, const GridMetadata& metadata) {
    m_points = points;
    m_metadata = metadata;
}

bool GridModel::hasVectorData() const {
    for (const GridPoint& point : m_points) {
        if (!std::isfinite(point.vx) || !std::isfinite(point.vy) || !std::isfinite(point.vz)) {
            continue;
        }
        if (std::abs(point.vx) > 1e-12 || std::abs(point.vy) > 1e-12 || std::abs(point.vz) > 1e-12) {
            return true;
        }
    }
    return false;
}

bool GridModel::hasHeightDimension() const {
    return m_metadata.hasHeightDimension || m_metadata.heightCount > 1;
}

QVector<double> GridModel::uniqueLatitudes() const {
    QSet<qint64> keys;
    keys.reserve(m_points.size());
    QVector<double> values;
    values.reserve(m_points.size());

    for (const GridPoint& point : m_points) {
        if (!std::isfinite(point.latitude)) {
            continue;
        }
        const qint64 key = quantizeToMicro(point.latitude);
        if (keys.contains(key)) {
            continue;
        }
        keys.insert(key);
        values.append(point.latitude);
    }

    std::sort(values.begin(), values.end());
    return values;
}

QVector<double> GridModel::uniqueLongitudes() const {
    QSet<qint64> keys;
    keys.reserve(m_points.size());
    QVector<double> values;
    values.reserve(m_points.size());

    for (const GridPoint& point : m_points) {
        if (!std::isfinite(point.longitude)) {
            continue;
        }
        const double lon = wrapTo180(point.longitude);
        const qint64 key = quantizeToMicro(lon);
        if (keys.contains(key)) {
            continue;
        }
        keys.insert(key);
        values.append(lon);
    }

    std::sort(values.begin(), values.end());
    return values;
}

QVector<double> GridModel::uniqueHeights() const {
    QSet<qint64> keys;
    keys.reserve(m_points.size());
    QVector<double> values;
    values.reserve(m_points.size());

    for (const GridPoint& point : m_points) {
        if (!std::isfinite(point.height)) {
            continue;
        }
        const qint64 key = quantizeToMicro(point.height);
        if (keys.contains(key)) {
            continue;
        }
        keys.insert(key);
        values.append(point.height);
    }

    std::sort(values.begin(), values.end());
    return values;
}

std::pair<double, double> GridModel::scalarRange() const {
    if (m_points.isEmpty()) {
        return {0.0, 1.0};
    }

    const auto finiteValueIt = std::find_if(m_points.cbegin(), m_points.cend(), [](const GridPoint& point) {
        return std::isfinite(point.value);
    });
    if (finiteValueIt == m_points.cend()) {
        return {0.0, 1.0};
    }

    double minimum = finiteValueIt->value;
    double maximum = finiteValueIt->value;
    for (const GridPoint& point : m_points) {
        if (!std::isfinite(point.value)) {
            continue;
        }
        minimum = std::min(minimum, point.value);
        maximum = std::max(maximum, point.value);
    }
    if (qFuzzyCompare(minimum, maximum)) {
        minimum -= 1.0;
        maximum += 1.0;
    }
    return {minimum, maximum};
}

QVector<GridPoint> GridModel::crop(double minLatitude, double maxLatitude, double minLongitude, double maxLongitude) const {
    QVector<GridPoint> out;
    out.reserve(m_points.size());

    const double safeMinLatitude = std::isfinite(minLatitude) ? minLatitude : -90.0;
    const double safeMaxLatitude = std::isfinite(maxLatitude) ? maxLatitude : 90.0;
    const double safeMinLongitude = std::isfinite(minLongitude) ? minLongitude : 0.0;
    const double safeMaxLongitude = std::isfinite(maxLongitude) ? maxLongitude : 360.0;

    const double minLat = qBound(-90.0, std::min(safeMinLatitude, safeMaxLatitude), 90.0);
    const double maxLat = qBound(-90.0, std::max(safeMinLatitude, safeMaxLatitude), 90.0);
    const bool fullLongitudeRange = std::abs(safeMaxLongitude - safeMinLongitude) >= 360.0 - 1e-9;
    const double minLon = wrapTo360(safeMinLongitude);
    const double maxLon = wrapTo360(safeMaxLongitude);

    for (const GridPoint& point : m_points) {
        if (!std::isfinite(point.latitude) || !std::isfinite(point.longitude)) {
            continue;
        }
        if (point.latitude < minLat || point.latitude > maxLat) {
            continue;
        }

        const double lon = wrapTo360(point.longitude);
        const bool insideLon =
            fullLongitudeRange ||
            ((minLon <= maxLon) ? (lon >= minLon && lon <= maxLon) : (lon >= minLon || lon <= maxLon));
        if (insideLon) {
            out.append(point);
        }
    }

    return out;
}

QVector<GridPoint> GridModel::longitudeSlice(double longitude, double halfWindowDegrees) const {
    QVector<GridPoint> out;
    out.reserve(m_points.size());
    const double lon = wrapTo180(longitude);
    const double tolerance = qBound(0.0, halfWindowDegrees, 180.0);

    for (const GridPoint& point : m_points) {
        if (!std::isfinite(point.longitude)) {
            continue;
        }
        if (circularDistance(point.longitude, lon) <= tolerance) {
            out.append(point);
        }
    }
    return out;
}

QVector<GridPoint> GridModel::latitudeSlice(double latitude, double halfWindowDegrees) const {
    QVector<GridPoint> out;
    out.reserve(m_points.size());
    const double lat = qBound(-90.0, latitude, 90.0);
    const double tolerance = qBound(0.0, halfWindowDegrees, 180.0);

    for (const GridPoint& point : m_points) {
        if (!std::isfinite(point.latitude)) {
            continue;
        }
        if (std::abs(point.latitude - lat) <= tolerance) {
            out.append(point);
        }
    }
    return out;
}

GridPoint GridModel::nearest(double latitude, double longitude, double height) const {
    if (m_points.isEmpty()) {
        return GridPoint();
    }

    const double queryLat = qBound(-90.0, latitude, 90.0);
    const double queryLon = wrapTo180(longitude);

    const GridPoint* bestPoint = nullptr;
    double bestScore = std::numeric_limits<double>::max();

    for (const GridPoint& point : m_points) {
        if (!isFinitePointCoordinates(point)) {
            continue;
        }
        const double dLat = point.latitude - queryLat;
        const double dLon = circularDistance(point.longitude, queryLon);
        const double dH = point.height - height;
        const double score = dLat * dLat + dLon * dLon + dH * dH;
        if (score < bestScore) {
            bestScore = score;
            bestPoint = &point;
        }
    }

    if (bestPoint == nullptr) {
        return GridPoint();
    }

    return *bestPoint;
}

double GridModel::wrapTo360(double degrees) {
    double normalized = std::fmod(degrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

double GridModel::wrapTo180(double degrees) {
    double normalized = wrapTo360(degrees);
    if (normalized > 180.0) {
        normalized -= 360.0;
    }
    return normalized;
}

double GridModel::circularDistance(double lonA, double lonB) {
    return std::abs(wrapTo180(lonA - lonB));
}

} // namespace dataviz::core
