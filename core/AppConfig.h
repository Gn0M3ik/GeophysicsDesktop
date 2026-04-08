#ifndef DATAVIZ_CORE_APPCONFIG_H
#define DATAVIZ_CORE_APPCONFIG_H

#include <QString>

class QSettings;

namespace dataviz::core {

struct CropRegion {
    bool enabled = false;
    double minLatitude = -90.0;
    double maxLatitude = 90.0;
    double minLongitude = 0.0;
    double maxLongitude = 360.0;
};

struct ValueRange {
    bool manualEnabled = false;
    double minimum = 0.0;
    double maximum = 1.0;
};

struct SliceSettings {
    double longitude = 0.0;
    double latitude = 0.0;
    double halfWindow = 0.0;
};

struct OverlaySettings {
    bool showGrid = true;
    bool showVectors = false;
    bool showContours = false;
    bool showTerminator = false;
    bool showSubsolarPoint = false;
    bool showSubsolarTrack = false;
    bool showEquator = false;
    bool showMltLabels = false;
};

class AppConfig {
public:
    enum ProjectionMode {
        LatLonProjection = 0,
        NorthPolarProjection = 1,
        SouthPolarProjection = 2,
        LatitudeHeightProjection = 3,
        LongitudeHeightProjection = 4
    };

    enum ColorScheme {
        Grayscale = 0,
        Rainbow = 1,
        BlueGreenRed = 2,
        BlueRed = 3
    };

    ProjectionMode projectionMode = LatLonProjection;
    ColorScheme colorScheme = BlueGreenRed;
    OverlaySettings overlays;
    CropRegion crop;
    ValueRange valueRange;
    SliceSettings slice;

    bool logScale = false;
    bool centerOnNoonMeridian = false;
    bool southMirror = false;
    double contourStep = 10.0;
    QString lastOpenDirectory;

    void normalize();
    void load(QSettings& settings);
    void save(QSettings& settings) const;
};

} // namespace dataviz::core

#endif // DATAVIZ_CORE_APPCONFIG_H
