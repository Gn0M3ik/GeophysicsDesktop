#ifndef DATAVIZ_CORE_CORE_H
#define DATAVIZ_CORE_CORE_H

#include <QString>

#include "DataParser.h"
#include "core/AppConfig.h"
#include "core/GridModel.h"
#include "coords/CoordTransformer.h"

namespace dataviz::core {

class Core {
public:
    Core() = default;

    bool loadScalarFile(const QString& filePath);
    bool loadVectorFile(const QString& filePath);
    bool loadVectorComponents(const QString& xFilePath, const QString& yFilePath);
    bool transformToMagnetic(const dataviz::coords::CoordTransformer::Options& options);

    void clear();

    const GridModel& model() const { return m_model; }
    GridModel& model() { return m_model; }

    const AppConfig& config() const { return m_config; }
    AppConfig& config() { return m_config; }

    QString lastError() const { return m_lastError; }

private:
    bool loadFileInternal(const QString& filePath, DataParser::DataType dataType);

    GridModel m_model;
    AppConfig m_config;
    dataviz::coords::CoordTransformer m_transformer;
    QString m_lastError;
};

} // namespace dataviz::core

#endif // DATAVIZ_CORE_CORE_H
