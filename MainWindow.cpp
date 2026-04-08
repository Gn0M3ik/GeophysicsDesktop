#include "MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <limits>

#include "coords/CoordTransformer.h"
#include "coords/MagneticCoords.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kQuantizeScale = 1e6;

qint64 quantizeToMicro(double value) {
    return static_cast<qint64>(std::llround(value * kQuantizeScale));
}

double inferStep(QVector<double> values) {
    if (values.size() < 2) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    double bestStep = std::numeric_limits<double>::max();
    for (int i = 1; i < values.size(); ++i) {
        const double step = values[i] - values[i - 1];
        if (!std::isfinite(step) || std::abs(step) < 1e-12) {
            continue;
        }
        bestStep = std::min(bestStep, std::abs(step));
    }

    return (bestStep == std::numeric_limits<double>::max()) ? 0.0 : bestStep;
}

bool looksLikeMagneticDataset(const GridMetadata& metadata, const QString& sourceName) {
    const QString haystack = (metadata.parameterName + " " + sourceName).toLower();
    return haystack.contains("magnetic") ||
           haystack.contains("geomagnetic") ||
           haystack.contains("mlt") ||
           haystack.contains("aacgm") ||
           haystack.contains("apex");
}

void rebuildMetadata(const QVector<GridPoint>& points, GridMetadata& metadata) {
    const auto finitePointIt = std::find_if(points.cbegin(), points.cend(), [](const GridPoint& point) {
        return std::isfinite(point.latitude) && std::isfinite(point.longitude) && std::isfinite(point.height);
    });
    if (finitePointIt == points.cend()) {
        return;
    }

    metadata.minLat = finitePointIt->latitude;
    metadata.maxLat = finitePointIt->latitude;
    metadata.minLon = finitePointIt->longitude;
    metadata.maxLon = finitePointIt->longitude;
    metadata.minHeight = finitePointIt->height;
    metadata.maxHeight = finitePointIt->height;

    QVector<double> latitudes;
    QVector<double> longitudes;
    QVector<double> heights;
    latitudes.reserve(points.size());
    longitudes.reserve(points.size());
    heights.reserve(points.size());

    QSet<qint64> uniqueLat;
    QSet<qint64> uniqueLon;
    QSet<qint64> uniqueHeight;
    uniqueLat.reserve(points.size());
    uniqueLon.reserve(points.size());
    uniqueHeight.reserve(points.size());

    for (const GridPoint& point : points) {
        if (!std::isfinite(point.latitude) || !std::isfinite(point.longitude) || !std::isfinite(point.height)) {
            continue;
        }

        metadata.minLat = std::min(metadata.minLat, point.latitude);
        metadata.maxLat = std::max(metadata.maxLat, point.latitude);
        metadata.minLon = std::min(metadata.minLon, point.longitude);
        metadata.maxLon = std::max(metadata.maxLon, point.longitude);
        metadata.minHeight = std::min(metadata.minHeight, point.height);
        metadata.maxHeight = std::max(metadata.maxHeight, point.height);

        latitudes.append(point.latitude);
        longitudes.append(point.longitude);
        heights.append(point.height);
        uniqueLat.insert(quantizeToMicro(point.latitude));
        uniqueLon.insert(quantizeToMicro(point.longitude));
        uniqueHeight.insert(quantizeToMicro(point.height));
    }

    metadata.latCount = static_cast<int>(uniqueLat.size());
    metadata.lonCount = static_cast<int>(uniqueLon.size());
    metadata.heightCount = std::max(1, static_cast<int>(uniqueHeight.size()));
    metadata.latStep = inferStep(latitudes);
    metadata.lonStep = inferStep(longitudes);
    metadata.heightStep = inferStep(heights);
    metadata.height = metadata.minHeight;
    metadata.hasHeightDimension = metadata.heightCount > 1 || !qFuzzyCompare(metadata.minHeight, metadata.maxHeight);
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    plotWidget = new PlotWidget(this);
    setCentralWidget(plotWidget);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    setWindowTitle(tr("Data Visualizer"));
    readSettings();
    updateHeightModeAvailability();
}

MainWindow::~MainWindow() {
    writeSettings();
}

