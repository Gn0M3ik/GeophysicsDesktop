#include <QApplication>
#include "PlotWidget.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    PlotWidget widget;
    widget.resize(1000, 700);
    widget.show();
    
    // Построение сетки после показа окна (корректная инициализация OpenGL-контекста)
    widget.buildCartesianGrid(40, 40, 20);
    
    return app.exec();
}
