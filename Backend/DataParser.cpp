#include "DataParser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <cmath>
#include <limits>

// Статическая переменная для хранения последней ошибки
QString DataParser::s_lastError;

bool DataParser::isValidValue(double value) {
    return std::isfinite(value) && 
           value != std::numeric_limits<double>::infinity() &&
           value != -std::numeric_limits<double>::infinity() &&
           value != std::numeric_limits<double>::quiet_NaN() &&
           value != std::numeric_limits<double>::signaling_NaN();
}

bool DataParser::parseFile(const QString& filePath, 
                          GridMetadata& metadata, 
                          QVector<GridPoint>& points) {
    s_lastError.clear();
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        s_lastError = "Не удалось открыть файл: " + filePath;
        qWarning() << s_lastError;
        return false;
    }
    
    QTextStream stream(&file);
    
    // Определение типа данных
    DataType dataType = detectDataType(filePath);
    
    // Парсинг заголовка
    if (!parseHeader(stream, metadata)) {
        return false;
    }
    
    // Парсинг данных
    if (!parseData(stream, points, dataType)) {
        return false;
    }
    
    // Валидация данных
    if (!validateData(points, metadata)) {
        return false;
    }
    
    qDebug() << "Файл успешно загружен:" << filePath;
    qDebug() << "Точек данных:" << points.size();
    qDebug() << "Тип данных:" << (dataType == SCALAR_DATA ? "SCALAR" : "VECTOR");
    
    return true;
}

DataParser::DataType DataParser::detectDataType(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return SCALAR_DATA; // по умолчанию
    }
    
    QTextStream stream(&file);
    QString line;
    
    // Пропускаем заголовки
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#") || line.contains("DATA_START")) {
            break;
        }
    }
    
    // Читаем несколько строк данных
    int scalarCount = 0, vectorCount = 0;
    int checkLines = 10; // проверяем первые 10 строк данных
    
    while (!stream.atEnd() && checkLines-- > 0) {
        line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#")) continue;
        
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        
        if (parts.size() >= 6) { // широта, долгота, значение, vx, vy, vz
            vectorCount++;
        } else if (parts.size() >= 3) { // широта, долгота, значение
            scalarCount++;
        }
    }
    
    // Если больше векторных данных - считаем файл векторным
    return (vectorCount > scalarCount) ? VECTOR_DATA : SCALAR_DATA;
}

bool DataParser::parseHeader(QTextStream& stream, GridMetadata& metadata) {
    QString line;
    bool headerFound = false;
    
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        
        if (line.isEmpty()) continue;
        if (line.startsWith("#")) continue; // комментарии
        
        // Ищем дату и время
        if (line.contains("DATE=") || line.contains("date=")) {
            QRegularExpression dateRe(R"(DATE[=\s]+(\d{4}-\d{2}-\d{2})\s+TIME[=\s]+(\d{2}:\d{2}:\d{2}))");
            QRegularExpressionMatch match = dateRe.match(line);
            if (match.hasMatch()) {
                QString dateStr = match.captured(1);
                QString timeStr = match.captured(2);
                
                metadata.dateTime = QDateTime::fromString(
                    dateStr + " " + timeStr, 
                    "yyyy-MM-dd HH:mm:ss"
                );
            } else {
                // Альтернативный формат
                QRegularExpression altDateRe(R"(DATE[=\s]+(\d{4}-\d{2}-\d{2}))");
                QRegularExpressionMatch altMatch = altDateRe.match(line);
                if (altMatch.hasMatch()) {
                    metadata.dateTime = QDateTime::fromString(
                        altMatch.captured(1), 
                        "yyyy-MM-dd"
                    );
                }
            }
        }
        
        // Ищем границы широты
        if (line.contains("LAT_MIN=") || line.contains("lat_min=")) {
            QRegularExpression coordRe(R"(LAT[_]?MIN[=\s]+([+-]?\d+\.?\d*)[^0-9]*LAT[_]?MAX[=\s]+([+-]?\d+\.?\d*))");
            QRegularExpressionMatch match = coordRe.match(line);
            if (match.hasMatch()) {
                metadata.minLat = match.captured(1).toDouble();
                metadata.maxLat = match.captured(2).toDouble();
            }
        }
        
        // Ищем границы долготы
        if (line.contains("LON_MIN=") || line.contains("lon_min=")) {
            QRegularExpression coordRe(R"(LON[_]?MIN[=\s]+([+-]?\d+\.?\d*)[^0-9]*LON[_]?MAX[=\s]+([+-]?\d+\.?\d*))");
            QRegularExpressionMatch match = coordRe.match(line);
            if (match.hasMatch()) {
                metadata.minLon = match.captured(1).toDouble();
                metadata.maxLon = match.captured(2).toDouble();
            }
        }
        
        // Ищем шаги
        if (line.contains("STEP=") || line.contains("step=")) {
            QRegularExpression stepRe(R"(STEP[_]?LAT[=\s]+([+-]?\d+\.?\d*)[^0-9]*STEP[_]?LON[=\s]+([+-]?\d+\.?\d*))");
            QRegularExpressionMatch match = stepRe.match(line);
            if (match.hasMatch()) {
                metadata.latStep = match.captured(1).toDouble();
                metadata.lonStep = match.captured(2).toDouble();
            } else {
                // Альтернативный формат
                QRegularExpression simpleStepRe(R"(STEP[=\s]+([+-]?\d+\.?\d*))");
                QRegularExpressionMatch simpleMatch = simpleStepRe.match(line);
                if (simpleMatch.hasMatch()) {
                    double step = simpleMatch.captured(1).toDouble();
                    metadata.latStep = step;
                    metadata.lonStep = step;
                }
            }
        }
        
        // Ищем название параметра
        if (line.contains("PARAMETER=") || line.contains("parameter=")) {
            QRegularExpression paramRe(R"(PARAMETER[=\s]+(\w+))");
            QRegularExpressionMatch match = paramRe.match(line);
            if (match.hasMatch()) {
                metadata.parameterName = match.captured(1);
            }
        }
        
        // Метка начала данных
        if (line.startsWith("DATA_START") || line.startsWith("BEGIN_DATA") || 
            line.startsWith("data_start") || line.startsWith("begin_data")) {
            headerFound = true;
            break;
        }
    }
    
    if (!headerFound) {
        s_lastError = "Не найдена метка начала данных";
        return false;
    }
    
    // Рассчитываем количество точек
    if (metadata.latStep != 0.0 && metadata.lonStep != 0.0) {
        metadata.latCount = static_cast<int>(std::abs((metadata.maxLat - metadata.minLat) / metadata.latStep)) + 1;
        metadata.lonCount = static_cast<int>(std::abs((metadata.maxLon - metadata.minLon) / metadata.lonStep)) + 1;
    }
    
    return true;
}

