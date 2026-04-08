#include "core/AppConfig.h"

#include <QDir>
#include <QSettings>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace dataviz::core {

namespace {
double wrapTo360(double degrees) {
    double normalized = std::fmod(degrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

double wrapTo180(double degrees) {
    double normalized = wrapTo360(degrees);
    if (normalized > 180.0) {
        normalized -= 360.0;
    }
    return normalized;
}

void normalizeLongitudeBounds(double minLongitude,
                              double maxLongitude,
                              double& normalizedMinLongitude,
                              double& normalizedMaxLongitude) {
    if (!std::isfinite(minLongitude) || !std::isfinite(maxLongitude)) {
        normalizedMinLongitude = 0.0;
        normalizedMaxLongitude = 360.0;
        return;
    }

    const double span = std::abs(maxLongitude - minLongitude);
    if (span >= 360.0 - 1e-9) {
        normalizedMinLongitude = 0.0;
        normalizedMaxLongitude = 360.0;
        return;
    }

    normalizedMinLongitude = wrapTo360(minLongitude);
    normalizedMaxLongitude = wrapTo360(maxLongitude);
}
} // namespace

void AppConfig::normalize() {
    projectionMode = static_cast<ProjectionMode>(qBound(0, static_cast<int>(projectionMode), 4));
    colorScheme = static_cast<ColorScheme>(qBound(0, static_cast<int>(colorScheme), 3));

    if (!std::isfinite(contourStep) || contourStep < 0.001) {
        contourStep = 0.001;
    }

    const double oldMin = valueRange.minimum;
    const double oldMax = valueRange.maximum;
    if (!std::isfinite(oldMin) || !std::isfinite(oldMax)) {
        valueRange.minimum = 0.0;
        valueRange.maximum = 1.0;
    } else {
        valueRange.minimum = std::min(oldMin, oldMax);
        valueRange.maximum = std::max(oldMin, oldMax);
    }

    const double oldCropMinLat = std::isfinite(crop.minLatitude) ? crop.minLatitude : -90.0;
    const double oldCropMaxLat = std::isfinite(crop.maxLatitude) ? crop.maxLatitude : 90.0;
    crop.minLatitude = qBound(-90.0, std::min(oldCropMinLat, oldCropMaxLat), 90.0);
    crop.maxLatitude = qBound(-90.0, std::max(oldCropMinLat, oldCropMaxLat), 90.0);
    normalizeLongitudeBounds(crop.minLongitude, crop.maxLongitude, crop.minLongitude, crop.maxLongitude);

    slice.latitude = qBound(-90.0, std::isfinite(slice.latitude) ? slice.latitude : 0.0, 90.0);
    slice.longitude = wrapTo180(std::isfinite(slice.longitude) ? slice.longitude : 0.0);
    slice.halfWindow = qBound(0.0, std::isfinite(slice.halfWindow) ? slice.halfWindow : 0.0, 180.0);

    if (!lastOpenDirectory.isEmpty() && !QDir(lastOpenDirectory).exists()) {
        lastOpenDirectory.clear();
    }
}

void AppConfig::load(QSettings& settings) {
    projectionMode = static_cast<ProjectionMode>(settings.value("view/projection", static_cast<int>(LatLonProjection)).toInt());
    colorScheme = static_cast<ColorScheme>(settings.value("view/colorScheme", static_cast<int>(BlueGreenRed)).toInt());

    overlays.showGrid = settings.value("view/showGrid", true).toBool();
    overlays.showVectors = settings.value("view/showVectors", false).toBool();
    overlays.showContours = settings.value("view/showContours", false).toBool();
    overlays.showTerminator = settings.value("view/showTerminator", false).toBool();
    overlays.showSubsolarPoint = settings.value("view/showSubsolarPoint", false).toBool();
    overlays.showSubsolarTrack = settings.value("view/showSubsolarTrack", false).toBool();
    overlays.showEquator = settings.value("view/showEquator", false).toBool();
    overlays.showMltLabels = settings.value("view/showMltLabels", false).toBool();

    centerOnNoonMeridian = settings.value("view/centerOnNoon", false).toBool();
    southMirror = settings.value("view/southMirror", false).toBool();
    logScale = settings.value("view/logScale", false).toBool();
    contourStep = settings.value("view/contourStep", 10.0).toDouble();

    crop.enabled = settings.value("view/cropEnabled", false).toBool();
    crop.minLatitude = settings.value("view/cropMinLat", -90.0).toDouble();
    crop.maxLatitude = settings.value("view/cropMaxLat", 90.0).toDouble();
    crop.minLongitude = settings.value("view/cropMinLon", 0.0).toDouble();
    crop.maxLongitude = settings.value("view/cropMaxLon", 360.0).toDouble();

    valueRange.manualEnabled = settings.value("view/manualRangeEnabled", false).toBool();
    valueRange.minimum = settings.value("view/manualMinValue", 0.0).toDouble();
    valueRange.maximum = settings.value("view/manualMaxValue", 1.0).toDouble();

    slice.longitude = settings.value("view/longitudeSlice", 0.0).toDouble();
    slice.latitude = settings.value("view/latitudeSlice", 0.0).toDouble();
    slice.halfWindow = settings.value("view/sliceWindow", 0.0).toDouble();

    lastOpenDirectory = settings.value("lastOpenDirectory", QString()).toString();
    normalize();
}

void AppConfig::save(QSettings& settings) const {
    settings.setValue("view/projection", static_cast<int>(projectionMode));
    settings.setValue("view/colorScheme", static_cast<int>(colorScheme));

    settings.setValue("view/showGrid", overlays.showGrid);
    settings.setValue("view/showVectors", overlays.showVectors);
    settings.setValue("view/showContours", overlays.showContours);
    settings.setValue("view/showTerminator", overlays.showTerminator);
    settings.setValue("view/showSubsolarPoint", overlays.showSubsolarPoint);
    settings.setValue("view/showSubsolarTrack", overlays.showSubsolarTrack);
    settings.setValue("view/showEquator", overlays.showEquator);
    settings.setValue("view/showMltLabels", overlays.showMltLabels);

    settings.setValue("view/centerOnNoon", centerOnNoonMeridian);
    settings.setValue("view/southMirror", southMirror);
    settings.setValue("view/logScale", logScale);
    settings.setValue("view/contourStep", contourStep);

    settings.setValue("view/cropEnabled", crop.enabled);
    settings.setValue("view/cropMinLat", crop.minLatitude);
    settings.setValue("view/cropMaxLat", crop.maxLatitude);
    settings.setValue("view/cropMinLon", crop.minLongitude);
    settings.setValue("view/cropMaxLon", crop.maxLongitude);

    settings.setValue("view/manualRangeEnabled", valueRange.manualEnabled);
    settings.setValue("view/manualMinValue", valueRange.minimum);
    settings.setValue("view/manualMaxValue", valueRange.maximum);

    settings.setValue("view/longitudeSlice", slice.longitude);
    settings.setValue("view/latitudeSlice", slice.latitude);
    settings.setValue("view/sliceWindow", slice.halfWindow);
    settings.setValue("lastOpenDirectory", lastOpenDirectory);
}

} // namespace dataviz::core