void MainWindow::createActions() {
    openScalarAct = new QAction(tr("&Open Scalar File..."), this);
    openScalarAct->setShortcuts(QKeySequence::Open);
    openScalarAct->setStatusTip(tr("Open scalar field data"));
    connect(openScalarAct, &QAction::triggered, this, &MainWindow::openScalarFile);

    openVectorAct = new QAction(tr("Open &Vector File(s)..."), this);
    openVectorAct->setShortcut(QKeySequence(tr("Ctrl+Shift+O")));
    openVectorAct->setStatusTip(tr("Open one vector file or two component files"));
    connect(openVectorAct, &QAction::triggered, this, &MainWindow::openVectorFile);

    exportImageAct = new QAction(tr("&Export Image..."), this);
    exportImageAct->setShortcut(QKeySequence(tr("Ctrl+E")));
    exportImageAct->setStatusTip(tr("Save rendered image to file"));
    connect(exportImageAct, &QAction::triggered, this, &MainWindow::exportImage);

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &MainWindow::close);

    showGridAct = new QAction(tr("Show &Grid"), this);
    showGridAct->setCheckable(true);
    showGridAct->setChecked(true);
    connect(showGridAct, &QAction::toggled, plotWidget, &PlotWidget::setShowGrid);

    showVectorsAct = new QAction(tr("Show &Vectors"), this);
    showVectorsAct->setCheckable(true);
    showVectorsAct->setChecked(false);
    connect(showVectorsAct, &QAction::toggled, plotWidget, &PlotWidget::setShowVectors);

    showTerminatorAct = new QAction(tr("Show &Terminator"), this);
    showTerminatorAct->setCheckable(true);
    showTerminatorAct->setChecked(false);
    connect(showTerminatorAct, &QAction::toggled, plotWidget, &PlotWidget::setShowTerminator);

    showSubsolarPointAct = new QAction(tr("Show Subsolar &Point"), this);
    showSubsolarPointAct->setCheckable(true);
    connect(showSubsolarPointAct, &QAction::toggled, plotWidget, &PlotWidget::setShowSubsolarPoint);

    showSubsolarTrackAct = new QAction(tr("Show Subsolar &Track"), this);
    showSubsolarTrackAct->setCheckable(true);
    connect(showSubsolarTrackAct, &QAction::toggled, plotWidget, &PlotWidget::setShowSubsolarTrack);

    showEquatorAct = new QAction(tr("Show &Equator"), this);
    showEquatorAct->setCheckable(true);
    connect(showEquatorAct, &QAction::toggled, plotWidget, &PlotWidget::setShowEquator);

    showContoursAct = new QAction(tr("Show &Contours"), this);
    showContoursAct->setCheckable(true);
    connect(showContoursAct, &QAction::toggled, plotWidget, &PlotWidget::setShowContours);

    showMltLabelsAct = new QAction(tr("Show &MLT Labels"), this);
    showMltLabelsAct->setCheckable(true);
    connect(showMltLabelsAct, &QAction::toggled, plotWidget, &PlotWidget::setShowMltLabels);

    centerOnNoonAct = new QAction(tr("Center On &Noon Meridian"), this);
    centerOnNoonAct->setCheckable(true);
    connect(centerOnNoonAct, &QAction::toggled, plotWidget, &PlotWidget::setCenterOnNoonMeridian);

    southMirrorAct = new QAction(tr("&Mirror South Polar"), this);
    southMirrorAct->setCheckable(true);
    connect(southMirrorAct, &QAction::toggled, plotWidget, &PlotWidget::setSouthMirror);

    logScaleAct = new QAction(tr("&Logarithmic Scale"), this);
    logScaleAct->setCheckable(true);
    connect(logScaleAct, &QAction::toggled, plotWidget, &PlotWidget::setLogScale);

    resetViewAct = new QAction(tr("&Reset View"), this);
    resetViewAct->setShortcut(QKeySequence(tr("Ctrl+0")));
    connect(resetViewAct, &QAction::triggered, plotWidget, &PlotWidget::resetView);

    projectionGroup = new QActionGroup(this);
    projectionGroup->setExclusive(true);

    projectionLatLonAct = new QAction(tr("Lat/Lon"), this);
    projectionLatLonAct->setCheckable(true);
    projectionLatLonAct->setChecked(true);
    projectionGroup->addAction(projectionLatLonAct);
    connect(projectionLatLonAct, &QAction::triggered, this, [this]() {
        applyProjectionSelection(PlotWidget::LatLonProjection);
    });

    projectionNorthPolarAct = new QAction(tr("North Polar"), this);
    projectionNorthPolarAct->setCheckable(true);
    projectionGroup->addAction(projectionNorthPolarAct);
    connect(projectionNorthPolarAct, &QAction::triggered, this, [this]() {
        applyProjectionSelection(PlotWidget::NorthPolarProjection);
    });

    projectionSouthPolarAct = new QAction(tr("South Polar"), this);
    projectionSouthPolarAct->setCheckable(true);
    projectionGroup->addAction(projectionSouthPolarAct);
    connect(projectionSouthPolarAct, &QAction::triggered, this, [this]() {
        applyProjectionSelection(PlotWidget::SouthPolarProjection);
    });

    projectionLatHeightAct = new QAction(tr("Latitude-Height"), this);
    projectionLatHeightAct->setCheckable(true);
    projectionGroup->addAction(projectionLatHeightAct);
    connect(projectionLatHeightAct, &QAction::triggered, this, [this]() {
        applyProjectionSelection(PlotWidget::LatitudeHeightProjection);
    });

    projectionLonHeightAct = new QAction(tr("Longitude-Height"), this);
    projectionLonHeightAct->setCheckable(true);
    projectionGroup->addAction(projectionLonHeightAct);
    connect(projectionLonHeightAct, &QAction::triggered, this, [this]() {
        applyProjectionSelection(PlotWidget::LongitudeHeightProjection);
    });

    colorSchemeGroup = new QActionGroup(this);
    colorSchemeGroup->setExclusive(true);

    schemeGrayscaleAct = new QAction(tr("Grayscale"), this);
    schemeGrayscaleAct->setCheckable(true);
    colorSchemeGroup->addAction(schemeGrayscaleAct);
    connect(schemeGrayscaleAct, &QAction::triggered, this, [this]() { plotWidget->setColorScheme(0); });

    schemeRainbowAct = new QAction(tr("Rainbow"), this);
    schemeRainbowAct->setCheckable(true);
    colorSchemeGroup->addAction(schemeRainbowAct);
    connect(schemeRainbowAct, &QAction::triggered, this, [this]() { plotWidget->setColorScheme(1); });

    schemeBlueGreenRedAct = new QAction(tr("Blue-Green-Red"), this);
    schemeBlueGreenRedAct->setCheckable(true);
    schemeBlueGreenRedAct->setChecked(true);
    colorSchemeGroup->addAction(schemeBlueGreenRedAct);
    connect(schemeBlueGreenRedAct, &QAction::triggered, this, [this]() { plotWidget->setColorScheme(2); });

    schemeBlueRedAct = new QAction(tr("Blue-Red"), this);
    schemeBlueRedAct->setCheckable(true);
    colorSchemeGroup->addAction(schemeBlueRedAct);
    connect(schemeBlueRedAct, &QAction::triggered, this, [this]() { plotWidget->setColorScheme(3); });

    setManualRangeAct = new QAction(tr("Set Manual Value Range..."), this);
    connect(setManualRangeAct, &QAction::triggered, this, &MainWindow::setManualValueRange);

    clearManualRangeAct = new QAction(tr("Use Auto Value Range"), this);
    connect(clearManualRangeAct, &QAction::triggered, this, &MainWindow::clearManualValueRange);

    setCropAct = new QAction(tr("Set Crop Region..."), this);
    connect(setCropAct, &QAction::triggered, this, &MainWindow::setCropRegion);

    clearCropAct = new QAction(tr("Clear Crop Region"), this);
    connect(clearCropAct, &QAction::triggered, this, &MainWindow::clearCropRegion);

    contourStepAct = new QAction(tr("Set Contour Step..."), this);
    connect(contourStepAct, &QAction::triggered, this, &MainWindow::setContourStep);

    setLongitudeSliceAct = new QAction(tr("Set Longitude Slice..."), this);
    connect(setLongitudeSliceAct, &QAction::triggered, this, &MainWindow::setLongitudeSlice);

    setLatitudeSliceAct = new QAction(tr("Set Latitude Slice..."), this);
    connect(setLatitudeSliceAct, &QAction::triggered, this, &MainWindow::setLatitudeSlice);

    setSliceWindowAct = new QAction(tr("Set Slice Window..."), this);
    connect(setSliceWindowAct, &QAction::triggered, this, &MainWindow::setSliceWindow);

    convertCoordsAct = new QAction(tr("Convert Coordinates..."), this);
    convertCoordsAct->setShortcut(QKeySequence(tr("Ctrl+Shift+C")));
    convertCoordsAct->setStatusTip(tr("Convert coordinates between geographic and magnetic systems"));
    connect(convertCoordsAct, &QAction::triggered, this, &MainWindow::convertCoordinates);

    aboutAct = new QAction(tr("&About"), this);
    connect(aboutAct, &QAction::triggered, this, &MainWindow::about);

    aboutQtAct = new QAction(tr("About &Qt"), this);
    connect(aboutQtAct, &QAction::triggered, this, &MainWindow::aboutQt);
}

