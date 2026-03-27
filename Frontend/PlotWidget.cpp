#include "PlotWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QStyle>
#include <cmath>
#include <algorithm>

PlotWidget::PlotWidget(QWidget *parent)
    : QWidget(parent),
      m_showGrid(true),
      m_showVectors(false),
      m_showTerminator(false),
      m_colorScheme(0),
      m_scaleFactor(1.0),
      m_offsetX(0.0),
      m_offsetY(0.0),
      m_cacheValid(false) {
    
    setMinimumSize(400, 300);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void PlotWidget::setData(const QVector<GridPoint>& points, const GridMetadata& metadata) {
    m_points = points;
    m_metadata = metadata;
    m_cacheValid = false;
    update();
}

void PlotWidget::setShowGrid(bool show) {
    m_showGrid = show;
    m_cacheValid = false;
    update();
}

void PlotWidget::setShowVectors(bool show) {
    m_showVectors = show;
    m_cacheValid = false;
    update();
}

void PlotWidget::setShowTerminator(bool show) {
    m_showTerminator = show;
    m_cacheValid = false;
    update();
}

void PlotWidget::setColorScheme(int scheme) {
    m_colorScheme = scheme;
    m_cacheValid = false;
    update();
}

void PlotWidget::paintEvent(QPaintEvent *event) {
    if (!m_cacheValid || m_cachePixmap.size() != size()) {
        m_cachePixmap = QPixmap(size());
        m_cachePixmap.fill(Qt::white);
        
        QPainter cachePainter(&m_cachePixmap);
        cachePainter.set::Antialiasing);
        
        drawBackground(cachePainter);
        
        if (m_showGrid) {
            drawGrid(cachePainter);
        }
        
        drawPoints(cachePainter);
        
        if (m_showVectors) {
            drawVectors(cachePainter);
        }
        
        if (m_showTerminator) {
            drawTerminator(cachePainter);
        }
        
        drawColorBar(cachePainter);
        
        m_cacheValid = true;
    }
    
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_cachePixmap);
}

void PlotWidget::resizeEvent(QResizeEvent *event) {
    m_cacheValid = false;
    QWidget::resizeEvent(event);
}

void PlotWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        setFocus();
    }
    QWidget::mousePressEvent(event);
}

void PlotWidget::mouseMoveEvent(QMouseEvent *event) {
    // Позиционирование мыши для статуса (если нужно)
    QWidget::mouseMoveEvent(event);
}

void PlotWidget::wheelEvent(QWheelEvent *event) {
    // Зум колесом мыши
    if (event->angleDelta().y() > 0) {
        m_scaleFactor *= 1.1;
    } else {
        m_scaleFactor *= 0.9;
    }
    
    m_scaleFactor = qBound(0.1, m_scaleFactor, 10.0);
    m_cacheValid = false;
    update();
}

void PlotWidget::drawBackground(QPainter &painter) {
    painter.fillRect(rect(), Qt::white);
}

void PlotWidget::drawGrid(QPainter &painter) {
    painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));
    
    if (m_metadata.latStep > 0 && m_metadata.lonStep > 0) {
        // Вертикальные линии (долгота)
        for (double lon = m_metadata.minLon; lon <= m_metadata.maxLon; lon += m_metadata.lonStep) {
            QPointF start = gridToScreen(m_metadata.minLat, lon);
            QPointF end = gridToScreen(m_metadata.maxLat, lon);
            
            if (start.x() >= 0 && start.x() <= width() && 
                end.x() >= 0 && end.x() <= width()) {
                painter.drawLine(start, end);
            }
        }
        
        // Горизонтальные линии (широта)
        for (double lat = m_metadata.minLat; lat <= m_metadata.maxLat; lat += m_metadata.latStep) {
            QPointF start = gridToScreen(lat, m_metadata.minLon);
            QPointF end = gridToScreen(lat, m_metadata.maxLon);
            
            if (start.y() >= 0 && start.y() <= height() && 
                end.y() >= 0 && end.y() <= height()) {
                painter.drawLine(start, end);
            }
        }
    }
}

void PlotWidget::drawPoints(QPainter &painter) {
    if (m_points.isEmpty()) return;
    
    // Найти мин/макс значения для цветовой шкалы
    double minVal = m_points[0].value;
    double maxVal = m_points[0].value;
    
    for (const auto &point : m_points) {
        minVal = std::min(minVal, point.value);
        maxVal = std::max(maxVal, point.value);
    }
    
    // Рисуем точки
    for (const auto &point : m_points) {
        QPointF pos = gridToScreen(point.latitude, point.longitude);
        
        // Проверяем, находится ли точка в видимой области
        if (pos.x() >= -10 && pos.x() <= width() + 10 && 
            pos.y() >= -10 && pos.y() <= height() + 10) {
            
            QColor color = getValueColor(point.value, minVal, maxVal, m_colorScheme);
            painter.setPen(color);
            painter.setBrush(color);
            
            // Размер точки в зависимости от масштаба
            int radius = qMax(1, static_cast<int>(3 * m_scaleFactor));
            painter.drawEllipse(pos, radius, radius);
        }
    }
}

