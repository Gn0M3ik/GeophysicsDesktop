#pragma once

#include <QWidget>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkGenericOpenGLRenderWindow.h>

class PlotWidget : public QWidget {
    Q_OBJECT

public:
    explicit PlotWidget(QWidget *parent = nullptr);

    /// Построение 3D-сетки в декартовых координатах
    void buildCartesianGrid(int nx = 30, int ny = 30, int nz = 15);

    /// Очистка сцены перед перестроением
    void clearScene();

private:
    QVTKOpenGLNativeWidget* vtkWidget;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow;
    vtkSmartPointer<vtkRenderer> renderer;
};