void MainWindow::createMenus() {
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openScalarAct);
    fileMenu->addAction(openVectorAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exportImageAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(showGridAct);
    viewMenu->addAction(showVectorsAct);
    viewMenu->addAction(showContoursAct);
    viewMenu->addSeparator();
    viewMenu->addAction(showTerminatorAct);
    viewMenu->addAction(showSubsolarPointAct);
    viewMenu->addAction(showSubsolarTrackAct);
    viewMenu->addAction(showEquatorAct);
    viewMenu->addSeparator();
    viewMenu->addAction(showMltLabelsAct);
    viewMenu->addAction(centerOnNoonAct);
    viewMenu->addAction(southMirrorAct);
    viewMenu->addAction(logScaleAct);
    viewMenu->addSeparator();
    viewMenu->addAction(resetViewAct);
    viewMenu->addAction(contourStepAct);

    projectionMenu = viewMenu->addMenu(tr("Projection"));
    projectionMenu->addAction(projectionLatLonAct);
    projectionMenu->addAction(projectionNorthPolarAct);
    projectionMenu->addAction(projectionSouthPolarAct);
    projectionMenu->addSeparator();
    projectionMenu->addAction(projectionLatHeightAct);
    projectionMenu->addAction(projectionLonHeightAct);

    colorSchemeMenu = viewMenu->addMenu(tr("Color Scheme"));
    colorSchemeMenu->addAction(schemeGrayscaleAct);
    colorSchemeMenu->addAction(schemeRainbowAct);
    colorSchemeMenu->addAction(schemeBlueGreenRedAct);
    colorSchemeMenu->addAction(schemeBlueRedAct);

    dataRangeMenu = viewMenu->addMenu(tr("Value Range"));
    dataRangeMenu->addAction(setManualRangeAct);
    dataRangeMenu->addAction(clearManualRangeAct);

    cropMenu = viewMenu->addMenu(tr("Crop"));
    cropMenu->addAction(setCropAct);
    cropMenu->addAction(clearCropAct);

    QMenu* sliceMenu = viewMenu->addMenu(tr("Height Slice"));
    sliceMenu->addAction(setLongitudeSliceAct);
    sliceMenu->addAction(setLatitudeSliceAct);
    sliceMenu->addAction(setSliceWindowAct);
    viewMenu->addSeparator();
    viewMenu->addAction(convertCoordsAct);

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);
    helpMenu->addAction(aboutQtAct);
}

void MainWindow::createToolBars() {
    QToolBar* fileToolbar = addToolBar(tr("File"));
    fileToolbar->setObjectName("fileToolbar");
    fileToolbar->addAction(openScalarAct);
    fileToolbar->addAction(openVectorAct);
    fileToolbar->addAction(exportImageAct);

    QToolBar* viewToolbar = addToolBar(tr("View"));
    viewToolbar->setObjectName("viewToolbar");
    viewToolbar->addAction(showGridAct);
    viewToolbar->addAction(showVectorsAct);
    viewToolbar->addAction(showContoursAct);
    viewToolbar->addAction(showTerminatorAct);
    viewToolbar->addAction(resetViewAct);
    viewToolbar->addSeparator();
    viewToolbar->addAction(convertCoordsAct);
}

