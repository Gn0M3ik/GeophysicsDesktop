#ifndef DATAPARSER_H
#define DATAPARSER_H

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

struct GridPoint {
    double latitude;
    double longitude;
    double height;
    double value;
    double vx;
    double vy;
    double vz;

    GridPoint()
        : latitude(0.0)
        , longitude(0.0)
        , height(0.0)
        , value(0.0)
        , vx(0.0)
        , vy(0.0)
        , vz(0.0) {
    }

    GridPoint(double lat, double lon, double val)
        : latitude(lat)
        , longitude(lon)
        , height(0.0)
        , value(val)
        , vx(0.0)
        , vy(0.0)
        , vz(0.0) {
    }

    GridPoint(double lat, double lon, double h, double val)
        : latitude(lat)
        , longitude(lon)
        , height(h)
        , value(val)
        , vx(0.0)
        , vy(0.0)
        , vz(0.0) {
    }

    GridPoint(double lat, double lon, double val, double vectorX, double vectorY, double vectorZ, double h = 0.0)
        : latitude(lat)
        , longitude(lon)
        , height(h)
        , value(val)
        , vx(vectorX)
        , vy(vectorY)
        , vz(vectorZ) {
    }
};

struct GridMetadata {
    QDateTime dateTime;
    double minLat;
    double maxLat;
    double minLon;
    double maxLon;
    double latStep;
    double lonStep;
    int latCount;
    int lonCount;

    double minHeight;
    double maxHeight;
    double heightStep;
    int heightCount;
    double height;
    bool hasHeightDimension;

    QString parameterName;

    GridMetadata()
        : minLat(-90.0)
        , maxLat(90.0)
        , minLon(0.0)
        , maxLon(360.0)
        , latStep(1.0)
        , lonStep(1.0)
        , latCount(0)
        , lonCount(0)
        , minHeight(0.0)
        , maxHeight(0.0)
        , heightStep(0.0)
        , heightCount(1)
        , height(0.0)
        , hasHeightDimension(false)
        , parameterName("unknown") {
    }
};

class DataParser {
public:
    enum DataType {
        SCALAR_DATA,
        VECTOR_DATA
    };

    static bool parseFile(const QString& filePath,
                          GridMetadata& metadata,
                          QVector<GridPoint>& points,
                          DataType dataTypeHint = SCALAR_DATA);
    static bool parseVectorComponents(const QString& xFilePath,
                                      const QString& yFilePath,
                                      GridMetadata& metadata,
                                      QVector<GridPoint>& points);
    static DataType detectDataType(const QString& filePath);
    static QString getLastError();

private:
    enum class FileFormat {
        Unknown,
        Matrix,
        PointList
    };

    static QString s_lastError;
    static bool parseHeader(const QStringList& lines,
                            GridMetadata& metadata,
                            int& dataStartIndex,
                            QVector<double>& longitudeHeader);
    static bool parseData(const QStringList& lines,
                          int dataStartIndex,
                          QVector<GridPoint>& points,
                          GridMetadata& metadata,
                          DataType dataType,
                          const QVector<double>& longitudeHeader);
    static bool validateData(const QVector<GridPoint>& points,
                             const GridMetadata& metadata);
    static FileFormat detectFileFormat(const QStringList& lines, int dataStartIndex);
    static bool isValidValue(double value);
};

Q_DECLARE_METATYPE(GridPoint)
Q_DECLARE_METATYPE(GridMetadata)

#endif // DATAPARSER_H
