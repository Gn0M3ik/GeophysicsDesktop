#include "DataParser.h"

#include <QDate>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QTime>
#include <QTimeZone>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

namespace {
constexpr int kHeaderScanLines = 12;
constexpr int kMinMatrixColumns = 8;
constexpr double kQuantizeScale = 1e6;
const QRegularExpression kWhitespaceRx("\\s+");

struct LocationKey {
    qint64 latitude = 0;
    qint64 longitude = 0;
    qint64 height = 0;

    bool operator==(const LocationKey& other) const {
        return latitude == other.latitude &&
               longitude == other.longitude &&
               height == other.height;
    }
};

size_t qHash(const LocationKey& key, size_t seed = 0) noexcept {
    seed ^= ::qHash(key.latitude, seed + 0x9e3779b9);
    seed ^= ::qHash(key.longitude, seed + 0x9e3779b9);
    seed ^= ::qHash(key.height, seed + 0x9e3779b9);
    return seed;
}

qint64 quantizeToMicro(double value) {
    return static_cast<qint64>(std::llround(value * kQuantizeScale));
}

LocationKey makeLocationKey(double lat, double lon, double height) {
    return {quantizeToMicro(lat), quantizeToMicro(lon), quantizeToMicro(height)};
}

bool isCommentOrEmpty(const QString& line) {
    const QString trimmed = line.trimmed();
    return trimmed.isEmpty() || trimmed.startsWith('#') || trimmed.startsWith("//");
}

bool parseNumericRow(const QString& line, QVector<double>& numbers) {
    numbers.clear();
    QString normalized = line;
    normalized.replace(',', ' ');
    normalized.replace('D', 'E');
    normalized.replace('d', 'E');
    const QStringList tokens = normalized.split(kWhitespaceRx, Qt::SkipEmptyParts);

    if (tokens.isEmpty()) {
        return false;
    }

    numbers.reserve(tokens.size());
    for (const QString& token : tokens) {
        bool ok = false;
        const double value = token.toDouble(&ok);
        if (!ok) {
            numbers.clear();
            return false;
        }
        numbers.append(value);
    }
    return true;
}

bool isMonotonic(const QVector<double>& values) {
    if (values.size() < 2) {
        return false;
    }

    int direction = 0;
    for (int i = 1; i < values.size(); ++i) {
        const double delta = values[i] - values[i - 1];
        if (qFuzzyIsNull(delta)) {
            continue;
        }

        const int sign = (delta > 0.0) ? 1 : -1;
        if (direction == 0) {
            direction = sign;
            continue;
        }

        if (sign != direction) {
            return false;
        }
    }

    return direction != 0;
}

bool isInLatitudeRange(double latitude) {
    return latitude >= -90.0 && latitude <= 90.0;
}

bool allInRange(const QVector<double>& values, double minValue, double maxValue) {
    for (const double value : values) {
        if (value < minValue || value > maxValue) {
            return false;
        }
    }
    return true;
}

bool looksLikeLongitudeHeaderRow(const QVector<double>& values) {
    if (values.size() <= kMinMatrixColumns) {
        return false;
    }
    if (!isMonotonic(values) || !allInRange(values, -360.0, 720.0)) {
        return false;
    }

    const double first = values.first();
    const double last = values.last();
    const double span = last - first;
    if (span < 180.0) {
        return false;
    }

    const double firstStep = values[1] - values[0];
    if (firstStep <= 0.0) {
        return false;
    }

    const double tolerance = std::max(0.25, std::abs(firstStep) * 0.25);
    for (int i = 2; i < values.size(); ++i) {
        const double step = values[i] - values[i - 1];
        if (step <= 0.0 || std::abs(step - firstStep) > tolerance) {
            return false;
        }
    }

    return true;
}

DataParser::DataType detectDataTypeFromLines(const QStringList& lines, int dataStartIndex) {
    int scalarRows = 0;
    int vectorRows = 0;
    QVector<double> numbers;

    for (int i = dataStartIndex; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (isCommentOrEmpty(line) || !parseNumericRow(line, numbers) || numbers.size() < 3) {
            continue;
        }

        if (numbers.size() > 6) {
            scalarRows++;
            continue;
        }

        if (numbers.size() >= 5) {
            vectorRows++;
        } else {
            scalarRows++;
        }
    }

    return (vectorRows > 0 && vectorRows >= scalarRows) ? DataParser::VECTOR_DATA : DataParser::SCALAR_DATA;
}

double inferStep(QVector<double> values) {
    if (values.size() < 2) {
        return 1.0;
    }

    std::sort(values.begin(), values.end());
    double bestStep = 0.0;

    for (int i = 1; i < values.size(); ++i) {
        const double step = values[i] - values[i - 1];
        if (qFuzzyIsNull(step)) {
            continue;
        }

        if (qFuzzyIsNull(bestStep) || std::abs(step) < std::abs(bestStep)) {
            bestStep = step;
        }
    }

    return qFuzzyIsNull(bestStep) ? 1.0 : bestStep;
}

void updateBoundsFromPoints(const QVector<GridPoint>& points, GridMetadata& metadata) {
    if (points.isEmpty()) {
        return;
    }

    metadata.minLat = points.first().latitude;
    metadata.maxLat = points.first().latitude;
    metadata.minLon = points.first().longitude;
    metadata.maxLon = points.first().longitude;
    metadata.minHeight = points.first().height;
    metadata.maxHeight = points.first().height;

    for (const GridPoint& point : points) {
        metadata.minLat = std::min(metadata.minLat, point.latitude);
        metadata.maxLat = std::max(metadata.maxLat, point.latitude);
        metadata.minLon = std::min(metadata.minLon, point.longitude);
        metadata.maxLon = std::max(metadata.maxLon, point.longitude);
        metadata.minHeight = std::min(metadata.minHeight, point.height);
        metadata.maxHeight = std::max(metadata.maxHeight, point.height);
    }
}

bool inferVectorHeightFromSixColumnRows(const QVector<QVector<double>>& rows) {
    QVector<double> thirdColumnValues;
    thirdColumnValues.reserve(rows.size());

    for (const QVector<double>& row : rows) {
        if (row.size() == 6) {
            thirdColumnValues.append(row[2]);
        }
    }

    if (thirdColumnValues.size() < 8) {
        return false;
    }

    QSet<QString> uniqueRounded;
    uniqueRounded.reserve(thirdColumnValues.size());
    for (const double value : thirdColumnValues) {
        uniqueRounded.insert(QString::number(value, 'f', 3));
    }

    const int uniqueCount = uniqueRounded.size();
    const int threshold = std::max(6, static_cast<int>(thirdColumnValues.size() / 6));
    return uniqueCount <= threshold;
}
} // namespace