void MainWindow::createStatusBar() {
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::applyProjectionSelection(PlotWidget::ProjectionMode mode) {
    plotWidget->setProjectionMode(mode);
    const bool southPolar = (mode == PlotWidget::SouthPolarProjection);
    southMirrorAct->setEnabled(southPolar);
    if (!southPolar) {
        southMirrorAct->setChecked(false);
    }

    const bool geoProjection = !plotWidget->isHeightProjection();
    showTerminatorAct->setEnabled(geoProjection);
    showSubsolarPointAct->setEnabled(geoProjection);
    showSubsolarTrackAct->setEnabled(geoProjection);
    showEquatorAct->setEnabled(geoProjection);
    showMltLabelsAct->setEnabled(geoProjection);
    centerOnNoonAct->setEnabled(true);

    if (!geoProjection) {
        showTerminatorAct->setChecked(false);
        showSubsolarPointAct->setChecked(false);
        showSubsolarTrackAct->setChecked(false);
        showEquatorAct->setChecked(false);
        showMltLabelsAct->setChecked(false);
    }
}

void MainWindow::updateHeightModeAvailability() {
    const bool hasHeightData = dataMetadata.hasHeightDimension || dataMetadata.heightCount > 1;
    projectionLatHeightAct->setEnabled(hasHeightData);
    projectionLonHeightAct->setEnabled(hasHeightData);
    setLongitudeSliceAct->setEnabled(hasHeightData);
    setLatitudeSliceAct->setEnabled(hasHeightData);
    setSliceWindowAct->setEnabled(hasHeightData);

    if (!hasHeightData && plotWidget->isHeightProjection()) {
        projectionLatLonAct->setChecked(true);
        applyProjectionSelection(PlotWidget::LatLonProjection);
    }
}

void MainWindow::readSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    const QByteArray geometry = settings.value("geometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    } else {
        const QScreen* targetScreen = screen() ? screen() : QGuiApplication::primaryScreen();
        const QRect available = targetScreen ? targetScreen->availableGeometry() : QRect(0, 0, 1920, 1080);
        const int preferredWidth = std::max(1000, static_cast<int>(available.width() * 0.72));
        const int preferredHeight = std::max(700, static_cast<int>(available.height() * 0.72));
        resize(std::min(preferredWidth, available.width()), std::min(preferredHeight, available.height()));
        move(available.center() - rect().center());
    }

    lastOpenDirectory = settings.value(
                            "lastOpenDirectory",
                            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                            .toString();
    if (lastOpenDirectory.isEmpty() || !QDir(lastOpenDirectory).exists()) {
        lastOpenDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    showGridAct->setChecked(settings.value("view/showGrid", true).toBool());
    showVectorsAct->setChecked(settings.value("view/showVectors", false).toBool());
    showContoursAct->setChecked(settings.value("view/showContours", false).toBool());
    showTerminatorAct->setChecked(settings.value("view/showTerminator", false).toBool());
    showSubsolarPointAct->setChecked(settings.value("view/showSubsolarPoint", false).toBool());
    showSubsolarTrackAct->setChecked(settings.value("view/showSubsolarTrack", false).toBool());
    showEquatorAct->setChecked(settings.value("view/showEquator", false).toBool());
    showMltLabelsAct->setChecked(settings.value("view/showMltLabels", false).toBool());
    centerOnNoonAct->setChecked(settings.value("view/centerOnNoon", false).toBool());
    southMirrorAct->setChecked(settings.value("view/southMirror", false).toBool());
    logScaleAct->setChecked(settings.value("view/logScale", false).toBool());
    plotWidget->setLongitudeSlice(settings.value("view/longitudeSlice", 0.0).toDouble());
    plotWidget->setLatitudeSlice(settings.value("view/latitudeSlice", 0.0).toDouble());
    plotWidget->setSliceWindow(settings.value("view/sliceWindow", 0.0).toDouble());

    const int projection = settings.value("view/projection", static_cast<int>(PlotWidget::LatLonProjection)).toInt();
    switch (projection) {
    case static_cast<int>(PlotWidget::NorthPolarProjection):
        projectionNorthPolarAct->setChecked(true);
        applyProjectionSelection(PlotWidget::NorthPolarProjection);
        break;
    case static_cast<int>(PlotWidget::SouthPolarProjection):
        projectionSouthPolarAct->setChecked(true);
        applyProjectionSelection(PlotWidget::SouthPolarProjection);
        break;
    case static_cast<int>(PlotWidget::LatitudeHeightProjection):
        projectionLatHeightAct->setChecked(true);
        applyProjectionSelection(PlotWidget::LatitudeHeightProjection);
        break;
    case static_cast<int>(PlotWidget::LongitudeHeightProjection):
        projectionLonHeightAct->setChecked(true);
        applyProjectionSelection(PlotWidget::LongitudeHeightProjection);
        break;
    case static_cast<int>(PlotWidget::LatLonProjection):
    default:
        projectionLatLonAct->setChecked(true);
        applyProjectionSelection(PlotWidget::LatLonProjection);
        break;
    }

    const int colorScheme = settings.value("view/colorScheme", 2).toInt();
    switch (colorScheme) {
    case 0:
        schemeGrayscaleAct->setChecked(true);
        break;
    case 1:
        schemeRainbowAct->setChecked(true);
        break;
    case 3:
        schemeBlueRedAct->setChecked(true);
        break;
    case 2:
    default:
        schemeBlueGreenRedAct->setChecked(true);
        break;
    }
    plotWidget->setColorScheme(colorScheme);

    plotWidget->setContourStep(settings.value("view/contourStep", 10.0).toDouble());

    const bool manualRangeEnabled = settings.value("view/manualRangeEnabled", false).toBool();
    if (manualRangeEnabled) {
        const double minValue = settings.value("view/manualMinValue", -100.0).toDouble();
        const double maxValue = settings.value("view/manualMaxValue", 100.0).toDouble();
        plotWidget->setManualValueRange(minValue, maxValue);
    } else {
        plotWidget->clearManualValueRange();
    }

    const bool cropEnabled = settings.value("view/cropEnabled", false).toBool();
    if (cropEnabled) {
        const double minLat = settings.value("view/cropMinLat", -90.0).toDouble();
        const double maxLat = settings.value("view/cropMaxLat", 90.0).toDouble();
        const double minLon = settings.value("view/cropMinLon", 0.0).toDouble();
        const double maxLon = settings.value("view/cropMaxLon", 360.0).toDouble();
        plotWidget->setCropRegion(true, minLat, maxLat, minLon, maxLon);
    } else {
        plotWidget->clearCropRegion();
    }
}

void MainWindow::writeSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("geometry", saveGeometry());
    settings.setValue("lastOpenDirectory", lastOpenDirectory);
    settings.setValue("view/showGrid", showGridAct->isChecked());
    settings.setValue("view/showVectors", showVectorsAct->isChecked());
    settings.setValue("view/showContours", showContoursAct->isChecked());
    settings.setValue("view/showTerminator", showTerminatorAct->isChecked());
    settings.setValue("view/showSubsolarPoint", showSubsolarPointAct->isChecked());
    settings.setValue("view/showSubsolarTrack", showSubsolarTrackAct->isChecked());
    settings.setValue("view/showEquator", showEquatorAct->isChecked());
    settings.setValue("view/showMltLabels", showMltLabelsAct->isChecked());
    settings.setValue("view/centerOnNoon", centerOnNoonAct->isChecked());
    settings.setValue("view/southMirror", southMirrorAct->isChecked());
    settings.setValue("view/logScale", logScaleAct->isChecked());
    settings.setValue("view/projection", static_cast<int>(plotWidget->projectionMode()));
    settings.setValue("view/longitudeSlice", plotWidget->longitudeSlice());
    settings.setValue("view/latitudeSlice", plotWidget->latitudeSlice());
    settings.setValue("view/sliceWindow", plotWidget->sliceWindow());
    settings.setValue("view/colorScheme", plotWidget->colorScheme());
    settings.setValue("view/contourStep", plotWidget->contourStep());
    settings.setValue("view/manualRangeEnabled", plotWidget->hasManualValueRange());
    settings.setValue("view/manualMinValue", plotWidget->manualMinValue());
    settings.setValue("view/manualMaxValue", plotWidget->manualMaxValue());
    settings.setValue("view/cropEnabled", plotWidget->cropEnabled());
    settings.setValue("view/cropMinLat", plotWidget->cropMinLat());
    settings.setValue("view/cropMaxLat", plotWidget->cropMaxLat());
    settings.setValue("view/cropMinLon", plotWidget->cropMinLon());
    settings.setValue("view/cropMaxLon", plotWidget->cropMaxLon());
}

void MainWindow::openScalarFile() {
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open Scalar Data File"),
        lastOpenDirectory,
        tr("Data Files (*.00 *.txt *.dat);;All Files (*)"));

    if (!fileName.isEmpty()) {
        lastOpenDirectory = QFileInfo(fileName).absolutePath();
        loadFile(fileName, DataParser::SCALAR_DATA);
    }
}

