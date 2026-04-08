#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QPixmap>
#include <QRectF>
#include <QSize>
#include <QVector>
#include <QWidget>

#include "DataParser.h"

class QPainter;

class PlotWidget : public QWidget {
    Q_OBJECT

public:
    enum ProjectionMode {
        LatLonProjection = 0,
        NorthPolarProjection = 1,
        SouthPolarProjection = 2,
        LatitudeHeightProjection = 3,
        LongitudeHeightProjection = 4
    };

    struct SunPosition {
        double latitude = 0.0;
        double longitude = 0.0;
    };

    explicit PlotWidget(QWidget* parent = nullptr);

    void setData(const QVector<GridPoint>& points, const GridMetadata& metadata);

    void setShowGrid(bool show);
    bool showGrid() const { return m_showGrid; }

    void setShowVectors(bool show);
    bool showVectors() const { return m_showVectors; }

    void setShowTerminator(bool show);
    bool showTerminator() const { return m_showTerminator; }

    void setShowSubsolarPoint(bool show);
    bool showSubsolarPoint() const { return m_showSubsolarPoint; }

    void setShowSubsolarTrack(bool show);
    bool showSubsolarTrack() const { return m_showSubsolarTrack; }

    void setShowEquator(bool show);
    bool showEquator() const { return m_showEquator; }

    void setShowContours(bool show);
    bool showContours() const { return m_showContours; }

    void setShowMltLabels(bool show);
    bool showMltLabels() const { return m_showMltLabels; }

    void setCenterOnNoonMeridian(bool enabled);
    bool centerOnNoonMeridian() const { return m_centerOnNoonMeridian; }

    void setMagneticMode(bool enabled);
    bool magneticMode() const { return m_magneticMode; }

    void setSouthMirror(bool enabled);
    bool southMirror() const { return m_southMirror; }

    void setLogScale(bool enabled);
    bool logScale() const { return m_logScale; }

    void setColorScheme(int scheme);
    int colorScheme() const { return m_colorScheme; }

    void setContourStep(double step);
    double contourStep() const { return m_contourStep; }

    void setProjectionMode(ProjectionMode mode);
    ProjectionMode projectionMode() const { return m_projectionMode; }

    void setLongitudeSlice(double longitudeDegrees);
    double longitudeSlice() const { return m_longitudeSlice; }
    double resolvedLongitudeSlice() const { return m_resolvedLongitudeSlice; }

    void setLatitudeSlice(double latitudeDegrees);
    double latitudeSlice() const { return m_latitudeSlice; }
    double resolvedLatitudeSlice() const { return m_resolvedLatitudeSlice; }

    void setSliceWindow(double halfWidthDegrees);
    double sliceWindow() const { return m_sliceHalfWindow; }

    void setCropRegion(bool enabled, double minLat, double maxLat, double minLon, double maxLon);
    void clearCropRegion();
    bool cropEnabled() const { return m_cropEnabled; }
    double cropMinLat() const { return m_cropMinLat; }
    double cropMaxLat() const { return m_cropMaxLat; }
    double cropMinLon() const { return m_cropMinLon; }
    double cropMaxLon() const { return m_cropMaxLon; }

    void setManualValueRange(double minValue, double maxValue);
    void clearManualValueRange();
    bool hasManualValueRange() const { return m_useManualValueRange; }
    double manualMinValue() const { return m_manualMinValue; }
    double manualMaxValue() const { return m_manualMaxValue; }

    bool isHeightProjection() const;
    bool isPolarProjection() const;

    void resetView();
    bool exportImage(const QString& filePath,
                     const QSize& imageSize,
                     int quality = -1,
                     bool grayscale = false);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct RenderArea {
        QRectF plotRect;
        QRectF colorBarRect;
    };

    struct ValueRange {
        double minRaw = 0.0;
        double maxRaw = 1.0;
        double minDisplay = 0.0;
        double maxDisplay = 1.0;
    };

    struct SectionDomain {
        double xMin = 0.0;
        double xMax = 1.0;
        double yMin = 0.0;
        double yMax = 1.0;
    };

    static double wrapTo360(double degrees);
    static double wrapTo180(double degrees);
    static double wrapTo24(double hours);

    void invalidateCache();
    SunPosition currentSubsolarPosition() const;
    bool isPointInsideCrop(const GridPoint& point) const;
    QVector<GridPoint> visiblePoints() const;
    RenderArea computeRenderArea(const QSize& canvasSize) const;
    QPointF mapToScreen(double latitude, double longitude, const RenderArea& area) const;