bool DataParser::parseData(QTextStream& stream, QVector<GridPoint>& points, 
                          DataType type) {
    points.clear();
    QString line;
    int lineNumber = 0;
    int validPoints = 0;
    
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        lineNumber++;
        
        if (line.isEmpty() || line.startsWith("#")) continue;
        
        // Если встречаем конец данных
        if (line.startsWith("END_DATA") || line.startsWith("DATA_END") ||
            line.startsWith("end_data") || line.startsWith("data_end")) {
            break;
        }
        
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        
        if (type == SCALAR_DATA && parts.size() >= 3) {
            // Формат: широта долгота значение
            bool ok1, ok2, ok3;
            double lat = parts[0].toDouble(&ok1);
            double lon = parts[1].toDouble(&ok2);
            double value = parts[2].toDouble(&ok3);
            
            if (ok1 && ok2 && ok3 && isValidValue(lat) && isValidValue(lon) && isValidValue(value)) {
                points.append(GridPoint(lat, lon, value));
                validPoints++;
            } else {
                qWarning() << QString("Предупреждение в строке %1: некорректные числовые данные").arg(lineNumber);
            }
        }
        else if (type == VECTOR_DATA && parts.size() >= 6) {
            // Формат: широта долгота значение vx vy vz
            bool ok1, ok2, ok3, ok4, ok5, ok6;
            double lat = parts[0].toDouble(&ok1);
            double lon = parts[1].toDouble(&ok2);
            double value = parts[2].toDouble(&ok3);
            double vx = parts[3].toDouble(&ok4);
            double vy = parts[4].toDouble(&ok5);
            double vz = parts[5].toDouble(&ok6);
            
            if (ok1 && ok2 && ok3 && ok4 && ok5 && ok6 && 
                isValidValue(lat) && isValidValue(lon) && isValidValue(value) &&
                isValidValue(vx) && isValidValue(vy) && isValidValue(vz)) {
                GridPoint point(lat, lon, value, vx, vy, vz);
                points.append(point);
                validPoints++;
            } else {
                qWarning() << QString("Предупреждение в строке %1: некорректные векторные данные").arg(lineNumber);
            }
        }
        else if (parts.size() < 3) {
            qWarning() << QString("Предупреждение в строке %1: недостаточно данных").arg(lineNumber);
        }
    }
    
    if (validPoints == 0) {
        s_lastError = "Файл не содержит корректных данных";
        return false;
    }
    
    qDebug() << "Загружено корректных точек:" << validPoints;
    return true;
}

bool DataParser::validateData(const QVector<GridPoint>& points, 
                             const GridMetadata& metadata) {
    if (points.isEmpty()) {
        s_lastError = " валидации";
        return false;
    }
    
    // Проверка