QString DataParser::s_lastError;

bool DataParser::isValidValue(double value) {
    return std::isfinite(value);
}

QString DataParser::getLastError() {
    return s_lastError;
}

bool DataParser::parseFile(const QString& filePath,
                           GridMetadata& metadata,
                           QVector<GridPoint>& points,
                           DataType dataTypeHint) {
    s_lastError.clear();
    points.clear();
    metadata = GridMetadata();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        s_lastError = QString("Could not open file: %1").arg(filePath);
        qWarning() << s_lastError;
        return false;
    }

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif

    QStringList lines;
    while (!stream.atEnd()) {
        lines.append(stream.readLine());
    }

    if (lines.isEmpty()) {
        s_lastError = "File is empty";
        return false;
    }

    int dataStartIndex = -1;
    QVector<double> longitudeHeader;

    if (!parseHeader(lines, metadata, dataStartIndex, longitudeHeader)) {
        return false;
    }

    DataType detectedType = detectDataTypeFromLines(lines, dataStartIndex);
    if (dataTypeHint == VECTOR_DATA && detectedType == SCALAR_DATA) {
        detectedType = VECTOR_DATA;
    }

    if (!parseData(lines, dataStartIndex, points, metadata, detectedType, longitudeHeader)) {
        return false;
    }

    if (!validateData(points, metadata)) {
        return false;
    }

    qDebug() << "Loaded file:" << filePath;
    qDebug() << "Point count:" << points.size();
    qDebug() << "Parameter:" << metadata.parameterName;
    qDebug() << "Latitude range:" << metadata.minLat << "-" << metadata.maxLat;
    qDebug() << "Longitude range:" << metadata.minLon << "-" << metadata.maxLon;
    qDebug() << "Height range:" << metadata.minHeight << "-" << metadata.maxHeight;

    return true;
}

