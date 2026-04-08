#include "core/Core.h"

namespace dataviz::core {

bool Core::loadScalarFile(const QString& filePath) {
    return loadFileInternal(filePath, DataParser::SCALAR_DATA);
}

bool Core::loadVectorFile(const QString& filePath) {
    return loadFileInternal(filePath, DataParser::VECTOR_DATA);
}

bool Core::loadVectorComponents(const QString& xFilePath, const QString& yFilePath) {
    QVector<GridPoint> points;
    GridMetadata metadata;
    if (!DataParser::parseVectorComponents(xFilePath, yFilePath, metadata, points)) {
        m_lastError = DataParser::getLastError();
        return false;
    }

    m_model.setData(points, metadata);
    m_lastError.clear();
    return true;
}

bool Core::transformToMagnetic(const dataviz::coords::CoordTransformer::Options& options) {
    if (m_model.isEmpty()) {
        m_lastError = "No data loaded";
        return false;
    }

    GridMetadata transformedMetadata;
    const QVector<GridPoint> transformed = m_transformer.transform(
        m_model.points(),
        m_model.metadata(),
        &transformedMetadata,
        options);
    const QString transformMessage = m_transformer.lastError();

    if (transformed.isEmpty()) {
        m_lastError = transformMessage.isEmpty() ? QString("Coordinate transform failed") : transformMessage;
        return false;
    }

    m_model.setData(transformed, transformedMetadata);
    m_lastError = transformMessage;
    return true;
}

void Core::clear() {
    m_model.clear();
    m_lastError.clear();
}

bool Core::loadFileInternal(const QString& filePath, DataParser::DataType dataType) {
    QVector<GridPoint> points;
    GridMetadata metadata;
    if (!DataParser::parseFile(filePath, metadata, points, dataType)) {
        m_lastError = DataParser::getLastError();
        return false;
    }

    m_model.setData(points, metadata);
    m_lastError.clear();
    return true;
}

} // namespace dataviz::core