void PlotWidget::drawVectors(QPainter &painter) {
    if (m_points.isEmpty() || !m_showVectors) return;
    
    painter.setPen(QPen(Qt::red, 1));
    
    for (const auto &point : m_points) {
        QPointF center = gridToScreen(point.latitude, point.longitude);
        
        // Масштабируем вектор (для визуализации)
        double scale = 10.0; // можно сделать параметром
        QPointF end(center.x() + point.vx * scale, 
                   center.y() - point.vy * scale); // минус для корректного направления
        
        if (center.x() >= 0 && center.x() <= width() && 
            center.y() >= 0 && center.y() <= height()) {
            painter.drawLine(center, end);
            
            // Нарисовать стрелку
            double angle = std::atan2(end.y() - center.y(), end.x() - center.x());
            double arrowSize = 3.0;
            
            QPointF arrowP1 = end - QPointF(sin(angle - M_PI/6) * arrowSize,
                                          cos(angle - M_PI/6) * arrowSize);
            QPointF arrowP2 = end - QPointF(sin(angle + M_PI/6) * arrowSize,
                                          cos(angle + M_PI/6) * arrowSize);
            
            painter.drawLine(end, arrowP1);
            painter.drawLine(end, arrowP2);
        }
    }
}

void PlotWidget::drawTerminator(QPainter &painter) {
    // TODO: Реализовать линию терминатора
    // Это сложная математика, зависящая от времени и положения Солнца
    // Пока рисуем примерную линию
    
    if (m_metadata.dateTime.isValid()) {
        painter.setPen(QPen(Qt::blue, 2, Qt::DashLine));
        
        // Пример: вертикальная линия (для демонстрации)
        QPointF start = gridToScreen(m_metadata.minLat, 0);
        QPointF end = gridToScreen(m_metadata.maxLat, 0);
        
        painter.drawLine(start, end);
    }
}

void PlotWidget::drawColorBar(QPainter &painter) {
    QRect barRect(width() - 60, 20, 40, height() - 40);
    
    // Найти мин/макс для шкалы
    double minVal = m_points.isEmpty() ? 0 : m_points[0].value;
    double maxVal = m_points.isEmpty() ? 100 : m_points[0].value;
    
    for (const auto &point : m_points) {
        minVal = std::min(minVal, point.value);
        maxVal = std::max(maxVal, point.value);
    }
    
    // Рисуем градиент
    QLinearGradient gradient(barRect.topRight(), barRect.bottomRight());
    gradient.setColorAt(0.0, getValueColor(maxVal, minVal, maxVal, m_colorScheme));
    gradient.setColorAt(1.0, getValueColor(minVal, minVal, maxVal, m_colorScheme));
    
    painter.fillRect(barRect, gradient);
    
    // Рамка
    painter.setPen(Qt::black);
    painter.drawRect(barRect);
    
    // Метки
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 8));
    
    painter.drawText(barRect.adjusted(5, 0, -5, 0), Qt::AlignTop | Qt::AlignLeft, 
                    QString::number(maxVal, 'f', 2));
    painter.drawText(barRect.adjusted(5, 0, -5, 0), Qt::AlignBottom | Qt::AlignLeft, 
                    QString::number(minVal, 'f', 2));
}

QPointF PlotWidget::gridToScreen(double lat, double lon) const {
    double x = (lon - m_metadata.minLon) / 
               (m_metadata.maxLon - m_metadata.minLon) * width();
    double y = (1.0 - (lat - m_metadata.minLat) / 
               (m_metadata.maxLat - m_metadata.minLat)) * height();
    
    // Применяем трансформации
    x = x * m_scaleFactor + m_offsetX;
    y = y * m_scaleFactor + m_offsetY;
    
    return QPointF(x, y);
}

double PlotWidget::screenToGridX(int x) const {
    double normalizedX = (x - m_offsetX) / m_scaleFactor;
    return m_metadata.minLon + 
           (normalizedX / width()) * (m_metadata.maxLon - m_metadata.minLon);
}

double PlotWidget::screenToGridY(int y) const {
    double normalizedY = (y - m_offsetY) / m_scaleFactor;
    return m_metadata.minLat + 
           (1.0 - normalizedY / height()) * (m_metadata.maxLat - m_metadata.minLat);
}

QColor PlotWidget::getValueColor(double value, double minVal, double maxVal, int colorScheme) const {
    if (minVal == maxVal) {
        return Qt::gray;
    }
    
    double t = (value - minVal) / (maxVal - minVal);
    t = qBound(0.0, t, 1.0);
    
    switch (colorScheme) {
    case 0: // Синий-Красный
        return QColor::fromHsvF(0.6 - t * 0.6, 1.0, 0.8);
    case 1: // Черно-Белый
        return QColor::fromRgbF(t, t, t);
    case 2: // Зеленый-Красный
        return QColor::fromHsvF(0.33 - t * 0.33, 1.0, 0.8);
    case 3: // Цветной (спектр)
        return QColor::fromHsvF(t, 1.0, 0.8);
    default:
        return QColor::fromHsvF(0.6 - t * 0.6, 1.0, 0.8);
    }
}