bool DataParser::parseVectorComponents(const QString& xFilePath,
                                       const QString& yFilePath,
                                       GridMetadata& metadata,
                                       QVector<GridPoint>& points) {
    GridMetadata xMetadata;
    GridMetadata yMetadata;
    QVector<GridPoint> xPoints;
    QVector<GridPoint> yPoints;

    if (!parseFile(xFilePath, xMetadata, xPoints, SCALAR_DATA)) {
        s_lastError = QString("Could not parse X component file: %1").arg(getLastError());
        return false;
    }

    if (!parseFile(yFilePath, yMetadata, yPoints, SCALAR_DATA)) {
        s_lastError = QString("Could not parse Y component file: %1").arg(getLastError());
        return false;
    }

    QHash<LocationKey, GridPoint> yByLocation;
    yByLocation.reserve(yPoints.size());
    for (const GridPoint& point : yPoints) {
        if (!std::isfinite(point.latitude) || !std::isfinite(point.longitude) || !std::isfinite(point.height)) {
            continue;
        }
        yByLocation.insert(makeLocationKey(point.latitude, point.longitude, point.height), point);
    }

    points.clear();
    points.reserve(xPoints.size());

    QVector<double> latitudes;
    QVector<double> longitudes;
    QVector<double> heights;
    QSet<qint64> uniqueLat;
    QSet<qint64> uniqueLon;
    QSet<qint64> uniqueHeight;

    for (const GridPoint& xPoint : xPoints) {
        if (!std::isfinite(xPoint.latitude) || !std::isfinite(xPoint.longitude) || !std::isfinite(xPoint.height)) {
            continue;
        }

        const LocationKey key = makeLocationKey(xPoint.latitude, xPoint.longitude, xPoint.height);
        const auto it = yByLocation.constFind(key);
        if (it == yByLocation.constEnd()) {
            continue;
        }

        const GridPoint& yPoint = it.value();
        GridPoint vectorPoint(
            xPoint.latitude,
            xPoint.longitude,
            std::hypot(xPoint.value, yPoint.value),
            xPoint.value,
            yPoint.value,
            0.0,
            xPoint.height);
        points.append(vectorPoint);

        latitudes.append(vectorPoint.latitude);
        longitudes.append(vectorPoint.longitude);
        heights.append(vectorPoint.height);
        uniqueLat.insert(quantizeToMicro(vectorPoint.latitude));
        uniqueLon.insert(quantizeToMicro(vectorPoint.longitude));
        uniqueHeight.insert(quantizeToMicro(vectorPoint.height));
    }

    if (points.isEmpty()) {
        s_lastError = "Could not align points from component files";
        return false;
    }

    metadata = xMetadata;
    updateBoundsFromPoints(points, metadata);
    metadata.latCount = uniqueLat.size();
    metadata.lonCount = uniqueLon.size();
    metadata.latStep = inferStep(latitudes);
    metadata.lonStep = inferStep(longitudes);

    metadata.heightCount = std::max(1, static_cast<int>(uniqueHeight.size()));
    metadata.heightStep = inferStep(heights);
    metadata.hasHeightDimension = metadata.heightCount > 1;
    metadata.height = metadata.minHeight;

    if (!validateData(points, metadata)) {
        return false;
    }

    return true;
}

DataParser::DataType DataParser::detectDataType(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return SCALAR_DATA;
    }

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif

    QStringList lines;
    while (!stream.atEnd()) {
        lines.append(stream.readLine());
    }

    GridMetadata metadata;
    int dataStartIndex = -1;
    QVector<double> longitudeHeader;
    if (!parseHeader(lines, metadata, dataStartIndex, longitudeHeader)) {
        return SCALAR_DATA;
    }

    return detectDataTypeFromLines(lines, dataStartIndex);
}

