#include "MainWindow.h"
#include "PlotWidget.h"
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    
    plotWidget = new PlotWidget(this);
    setCentralWidget(plotWidget);
    
    createActions();
    createMenus();
    createToolBars
    createStatusBar();
    readSettings();

    setWindowTitle(tr("Data Visualizer"));
    resize(1024, 768);
}

MainWindow::~MainWindow() {}

void MainWindow::createActions() {
    // Открыть скалярный файл
    openScalarAct = new QAction(tr("&Open Scalar File..."), this);
    openScalarAct->setShortcuts(QKeySequence::Open);
    openScalarAct->setStatusTip(tr("Open a scalar data file"));
    connect(openScalarAct, &QAction::triggered, this, &MainWindow::openScalarFile);

    // Открыть векторный файл
    openVectorAct = new QAction(tr("Open &Vector File..."), this);
    openVectorAct->setShortcut(QKeySequence::UnknownKey);
    openVectorAct->setStatusTip(tr("Open a vector data file"));
    connect(openVectorAct, &QAction::triggered, this, &MainWindow::openVectorFile);

    // Выход
    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, &QAction::triggered, this, &MainWindow::close);

    // О программе
    aboutAct = new QAction(tr("&About"), this);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    connect(aboutAct, &QAction::triggered, this, &MainWindow::about);

    // О Qt
    aboutQtAct = new QAction(tr("About &Qt"), this);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
    connect(aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::createMenus() {
    // Меню Файл
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openScalarAct);
    fileMenu->addAction(openVectorAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    // Меню Вид (временно пустое)
    viewMenu = menuBar()->addMenu(tr("&View"));

    // Меню Помощь
    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);
    helpMenu->addAction(aboutQtAct);
}

void MainWindow::createToolBars() {
    // Пока без тулбаров
}

void MainWindow::createStatusBar() {
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::readSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
    if (geometry.isEmpty()) {
        const QRect availableGeometry = screen()->availableGeometry();
        resize(availableGeometry.width() / 3, availableGeometry.height() / 2);
        move((availableGeometry.width() - width()) / 2,
             (availableGeometry.height() - height()) / 2);
    } else {
        restoreGeometry(geometry);
    }
}

void MainWindow::writeSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("geometry", saveGeometry());
}

void MainWindow::openScalarFile() {
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open Scalar Data File"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Data Files (*.00 *.txt *.dat);;All Files (*)"));
    
    if (!fileName.isEmpty()) {
        loadFile(fileName, DataParser::SCALAR_DATA);
    }
}

void MainWindow::openVectorFile() {
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open Vector Data File"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Data Files (*.00 *.txt *.dat);;All Files (*)"));
    
    if (!fileName.isEmpty()) {
        loadFile(fileName, DataParser::VECTOR_DATA);
    }
}

void MainWindow::loadFile(const QString &fileName, DataParser::DataType dataType) {
    statusBar()->showMessage(tr("Loading file..."), 2000);
    
    QVector<GridPoint> points;
    GridMetadata metadata;
    
    if (DataParser::parseFile(fileName, metadata, points)) {
        dataPoints = points;
        dataMetadata = metadata;
        plotWidget->setData(dataPoints, dataMetadata);
        
        curFile = fileName;
        setWindowTitle(tr("%1 - Data Visualizer").arg(strippedName(curFile)));
        
        statusBar()->showMessage(tr("File loaded successfully"), 2000);
    } else {
        QMessageBox::critical(this, tr("Error"),
            tr("Could not load file:\n%1\n\nError: %2")
            .arg(QDir::toNativeSeparators(fileName))
            .arg(DataParser::getLastError()));
        statusBar()->showMessage(tr("Loading failed"), 2000);
    }
}

QString MainWindow::strippedName(const QString &fullFileName) {
    return QFileInfo(fullFileName).fileName();
}

void MainWindow::about() {
    QMessageBox::about(this, tr("About Data Visualizer"),
        tr("<h2>Data Visualizer %1</h2>"
           "<p>A desktop application for visualizing geophysical data.</p>"
           "<p>This application can display scalar and vector fields "
           "on geographic maps with various visualization options.</p>")
        .arg(QApplication::applicationVersion()));
}