void MainWindow::openVectorFile() {
    QFileDialog dialog(
        this,
        tr("Open Vector File(s)"),
        lastOpenDirectory,
        tr("Data Files (*.00 *.txt *.dat);;All Files (*)"));
    dialog.setFileMode(QFileDialog::ExistingFiles);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QStringList selectedFiles = dialog.selectedFiles();
    if (selectedFiles.isEmpty()) {
        return;
    }

    if (selectedFiles.size() > 2) {
        QMessageBox::warning(
            this,
            tr("Vector Selection"),
            tr("Select either one vector file or exactly two component files (X and Y)."));
        return;
    }

    if (selectedFiles.size() == 2 && QFileInfo(selectedFiles[0]) == QFileInfo(selectedFiles[1])) {
        QMessageBox::warning(
            this,
            tr("Vector Selection"),
            tr("The same file was selected twice. Choose two different component files."));
        return;
    }

    lastOpenDirectory = QFileInfo(selectedFiles.first()).absolutePath();

    if (selectedFiles.size() == 1) {
        loadFile(selectedFiles.first(), DataParser::VECTOR_DATA);
        return;
    }

    QVector<GridPoint> points;
    GridMetadata metadata;
    if (DataParser::parseVectorComponents(selectedFiles[0], selectedFiles[1], metadata, points)) {
        dataPoints = points;
        dataMetadata = metadata;
        curFile = QString("%1 + %2")
                      .arg(QFileInfo(selectedFiles[0]).fileName())
                      .arg(QFileInfo(selectedFiles[1]).fileName());

        const bool hasVectorComponents = std::any_of(
            dataPoints.cbegin(),
            dataPoints.cend(),
            [](const GridPoint& point) {
                return std::isfinite(point.vx) &&
                       std::isfinite(point.vy) &&
                       (std::abs(point.vx) > 1e-12 || std::abs(point.vy) > 1e-12 || std::abs(point.vz) > 1e-12);
            });

        plotWidget->setData(dataPoints, dataMetadata);
        const bool magneticDataset = looksLikeMagneticDataset(dataMetadata, curFile);
        plotWidget->setMagneticMode(magneticDataset);
        if (magneticDataset) {
            centerOnNoonAct->setChecked(true);
            showMltLabelsAct->setChecked(false);
            schemeBlueRedAct->setChecked(true);
        }
        updateHeightModeAvailability();
        showVectorsAct->setChecked(hasVectorComponents);
        setWindowTitle(tr("%1 - Data Visualizer").arg(curFile));
        statusBar()->showMessage(
            tr("Loaded vector components: %1 points (%2)")
                .arg(points.size())
                .arg(hasVectorComponents ? tr("vector") : tr("scalar")),
            5000);
    } else {
        QMessageBox::critical(
            this,
            tr("Vector Load Error"),
            tr("Could not combine selected vector component files.\n\nError: %1")
                .arg(DataParser::getLastError()));
    }
}

void MainWindow::loadFile(const QString& fileName, DataParser::DataType dataType) {
    statusBar()->showMessage(tr("Loading file..."));

    QVector<GridPoint> points;
    GridMetadata metadata;

    if (DataParser::parseFile(fileName, metadata, points, dataType)) {
        dataPoints = points;
        dataMetadata = metadata;
        curFile = fileName;

        const bool hasVectorComponents = std::any_of(
            dataPoints.cbegin(),
            dataPoints.cend(),
            [](const GridPoint& point) {
                return std::abs(point.vx) > 0.0 || std::abs(point.vy) > 0.0 || std::abs(point.vz) > 0.0;
            });

        plotWidget->setData(dataPoints, dataMetadata);
        const bool magneticDataset = looksLikeMagneticDataset(dataMetadata, fileName);
        plotWidget->setMagneticMode(magneticDataset);
        if (magneticDataset) {
            centerOnNoonAct->setChecked(true);
            showMltLabelsAct->setChecked(false);
            schemeBlueRedAct->setChecked(true);
        }
        updateHeightModeAvailability();
        if (dataType == DataParser::VECTOR_DATA) {
            showVectorsAct->setChecked(hasVectorComponents);
        } else if (!hasVectorComponents) {
            showVectorsAct->setChecked(false);
        }

        setWindowTitle(tr("%1 - Data Visualizer").arg(strippedName(curFile)));
        statusBar()->showMessage(
            tr("Loaded %1 points (%2)")
                .arg(dataPoints.size())
                .arg(hasVectorComponents ? tr("vector") : tr("scalar")),
            4000);
    } else {
        QMessageBox::critical(
            this,
            tr("Error"),
            tr("Could not load file:\n%1\n\nError: %2")
                .arg(QDir::toNativeSeparators(fileName))
                .arg(DataParser::getLastError()));
        statusBar()->showMessage(tr("Loading failed"), 4000);
    }
}

void MainWindow::exportImage() {
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export Image"),
        QDir(lastOpenDirectory).filePath("plot.png"),
        tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;BMP Image (*.bmp)"));

    if (filePath.isEmpty()) {
        return;
    }

    bool okWidth = false;
    const int defaultWidth = std::max(1280, plotWidget->width() * 2);
    const int width = QInputDialog::getInt(
        this,
        tr("Image Width"),
        tr("Width (px):"),
        defaultWidth,
        320,
        16000,
        10,
        &okWidth);
    if (!okWidth) {
        return;
    }

    bool okHeight = false;
    const int defaultHeight = std::max(900, plotWidget->height() * 2);
    const int height = QInputDialog::getInt(
        this,
        tr("Image Height"),
        tr("Height (px):"),
        defaultHeight,
        240,
        16000,
        10,
        &okHeight);
    if (!okHeight) {
        return;
    }

    bool okMode = false;
    const QStringList exportModes = {
        tr("Color"),
        tr("Grayscale (B/W)")
    };
    const QString selectedMode = QInputDialog::getItem(
        this,
        tr("Image Mode"),
        tr("Export mode:"),
        exportModes,
        0,
        false,
        &okMode);
    if (!okMode) {
        return;
    }

    const bool exportAsGrayscale = (selectedMode == exportModes[1]);

    if (plotWidget->exportImage(filePath, QSize(width, height), 95, exportAsGrayscale)) {
        statusBar()->showMessage(tr("Image exported: %1").arg(QDir::toNativeSeparators(filePath)), 5000);
    } else {
        QMessageBox::critical(this, tr("Export Error"), tr("Failed to save image file."));
    }
}