bool DataParser::parseHeader(const QStringList& lines,
                             GridMetadata& metadata,
                             int& dataStartIndex,
                             QVector<double>& longitudeHeader) {
    dataStartIndex = -1;
    longitudeHeader.clear();
    int longitudeHeaderIndex = -1;

    int firstContentLine = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (!isCommentOrEmpty(lines[i])) {
            firstContentLine = i;
            break;
        }
    }

    if (firstContentLine < 0) {
        s_lastError = "File does not contain data";
        return false;
    }

    QVector<double> numbers;
    bool dateParsed = false;
    const int maxHeaderLine =
        std::min<int>(static_cast<int>(lines.size()), firstContentLine + kHeaderScanLines);

    for (int i = firstContentLine; i < maxHeaderLine; ++i) {
        const QString line = lines[i].trimmed();
        if (isCommentOrEmpty(line)) {
            continue;
        }

        if (parseNumericRow(line, numbers) && !dateParsed && numbers.size() >= 2) {
            const int year = static_cast<int>(std::round(numbers[0]));
            const int dayOfYear = static_cast<int>(std::round(numbers[1]));
            const bool looksLikeYearDay = (std::abs(numbers[0] - year) < 1e-3) &&
                                          (std::abs(numbers[1] - dayOfYear) < 1e-3);

            if (looksLikeYearDay && year >= 1900 && year <= 3000 && dayOfYear >= 1 && dayOfYear <= 366) {
                const QDate date(year, 1, 1);
                QTime time(0, 0, 0);
                if (numbers.size() >= 3 && numbers[2] >= 0.0 && numbers[2] < 24.0) {
                    const int hour = static_cast<int>(numbers[2]);
                    const int minute = static_cast<int>((numbers[2] - hour) * 60.0);
                    const int second = static_cast<int>((numbers[2] * 3600.0) - hour * 3600 - minute * 60);
                    time = QTime(qBound(0, hour, 23), qBound(0, minute, 59), qBound(0, second, 59));
                } else if (numbers.size() >= 5 && numbers[4] >= 0.0 && numbers[4] < 86400.0) {
                    const int secondOfDay = static_cast<int>(std::round(numbers[4]));
                    time = QTime::fromMSecsSinceStartOfDay(qBound(0, secondOfDay, 86399) * 1000);
                }

                metadata.dateTime = QDateTime(date.addDays(dayOfYear - 1), time, QTimeZone::UTC);
                dateParsed = true;
            }
        }
    }

    for (int i = firstContentLine; i < maxHeaderLine; ++i) {
        const QString line = lines[i].trimmed();
        if (isCommentOrEmpty(line)) {
            continue;
        }

        if (parseNumericRow(line, numbers)) {
            continue;
        }

        if (!line.startsWith("dMagnetic", Qt::CaseInsensitive) &&
            !line.startsWith("sMagnetic", Qt::CaseInsensitive)) {
            metadata.parameterName = line.section(',', 0, 0).section('(', 0, 0).trimmed();
            break;
        }
    }

    for (int i = firstContentLine; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (isCommentOrEmpty(line) || !parseNumericRow(line, numbers) || numbers.size() < 2) {
            continue;
        }

        if (looksLikeLongitudeHeaderRow(numbers) && longitudeHeader.isEmpty()) {
            longitudeHeader = numbers;
            longitudeHeaderIndex = i;
            continue;
        }
    }

    for (int i = firstContentLine; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (isCommentOrEmpty(line) || !parseNumericRow(line, numbers) || numbers.size() < 2) {
            continue;
        }

        if (i <= longitudeHeaderIndex) {
            continue;
        }

        const bool firstIsLatitude = isInLatitudeRange(numbers[0]);
        if (!firstIsLatitude) {
            continue;
        }

        if (!longitudeHeader.isEmpty()) {
            if (numbers.size() >= longitudeHeader.size()) {
                dataStartIndex = i;
                break;
            }
            continue;
        }

        if (numbers.size() >= 3) {
            dataStartIndex = i;
            break;
        }
    }

    if (dataStartIndex < 0) {
        s_lastError = "Could not find start of data block";
        return false;
    }

    if (longitudeHeader.size() >= 2) {
        metadata.minLon = longitudeHeader.first();
        metadata.maxLon = longitudeHeader.last();
        metadata.lonStep = longitudeHeader[1] - longitudeHeader[0];
        metadata.lonCount = longitudeHeader.size();
    }

    return true;
}

DataParser::FileFormat DataParser::detectFileFormat(const QStringList& lines, int dataStartIndex) {
    int numericRows = 0;
    int matrixScore = 0;
    int pointScore = 0;
    QVector<double> numbers;

    for (int i = dataStartIndex; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (isCommentOrEmpty(line) || !parseNumericRow(line, numbers)) {
            continue;
        }

        numericRows++;

        if (numbers.size() > kMinMatrixColumns) {
            matrixScore++;
        }

        if (numbers.size() >= 3 && numbers.size() <= 10) {
            pointScore++;
        }
    }

    if (numericRows == 0) {
        return FileFormat::Unknown;
    }

    if (matrixScore > pointScore) {
        return FileFormat::Matrix;
    }

    if (pointScore > 0) {
        return FileFormat::PointList;
    }

    return FileFormat::Unknown;
}