    SectionDomain computeSectionDomain(const QVector<GridPoint>& points) const;
    QPointF mapSectionToScreen(double x, double y, const RenderArea& area, const SectionDomain& domain) const;
    double sectionX(const GridPoint& point) const;
    double sectionY(const GridPoint& point) const;
    double resolveNearestLongitudeSlice(const QVector<GridPoint>& points) const;
    double resolveNearestLatitudeSlice(const QVector<GridPoint>& points) const;
    bool isPointInCurrentSlice(const GridPoint& point) const;

    double transformedValue(double value) const;
    ValueRange computeValueRange(const QVector<GridPoint>& points) const;
    QColor colorForValue(double rawValue, const ValueRange& range) const;
    bool segmentIntersectsLevel(double v1, double v2, double level) const;
    QPointF interpolateContourPoint(double x1,
                                    double y1,
                                    double v1,
                                    double x2,
                                    double y2,
                                    double v2,
                                    double level,
                                    const RenderArea& area,
                                    const SectionDomain* domain) const;

    void drawScene(QPainter& painter, const QSize& canvasSize) const;
    void drawBackground(QPainter& painter, const QSize& canvasSize) const;
    void drawGrid(QPainter& painter,
                  const RenderArea& area,
                  const QVector<GridPoint>& points,
                  const SectionDomain& sectionDomain) const;
    void drawPolarGrid(QPainter& painter, const RenderArea& area) const;
    void drawData(QPainter& painter,
                  const QVector<GridPoint>& points,
                  const RenderArea& area,
                  const ValueRange& range,
                  const SectionDomain& sectionDomain) const;
    void drawVectors(QPainter& painter,
                     const QVector<GridPoint>& points,
                     const RenderArea& area,
                     const SectionDomain& sectionDomain) const;
    void drawTerminator(QPainter& painter, const RenderArea& area) const;
    void drawSubsolarGraphics(QPainter& painter, const RenderArea& area) const;
    void drawContours(QPainter& painter,
                      const QVector<GridPoint>& points,
                      const RenderArea& area,
                      const ValueRange& range,
                      const SectionDomain& sectionDomain) const;
    void drawAxes(QPainter& painter, const RenderArea& area, const SectionDomain& sectionDomain) const;
    void drawTitleAndFooter(QPainter& painter, const QSize& canvasSize, const RenderArea& area) const;
    void drawMltLabels(QPainter& painter, const RenderArea& area) const;
    void drawColorBar(QPainter& painter, const RenderArea& area, const ValueRange& range) const;
    void drawSliceInfo(QPainter& painter, const RenderArea& area) const;

    QVector<GridPoint> m_points;
    GridMetadata m_metadata;

    bool m_showGrid = true;
    bool m_showVectors = false;
    bool m_showTerminator = false;
    bool m_showSubsolarPoint = false;
    bool m_showSubsolarTrack = false;
    bool m_showEquator = false;
    bool m_showContours = false;
    bool m_showMltLabels = false;
    bool m_centerOnNoonMeridian = false;
    bool m_magneticMode = false;
    bool m_southMirror = false;
    bool m_logScale = false;

    int m_colorScheme = 2;
    double m_contourStep = 10.0;
    ProjectionMode m_projectionMode = LatLonProjection;

    double m_longitudeSlice = 0.0;
    double m_latitudeSlice = 0.0;
    double m_sliceHalfWindow = 0.0;
    mutable double m_resolvedLongitudeSlice = 0.0;
    mutable double m_resolvedLatitudeSlice = 0.0;

    bool m_cropEnabled = false;
    double m_cropMinLat = -90.0;
    double m_cropMaxLat = 90.0;
    double m_cropMinLon = 0.0;
    double m_cropMaxLon = 360.0;

    bool m_useManualValueRange = false;
    double m_manualMinValue = 0.0;
    double m_manualMaxValue = 1.0;

    double m_scaleFactor = 1.0;
    double m_offsetX = 0.0;
    double m_offsetY = 0.0;
    bool m_isPanning = false;
    QPoint m_lastMousePos;

    mutable QPixmap m_cachePixmap;
    mutable bool m_cacheValid = false;
    mutable double m_cachedNoonLongitude = 0.0;
    mutable bool m_cachedNoonLongitudeValid = false;
};

#endif // PLOTWIDGET_H
