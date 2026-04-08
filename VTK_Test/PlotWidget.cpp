#include "PlotWidget.h"

#include <QVBoxLayout>
#include <vtkStructuredGrid.h>
#include <vtkPoints.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkGeometryFilter.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkLookupTable.h>
#include <vtkCamera.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

PlotWidget::PlotWidget(QWidget *parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 1. Qt-виджет для встраивания VTK
    vtkWidget = new QVTKOpenGLNativeWidget(this);
    layout->addWidget(vtkWidget);

    // 2. Инициализация VTK-рендер-окна
    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    vtkWidget->setRenderWindow(renderWindow.get());

    // 3. Рендерер
    renderer = vtkSmartPointer<vtkRenderer>::New();
    renderWindow->AddRenderer(renderer.get());
    renderer->SetBackground(0.04, 0.04, 0.08);
    renderer->SetBackground2(0.1, 0.1, 0.15);
    renderer->GradientBackgroundOn();
}

void PlotWidget::clearScene() {
    renderer->RemoveAllViewProps();
    renderWindow->Render();
}

void PlotWidget::buildCartesianGrid(int nx, int ny, int nz) {
    clearScene();

    // 1. Создаём структурированную сетку
    auto grid = vtkSmartPointer<vtkStructuredGrid>::New();
    grid->SetDimensions(nx, ny, nz);

    // 2. Массивы координат и скалярного поля
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataType(VTK_DOUBLE);
    points->SetNumberOfPoints(nx * ny * nz);

    auto tempArray = vtkSmartPointer<vtkDoubleArray>::New();
    tempArray->SetName("Temperature");
    tempArray->SetNumberOfComponents(1);
    tempArray->SetNumberOfTuples(nx * ny * nz);

    // 3. Заполнение данными (Декартовы координаты X,Y,Z)
    double Lx = 10.0, Ly = 10.0, Lz = 5.0;
    double dx = Lx / (nx - 1), dy = Ly / (ny - 1), dz = Lz / (nz - 1);

    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                double x = i * dx;
                double y = j * dy;
                double z = k * dz;

                // Лёгкая деформация для имитации реальных геофизических данных
                x += 0.15 * std::sin(y * 0.5) * std::cos(z * 0.3);
                y += 0.12 * std::cos(x * 0.5);
                z += 0.08 * std::sin((x + y) * 0.2);

                vtkIdType idx = i + j * nx + k * nx * ny;
                points->SetPoint(idx, x, y, z);

                // Скалярное поле (например, температура/давление)
                double val = 20.0 + 15.0 * std::sin(x * 0.4) * std::cos(y * 0.4) + 8.0 * z;
                tempArray->SetTuple1(idx, val);
            }
        }
    }

    grid->SetPoints(points);
    grid->GetPointData()->SetScalars(tempArray);

    // 4. Извлечение поверхности для рендеринга
    auto surface = vtkSmartPointer<vtkGeometryFilter>::New();
    surface->SetInputData(grid);
    surface->Update();

    // 5. Маппер + цветовая карта
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(surface->GetOutputPort());
    mapper->ScalarVisibilityOn();
    mapper->SetScalarRange(tempArray->GetRange());

    auto lut = vtkSmartPointer<vtkLookupTable>::New();
    lut->SetHueRange(0.65, 0.0); // Синий → Красный
    lut->SetSaturationRange(0.8, 0.9);
    lut->SetValueRange(0.7, 1.0);
    lut->Build();
    mapper->SetLookupTable(lut);

    // 6. Актор
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetEdgeVisibility(true);
    actor->GetProperty()->SetEdgeColor(0.15, 0.15, 0.25);
    actor->GetProperty()->SetLineWidth(0.5);

    renderer->AddActor(actor);

    // 7. Камера
    renderer->ResetCamera();
    renderer->GetActiveCamera()->Azimuth(35);
    renderer->GetActiveCamera()->Elevation(25);
    renderer->ResetCameraClippingRange();

    renderWindow->Render();
}