void MainWindow::setManualValueRange() {
    bool okMin = false;
    const double initialMin = plotWidget->hasManualValueRange() ? plotWidget->manualMinValue() : -100.0;
    const double minimum = QInputDialog::getDouble(
        this,
        tr("Manual Value Range"),
        tr("Minimum value:"),
        initialMin,
        -1e12,
        1e12,
        6,
        &okMin);
    if (!okMin) {
        return;
    }

    bool okMax = false;
    const double initialMax = plotWidget->hasManualValueRange() ? plotWidget->manualMaxValue() : 100.0;
    const double maximum = QInputDialog::getDouble(
        this,
        tr("Manual Value Range"),
        tr("Maximum value:"),
        initialMax,
        -1e12,
        1e12,
        6,
        &okMax);
    if (!okMax) {
        return;
    }

    plotWidget->setManualValueRange(minimum, maximum);
    statusBar()->showMessage(tr("Manual value range applied"), 3000);
}

void MainWindow::clearManualValueRange() {
    plotWidget->clearManualValueRange();
    statusBar()->showMessage(tr("Auto value range enabled"), 3000);
}

void MainWindow::setCropRegion() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Crop Region"));

    QFormLayout layout(&dialog);
    QDoubleSpinBox minLatSpin;
    QDoubleSpinBox maxLatSpin;
    QDoubleSpinBox minLonSpin;
    QDoubleSpinBox maxLonSpin;

    minLatSpin.setRange(-90.0, 90.0);
    maxLatSpin.setRange(-90.0, 90.0);
    minLonSpin.setRange(-720.0, 720.0);
    maxLonSpin.setRange(-720.0, 720.0);

    minLatSpin.setDecimals(4);
    maxLatSpin.setDecimals(4);
    minLonSpin.setDecimals(4);
    maxLonSpin.setDecimals(4);

    const double initMinLat = plotWidget->cropEnabled() ? plotWidget->cropMinLat() : std::min(dataMetadata.minLat, dataMetadata.maxLat);
    const double initMaxLat = plotWidget->cropEnabled() ? plotWidget->cropMaxLat() : std::max(dataMetadata.minLat, dataMetadata.maxLat);
    const double initMinLon = plotWidget->cropEnabled() ? plotWidget->cropMinLon() : dataMetadata.minLon;
    const double initMaxLon = plotWidget->cropEnabled() ? plotWidget->cropMaxLon() : dataMetadata.maxLon;

    minLatSpin.setValue(initMinLat);
    maxLatSpin.setValue(initMaxLat);
    minLonSpin.setValue(initMinLon);
    maxLonSpin.setValue(initMaxLon);

    layout.addRow(tr("Min latitude"), &minLatSpin);
    layout.addRow(tr("Max latitude"), &maxLatSpin);
    layout.addRow(tr("Min longitude"), &minLonSpin);
    layout.addRow(tr("Max longitude"), &maxLonSpin);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    plotWidget->setCropRegion(true, minLatSpin.value(), maxLatSpin.value(), minLonSpin.value(), maxLonSpin.value());
    statusBar()->showMessage(tr("Crop region applied"), 3000);
}

void MainWindow::clearCropRegion() {
    plotWidget->clearCropRegion();
    statusBar()->showMessage(tr("Crop region cleared"), 3000);
}

void MainWindow::setContourStep() {
    bool ok = false;
    const double step = QInputDialog::getDouble(
        this,
        tr("Contour Step"),
        tr("Contour interval:"),
        plotWidget->contourStep(),
        0.001,
        1e9,
        3,
        &ok);
    if (!ok) {
        return;
    }
    plotWidget->setContourStep(step);
    statusBar()->showMessage(tr("Contour step set to %1").arg(step), 3000);
}

void MainWindow::setLongitudeSlice() {
    bool ok = false;
    const double value = QInputDialog::getDouble(
        this,
        tr("Longitude Slice"),
        tr("Slice longitude (degrees):"),
        plotWidget->longitudeSlice(),
        -180.0,
        180.0,
        4,
        &ok);
    if (!ok) {
        return;
    }

    plotWidget->setLongitudeSlice(value);
    statusBar()->showMessage(
        tr("Longitude slice set to %1 deg (effective: %2 deg)")
            .arg(QString::number(value, 'f', 2))
            .arg(QString::number(plotWidget->resolvedLongitudeSlice(), 'f', 2)),
        3500);
}

void MainWindow::setLatitudeSlice() {
    bool ok = false;
    const double value = QInputDialog::getDouble(
        this,
        tr("Latitude Slice"),
        tr("Slice latitude (degrees):"),
        plotWidget->latitudeSlice(),
        -90.0,
        90.0,
        4,
        &ok);
    if (!ok) {
        return;
    }

    plotWidget->setLatitudeSlice(value);
    statusBar()->showMessage(
        tr("Latitude slice set to %1 deg (effective: %2 deg)")
            .arg(QString::number(value, 'f', 2))
            .arg(QString::number(plotWidget->resolvedLatitudeSlice(), 'f', 2)),
        3500);
}

void MainWindow::setSliceWindow() {
    bool ok = false;
    const double value = QInputDialog::getDouble(
        this,
        tr("Slice Window"),
        tr("Half-window around selected slice (degrees):"),
        plotWidget->sliceWindow(),
        0.0,
        180.0,
        4,
        &ok);
    if (!ok) {
        return;
    }

    plotWidget->setSliceWindow(value);
    statusBar()->showMessage(tr("Slice half-window set to +/- %1 deg").arg(QString::number(value, 'f', 2)), 3000);
}

