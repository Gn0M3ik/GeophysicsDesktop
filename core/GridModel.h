#ifndef DATAVIZ_CORE_GRIDMODEL_H
#define DATAVIZ_CORE_GRIDMODEL_H

#include <QVector>
#include <utility>

#include "DataParser.h"

namespace dataviz::core {

class GridModel {
public:
    void clear();
    void setData(const QVector<GridPoint>& points, const GridMetadata& metadata);

    bool isEmpty() const { return m_points.isEmpty(); }
    const QVector<GridPoint>& points() const { return m_points; }
    const GridMetadata& metadata() const { return m_metadata; }

    bool hasVectorData() const;
    bool hasHeightDimension() const;

    QVector<double> uniqueLatitudes() const;
    QVector<double> uniqueLongitudes() const;
    QVector<double> uniqueHeights() const;

    std::pair<double, double> scalarRange() const;
    QVector<GridPoint> crop(double minLatitude, double maxLatitude, double minLongitude, double maxLongitude) const;
    QVector<GridPoint> longitudeSlice(double longitude, double halfWindowDegrees) const;
    QVector<GridPoint> latitudeSlice(double latitude, double halfWindowDegrees) const;
    GridPoint nearest(double latitude, double longitude, double height = 0.0) const;

private:
    static double wrapTo360(double degrees);
    static double wrapTo180(double degrees);
    static double circularDistance(double lonA, double lonB);

    QVector<GridPoint> m_points;
    GridMetadata m_metadata;
};

} // namespace dataviz::core

#endif // DATAVIZ_CORE_GRIDMODEL_H
