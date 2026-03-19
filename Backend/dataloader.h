#pragma once
#include <string>
#include <vector>
#include <memory>

struct DataPoint {
    double latitude;
    double longitude;
    double value;
    double vx = 0.0; // вектор X
    double vy = 0.0; // вектор Y
};

struct GridData {
    std::vector<double> latitudes;
    std::vector<double> longitudes;
    std::vector<std::vector<double>> values;
    std::vector<std::vector<double>> vectorsX;
    std::vector<std::vector<double>> vectorsY;
    
    int dayOfYear = 0;
    double universalTime = 0.0;
};

class DataLoader {
public:
    bool load(const std::string& filename);
    const GridData& getData() const { return data_; }
    
private:
    GridData data_;
    bool parseFile(const std::string& filename);
};