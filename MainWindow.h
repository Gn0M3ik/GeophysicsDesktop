#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QVector>

#include "DataParser.h"
#include "PlotWidget.h"

class QAction;
class QActionGroup;
class QMenu;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void loadFile(const QString& fileName, DataParser::DataType dataType = DataParser::SCALAR_DATA);

private slots:
    void openScalarFile();
    void openVectorFile();
    void exportImage();
    void setManualValueRange();
    void clearManualValueRange();
    void setCropRegion();
    void clearCropRegion();
    void setContourStep();
    void setLongitudeSlice();
    void setLatitudeSlice();
    void setSliceWindow();
    void convertCoordinates();
    void about();
    void aboutQt();

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void readSettings();
    void writeSettings();
    void applyProjectionSelection(PlotWidget::ProjectionMode mode);
    void updateHeightModeAvailability();
    QString strippedName(const QString& fullFileName);

    PlotWidget* plotWidget = nullptr;

    QMenu* fileMenu = nullptr;
    QMenu* viewMenu = nullptr;
    QMenu* projectionMenu = nullptr;
    QMenu* colorSchemeMenu = nullptr;
    QMenu* dataRangeMenu = nullptr;
    QMenu* cropMenu = nullptr;
    QMenu* helpMenu = nullptr;

    QAction* openScalarAct = nullptr;
    QAction* openVectorAct = nullptr;
    QAction* exportImageAct = nullptr;
    QAction* exitAct = nullptr;
    QAction* aboutAct = nullptr;
    QAction* aboutQtAct = nullptr;

    QAction* showGridAct = nullptr;
    QAction* showVectorsAct = nullptr;
    QAction* showTerminatorAct = nullptr;
    QAction* showSubsolarPointAct = nullptr;
    QAction* showSubsolarTrackAct = nullptr;
    QAction* showEquatorAct = nullptr;
    QAction* showContoursAct = nullptr;
    QAction* showMltLabelsAct = nullptr;
    QAction* centerOnNoonAct = nullptr;
    QAction* southMirrorAct = nullptr;
    QAction* logScaleAct = nullptr;
    QAction* resetViewAct = nullptr;

    QAction* projectionLatLonAct = nullptr;
    QAction* projectionNorthPolarAct = nullptr;
    QAction* projectionSouthPolarAct = nullptr;
    QAction* projectionLatHeightAct = nullptr;
    QAction* projectionLonHeightAct = nullptr;
    QActionGroup* projectionGroup = nullptr;

    QAction* schemeGrayscaleAct = nullptr;
    QAction* schemeRainbowAct = nullptr;
    QAction* schemeBlueGreenRedAct = nullptr;
    QAction* schemeBlueRedAct = nullptr;
    QActionGroup* colorSchemeGroup = nullptr;

    QAction* setManualRangeAct = nullptr;
    QAction* clearManualRangeAct = nullptr;
    QAction* setCropAct = nullptr;
    QAction* clearCropAct = nullptr;
    QAction* contourStepAct = nullptr;
    QAction* setLongitudeSliceAct = nullptr;
    QAction* setLatitudeSliceAct = nullptr;
    QAction* setSliceWindowAct = nullptr;
    QAction* convertCoordsAct = nullptr;

    QString curFile;
    QString lastOpenDirectory;
    QVector<GridPoint> dataPoints;
    GridMetadata dataMetadata;
};

#endif // MAINWINDOW_H
