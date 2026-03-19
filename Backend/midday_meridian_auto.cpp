#include <iostream>
#include <cmath>
#include <chrono>


int main(){

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm* gmt = std::gmtime(&time_t_now);;

    int hours = gmt->tm_hour;   
    int minutes = gmt->tm_min; 

    // Каждый час = 15, каждая минута = 0.25
    double middayMeridian = 180.0 - (hours * 15.0 + minutes * 0.25);
    
    if (hours >24 || minutes >60 )
    {
        std::cout << "Ошибка!" << std::endl;
    }else {
        
        
        
        while (middayMeridian > 180.0) middayMeridian -= 360.0;
        while (middayMeridian < -180.0) middayMeridian += 360.0;

        
        std::cout << hours  << std::endl;
        std::cout << minutes << std::endl;
    } 

    std::cout << middayMeridian << std::endl;

    return 0;
}