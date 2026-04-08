#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QTimer>

#include "DataParser.h"
#include "MainWindow.h"

int main(int argc, char* argv[]) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("Data Visualizer");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("Geophysics Lab");
    QCoreApplication::setOrganizationDomain("geophysics.lab");

    QCommandLineParser parser;
    parser.setApplicationDescription("Tool for geophysical data visualization");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption fileOption(
        QStringList() << "f" << "file",
        "Open file at startup",
        "file");
    parser.addOption(fileOption);

    QCommandLineOption typeOption(
        QStringList() << "t" << "type",
        "Data type hint: scalar or vector",
        "type",
        "scalar");
    parser.addOption(typeOption);

    parser.addPositionalArgument("path", "Optional data file path.");
    parser.process(app);

    MainWindow mainWindow;
    mainWindow.show();

    QString fileName = parser.value(fileOption).trimmed();
    if (fileName.isEmpty()) {
        const QStringList positional = parser.positionalArguments();
        if (!positional.isEmpty()) {
            fileName = positional.first();
        }
    }

    QString typeValue = parser.value(typeOption).trimmed().toLower();
    if (typeValue != "scalar" && typeValue != "vector") {
        typeValue = "scalar";
    }
    const DataParser::DataType dataType =
        (typeValue == "vector") ? DataParser::VECTOR_DATA : DataParser::SCALAR_DATA;

    if (!fileName.isEmpty() && QFileInfo::exists(fileName)) {
        QTimer::singleShot(200, [&mainWindow, fileName, dataType]() {
            mainWindow.loadFile(fileName, dataType);
        });
    }

    return app.exec();
}