bool DataParser::parseData(const QStringList& lines,
                           int dataStartIndex,
                           QVector<GridPoint>& points,
                           GridMetadata& metadata,
                           DataType dataType,
                           const QVector<double>& longitudeHeader) {
    points.clear();

    const FileFormat format = detectFileFormat(lines, dataStartIndex);
    if (format == FileFormat::Unknown) {
        s_lastError = "Could not determine data format";
        return false;
    }

    QVector<double> numbers;

    if (format == FileFormat::PointList) {
        QVector<QVector<double>> rows;
        rows.reserve(lines.size() - dataStartIndex);

        for (int i = dataStartIndex; i < lines.size(); ++i) {
            const QString line = lines[i].trimmed();
            if (isCommentOrEmpty(line) || !parseNumericRow(line, numbers) || numbers.size() < 3) {
                continue;
            }
            rows.append(numbers);
        }

        if (rows.isEmpty()) {
            s_lastError = "No valid numeric rows found";
            return false;
        }

        bool hasHeightColumn = false;
        if (dataType == SCALAR_DATA) {
            for (const QVector<double>& row : rows) {
                if (row.size() >= 4) {
                    hasHeightColumn = true;
                    break;
                }
            }
        } else {
            bool hasSevenColumns = false;
            bool hasSixColumns = false;
            for (const QVector<double>& row : rows) {
                if (row.size() >= 7) {
                    hasSevenColumns = true;
                    break;
                }
                if (row.size() == 6) {
                    hasSixColumns = true;
                }
            }

            if (hasSevenColumns) {
                hasHeightColumn = true;
            } else if (hasSixColumns) {
                hasHeightColumn = inferVectorHeightFromSixColumnRows(rows);
            }
        }

        QVector<double> latitudes;
        QVector<double> longitudes;
        QVector<double> heights;
        QSet<qint64> uniqueLat;
        QSet<qint64> uniqueLon;
        QSet<qint64> uniqueHeight;

        points.reserve(rows.size());

        for (const QVector<double>& row : rows) {
            const double lat = row[0];
            const double lon = row[1];

            if (!isValidValue(lat) || !isValidValue(lon)) {
                continue;
            }

            double height = 0.0;
            double value = 0.0;
            double vx = 0.0;
            double vy = 0.0;
            double vz = 0.0;

            if (dataType == SCALAR_DATA) {
                if (hasHeightColumn) {
                    if (row.size() < 4) {
                        continue;
                    }
                    height = row[2];
                    value = row[3];
                } else {
                    value = row[2];
                }
            } else {
                if (hasHeightColumn) {
                    if (row.size() >= 7) {
                        height = row[2];
                        value = row[3];
                        vx = row[4];
                        vy = row[5];
                        vz = row[6];
                    } else if (row.size() == 6) {
                        height = row[2];
                        value = row[3];
                        vx = row[4];
                        vy = row[5];
                    } else if (row.size() >= 5) {
                        value = row[2];
                        vx = row[3];
                        vy = row[4];
                    } else {
                        continue;
                    }
                } else {
                    if (row.size() >= 6) {
                        value = row[2];
                        vx = row[3];
                        vy = row[4];
                        vz = row[5];
                    } else if (row.size() >= 5) {
                        value = row[2];
                        vx = row[3];
                        vy = row[4];
                    } else {
                        value = row[2];
                    }
                }
            }

            if (!isValidValue(height) || !isValidValue(value) || !isValidValue(vx) || !isValidValue(vy) || !isValidValue(vz)) {
                continue;
            }

            points.append(GridPoint(lat, lon, value, vx, vy, vz, height));
            latitudes.append(lat);
            longitudes.append(lon);
            heights.append(height);
            uniqueLat.insert(quantizeToMicro(lat));
            uniqueLon.insert(quantizeToMicro(lon));
            uniqueHeight.insert(quantizeToMicro(height));
        }

        if (points.isEmpty()) {
            s_lastError = "No valid rows left after parsing";
            return false;
        }

        updateBoundsFromPoints(points, metadata);
        metadata.latCount = uniqueLat.size();
        metadata.lonCount = uniqueLon.size();
        metadata.latStep = inferStep(latitudes);
        metadata.lonStep = inferStep(longitudes);

        metadata.heightCount = std::max(1, static_cast<int>(uniqueHeight.size()));
        metadata.heightStep = inferStep(heights);
        metadata.hasHeightDimension = hasHeightColumn || metadata.heightCount > 1;
        metadata.height = metadata.minHeight;
        return true;
    }

    QVector<double> matrixLatitudes;
    QVector<QVector<double>> matrixValues;
    int lonValueCount = 0;

    for (int i = dataStartIndex; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (isCommentOrEmpty(line) || !parseNumericRow(line, numbers) || numbers.size() < 2) {
            continue;
        }

        const double latitude = numbers[0];
        if (!isInLatitudeRange(latitude)) {
            continue;
        }

        QVector<double> rowValues;
        rowValues.reserve(numbers.size() - 1);
        for (int j = 1; j < numbers.size(); ++j) {
            if (isValidValue(numbers[j])) {
                rowValues.append(numbers[j]);
            }
        }

        if (rowValues.isEmpty()) {
            continue;
        }

        matrixLatitudes.append(latitude);
        matrixValues.append(rowValues);
        lonValueCount = std::max(lonValueCount, static_cast<int>(rowValues.size()));
    }

    if (matrixValues.isEmpty()) {
        s_lastError = "Could not parse matrix data";
        return false;
    }

    QVector<double> resolvedLongitudes = longitudeHeader;
    if (resolvedLongitudes.size() != lonValueCount) {
        resolvedLongitudes.clear();
        resolvedLongitudes.reserve(lonValueCount);
        for (int i = 0; i < lonValueCount; ++i) {
            resolvedLongitudes.append(metadata.minLon + i * metadata.lonStep);
        }
    }

    for (int row = 0; row < matrixValues.size(); ++row) {
        const double lat = matrixLatitudes[row];
        const QVector<double>& values = matrixValues[row];

        for (int col = 0; col < values.size(); ++col) {
            const double lon = resolvedLongitudes.value(col, metadata.minLon + col * metadata.lonStep);
            points.append(GridPoint(lat, lon, values[col]));
        }
    }

    metadata.minLat = *std::min_element(matrixLatitudes.begin(), matrixLatitudes.end());
    metadata.maxLat = *std::max_element(matrixLatitudes.begin(), matrixLatitudes.end());
    metadata.latCount = matrixLatitudes.size();
    metadata.latStep = inferStep(matrixLatitudes);

    if (resolvedLongitudes.size() >= 2) {
        metadata.minLon = resolvedLongitudes.first();
        metadata.maxLon = resolvedLongitudes.last();
        metadata.lonCount = resolvedLongitudes.size();
        metadata.lonStep = resolvedLongitudes[1] - resolvedLongitudes[0];
    } else {
        metadata.lonCount = lonValueCount;
        metadata.maxLon = metadata.minLon + (metadata.lonCount - 1) * metadata.lonStep;
    }

    metadata.minHeight = 0.0;
    metadata.maxHeight = 0.0;
    metadata.heightStep = 0.0;
    metadata.heightCount = 1;
    metadata.height = 0.0;
    metadata.hasHeightDimension = false;

    return !points.isEmpty();
}

bool DataParser::validateData(const QVector<GridPoint>& points,
                              const GridMetadata& metadata) {
    Q_UNUSED(metadata)

    if (points.isEmpty()) {
        s_lastError = "No points to validate";
        return false;
    }

    int invalidLatitudeCount = 0;
    for (const GridPoint& point : points) {
        if (!isValidValue(point.latitude) ||
            !isValidValue(point.longitude) ||
            !isValidValue(point.height) ||
            !isValidValue(point.value) ||
            !isValidValue(point.vx) ||
            !isValidValue(point.vy) ||
            !isValidValue(point.vz)) {
            s_lastError = "Non-numeric values found in parsed data";
            return false;
        }

        if (point.latitude < -90.0 || point.latitude > 90.0) {
            invalidLatitudeCount++;
        }
    }

    if (invalidLatitudeCount > points.size() / 2) {
        s_lastError = "Too many latitude values out of range [-90, 90]";
        return false;
    }

    return true;
}