void MainWindow::convertCoordinates() {
    using dataviz::coords::GeoCoord;
    using dataviz::coords::MagCoord;
    using dataviz::coords::CoordTransformer;
    using dataviz::coords::MagneticCoords;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Coordinate Converter"));

    auto* layout = new QFormLayout(&dialog);

    auto* directionCombo = new QComboBox(&dialog);
    directionCombo->addItem(tr("Geographic -> Magnetic"));
    directionCombo->addItem(tr("Magnetic -> Geographic"));

    auto* latitudeSpin = new QDoubleSpinBox(&dialog);
    latitudeSpin->setRange(-90.0, 90.0);
    latitudeSpin->setDecimals(6);
    latitudeSpin->setValue(0.0);

    auto* longitudeSpin = new QDoubleSpinBox(&dialog);
    longitudeSpin->setRange(-180.0, 180.0);
    longitudeSpin->setDecimals(6);
    longitudeSpin->setValue(0.0);
    longitudeSpin->setSuffix(tr(" deg"));

    auto* heightSpin = new QDoubleSpinBox(&dialog);
    heightSpin->setRange(-1e6, 1e7);
    heightSpin->setDecimals(3);
    heightSpin->setValue(0.0);
    heightSpin->setSuffix(tr(" km"));

    auto* poleLatSpin = new QDoubleSpinBox(&dialog);
    poleLatSpin->setRange(-90.0, 90.0);
    poleLatSpin->setDecimals(4);
    poleLatSpin->setValue(80.65);
    poleLatSpin->setSuffix(tr(" deg"));

    auto* poleLonSpin = new QDoubleSpinBox(&dialog);
    poleLonSpin->setRange(-180.0, 180.0);
    poleLonSpin->setDecimals(4);
    poleLonSpin->setValue(-72.68);
    poleLonSpin->setSuffix(tr(" deg"));

    auto* subsolarLonSpin = new QDoubleSpinBox(&dialog);
    subsolarLonSpin->setRange(-180.0, 180.0);
    subsolarLonSpin->setDecimals(4);
    subsolarLonSpin->setSuffix(tr(" deg"));
    if (dataMetadata.dateTime.isValid()) {
        subsolarLonSpin->setValue(MagneticCoords::estimateSubsolarPoint(dataMetadata.dateTime).longitude);
    } else {
        subsolarLonSpin->setValue(0.0);
    }

    auto* inputMltCheck = new QCheckBox(tr("Input longitude is MLT (hours)"), &dialog);
    inputMltCheck->setChecked(false);

    auto* outputMltCheck = new QCheckBox(tr("Compute output MLT"), &dialog);
    outputMltCheck->setChecked(true);

    auto* resultLabel = new QLabel(&dialog);
    resultLabel->setWordWrap(true);
    resultLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    resultLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    resultLabel->setMinimumHeight(64);
    resultLabel->setText(tr("Result will appear here."));

    auto* longitudeLabel = new QLabel(tr("Longitude (deg):"), &dialog);

    layout->addRow(tr("Direction"), directionCombo);
    layout->addRow(tr("Latitude (deg)"), latitudeSpin);
    layout->addRow(longitudeLabel, longitudeSpin);
    layout->addRow(tr("Height"), heightSpin);
    layout->addRow(tr("Pole latitude"), poleLatSpin);
    layout->addRow(tr("Pole longitude"), poleLonSpin);
    layout->addRow(tr("Subsolar longitude"), subsolarLonSpin);
    layout->addRow(inputMltCheck);
    layout->addRow(outputMltCheck);
    layout->addRow(tr("Result"), resultLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto* convertButton = buttons->addButton(tr("Convert"), QDialogButtonBox::ActionRole);
    auto* applyDataButton = buttons->addButton(tr("Convert Loaded Data and Plot"), QDialogButtonBox::ActionRole);
    convertButton->setDefault(true);
    applyDataButton->setEnabled(!dataPoints.isEmpty());
    layout->addRow(buttons);

    const auto updateInputUi = [=]() {
        const bool geoToMag = directionCombo->currentIndex() == 0;

        inputMltCheck->setVisible(!geoToMag);
        outputMltCheck->setVisible(geoToMag);

        const bool inputIsMlt = !geoToMag && inputMltCheck->isChecked();
        const double previousValue = longitudeSpin->value();

        if (inputIsMlt) {
            longitudeLabel->setText(tr("MLT (hours):"));
            longitudeSpin->setRange(0.0, 24.0);
            longitudeSpin->setDecimals(4);
            longitudeSpin->setSuffix(tr(" h"));
        } else {
            longitudeLabel->setText(geoToMag ? tr("Longitude (deg):") : tr("Magnetic longitude (deg):"));
            longitudeSpin->setRange(-180.0, 180.0);
            longitudeSpin->setDecimals(6);
            longitudeSpin->setSuffix(tr(" deg"));
        }

        longitudeSpin->setValue(qBound(longitudeSpin->minimum(), previousValue, longitudeSpin->maximum()));
    };

    const auto convertNow = [=]() {
        const bool geoToMag = directionCombo->currentIndex() == 0;
        const double latitude = latitudeSpin->value();
        const double longitudeOrMlt = longitudeSpin->value();
        const double height = heightSpin->value();
        const double poleLatitude = poleLatSpin->value();
        const double poleLongitude = poleLonSpin->value();
        const double subsolarLongitude = subsolarLonSpin->value();

        if (geoToMag) {
            const GeoCoord geo {latitude, longitudeOrMlt, height};
            const MagCoord mag = MagneticCoords::toSimpleMagnetic(
                geo,
                poleLatitude,
                poleLongitude,
                subsolarLongitude,
                outputMltCheck->isChecked());

            if (!std::isfinite(mag.latitude) || !std::isfinite(mag.longitude) || !std::isfinite(mag.height)) {
                resultLabel->setText(tr("Conversion failed: non-finite result values."));
                return;
            }

            QString result = tr("Magnetic lat: %1 deg\nMagnetic lon: %2 deg\nHeight: %3 km")
                                 .arg(QString::number(mag.latitude, 'f', 6))
                                 .arg(QString::number(mag.longitude, 'f', 6))
                                 .arg(QString::number(mag.height, 'f', 3));

            if (outputMltCheck->isChecked() && std::isfinite(mag.mlt)) {
                result += tr("\nMLT: %1 h").arg(QString::number(mag.mlt, 'f', 4));
            }

            resultLabel->setText(result);
            return;
        }

        MagCoord mag;
        mag.latitude = latitude;
        mag.height = height;
        const bool inputIsMlt = inputMltCheck->isChecked();
        if (inputIsMlt) {
            mag.mlt = longitudeOrMlt;
        } else {
            mag.longitude = longitudeOrMlt;
            mag.mlt = MagneticCoords::longitudeToMlt(mag.longitude, subsolarLongitude);
        }

        const GeoCoord geo = MagneticCoords::toGeographic(
            mag,
            poleLatitude,
            poleLongitude,
            inputIsMlt,
            subsolarLongitude);

        if (!std::isfinite(geo.latitude) || !std::isfinite(geo.longitude) || !std::isfinite(geo.height)) {
            resultLabel->setText(tr("Conversion failed: non-finite result values."));
            return;
        }

        resultLabel->setText(
            tr("Geographic lat: %1 deg\nGeographic lon: %2 deg\nHeight: %3 km")
                .arg(QString::number(geo.latitude, 'f', 6))
                .arg(QString::number(geo.longitude, 'f', 6))
                .arg(QString::number(geo.height, 'f', 3)));
    };

    const auto applyDatasetConversion = [&, this]() {
        if (dataPoints.isEmpty()) {
            QMessageBox::information(
                &dialog,
                tr("No Data"),
                tr("Load a file first, then convert coordinates for plotting."));
            return;
        }

        const bool geoToMag = directionCombo->currentIndex() == 0;
        const bool inputIsMlt = inputMltCheck->isChecked();
        const double poleLatitude = poleLatSpin->value();
        const double poleLongitude = poleLonSpin->value();
        const double subsolarLongitude = subsolarLonSpin->value();

        QVector<GridPoint> convertedPoints;
        convertedPoints.reserve(dataPoints.size());
        int skippedPoints = 0;

        if (geoToMag) {
            CoordTransformer transformer;
            CoordTransformer::Options options;
            options.useFortranBackend = false;
            options.outputMlt = false;
            options.centerOnNoonMeridian = false;
            options.southMirror = false;
            options.transformVectors = true;
            options.poleLatitude = poleLatitude;
            options.poleLongitude = poleLongitude;
            options.subsolarLongitude = subsolarLongitude;
            options.longitudeOffset = 0.0;

            for (const GridPoint& point : dataPoints) {
                if (!std::isfinite(point.latitude) || !std::isfinite(point.longitude) || !std::isfinite(point.height)) {
                    ++skippedPoints;
                    continue;
                }

                GridPoint converted = transformer.transformPoint(point, options);
                if (!std::isfinite(converted.latitude) || !std::isfinite(converted.longitude) || !std::isfinite(converted.height)) {
                    ++skippedPoints;
                    continue;
                }

                converted.longitude = MagneticCoords::wrapTo180(converted.longitude);
                convertedPoints.append(converted);
            }
        } else {
            for (const GridPoint& point : dataPoints) {
                if (!std::isfinite(point.latitude) || !std::isfinite(point.longitude) || !std::isfinite(point.height)) {
                    ++skippedPoints;
                    continue;
                }

                MagCoord mag;
                mag.latitude = point.latitude;
                mag.height = point.height;
                if (inputIsMlt) {
                    mag.mlt = MagneticCoords::wrapTo24(point.longitude);
                } else {
                    mag.longitude = point.longitude;
                    mag.mlt = MagneticCoords::longitudeToMlt(point.longitude, subsolarLongitude);
                }

                const GeoCoord geo = MagneticCoords::toGeographic(
                    mag,
                    poleLatitude,
                    poleLongitude,
                    inputIsMlt,
                    subsolarLongitude);

                if (!std::isfinite(geo.latitude) || !std::isfinite(geo.longitude) || !std::isfinite(geo.height)) {
                    ++skippedPoints;
                    continue;
                }

                GridPoint converted = point;
                converted.latitude = geo.latitude;
                converted.longitude = MagneticCoords::wrapTo180(geo.longitude);
                converted.height = geo.height;

                if (std::isfinite(point.vx) && std::isfinite(point.vy)) {
                    const double deltaDegrees = MagneticCoords::wrapTo180(converted.longitude - point.longitude);
                    const double deltaRadians = deltaDegrees * (kPi / 180.0);
                    const double cosD = std::cos(deltaRadians);
                    const double sinD = std::sin(deltaRadians);
                    const double vx = point.vx * cosD - point.vy * sinD;
                    const double vy = point.vx * sinD + point.vy * cosD;
                    if (std::isfinite(vx) && std::isfinite(vy)) {
                        converted.vx = vx;
                        converted.vy = vy;
                    }
                }

                convertedPoints.append(converted);
            }
        }

        if (convertedPoints.isEmpty()) {
            QMessageBox::warning(
                &dialog,
                tr("Conversion Failed"),
                tr("Could not convert any points. Check coordinate format and parameters."));
            return;
        }

        GridMetadata convertedMetadata = dataMetadata;
        rebuildMetadata(convertedPoints, convertedMetadata);

        dataPoints = convertedPoints;
        dataMetadata = convertedMetadata;
        plotWidget->setData(dataPoints, dataMetadata);
        plotWidget->setMagneticMode(geoToMag);
        if (geoToMag) {
            centerOnNoonAct->setChecked(true);
            showMltLabelsAct->setChecked(false);
            schemeBlueRedAct->setChecked(true);
        }
        updateHeightModeAvailability();

        const QString sourceName = curFile.isEmpty() ? tr("data") : strippedName(curFile);
        const QString conversionName = geoToMag ? tr("magnetic") : tr("geographic");
        setWindowTitle(tr("%1 [%2] - Data Visualizer").arg(sourceName, conversionName));

        resultLabel->setText(
            tr("Dataset converted: %1 points\nSkipped: %2 points\nNow plotting converted coordinates.")
                .arg(convertedPoints.size())
                .arg(skippedPoints));

        statusBar()->showMessage(
            tr("Converted and plotted %1 points (%2)")
                .arg(convertedPoints.size())
                .arg(geoToMag ? tr("geo -> magnetic") : tr("magnetic -> geo")),
            5000);
    };

    connect(directionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [=](int) {
        updateInputUi();
    });
    connect(inputMltCheck, &QCheckBox::toggled, &dialog, [=](bool) {
        updateInputUi();
    });
    connect(convertButton, &QPushButton::clicked, &dialog, [=]() { convertNow(); });
    connect(applyDataButton, &QPushButton::clicked, &dialog, [=]() { applyDatasetConversion(); });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    updateInputUi();
    convertNow();
    dialog.exec();
}

QString MainWindow::strippedName(const QString& fullFileName) {
    return QFileInfo(fullFileName).fileName();
}

void MainWindow::about() {
    QMessageBox::about(
        this,
        tr("About Data Visualizer"),
        tr("<h2>Data Visualizer</h2>"
           "<p>Desktop tool for scalar/vector geophysical maps.</p>"
           "<p>Implemented features:</p>"
           "<ul>"
           "<li>Lat/Lon, North/South polar and height-slice projections</li>"
           "<li>Terminator, subsolar point/track, equator overlays</li>"
           "<li>Contours with labels, crop region, manual value limits</li>"
           "<li>Meridional and lat-height contour slices for 3D grids</li>"
           "<li>MLT labels, noon-meridian centering, south-polar mirror</li>"
           "<li>Linear/log scale and multiple color schemes</li>"
           "<li>Image export with custom resolution</li>"
           "</ul>"));
}

void MainWindow::aboutQt() {
    QApplication::aboutQt();
}
