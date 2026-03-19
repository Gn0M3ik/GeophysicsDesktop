#include <iostream>
#include <cmath>


int main(){
    int timeUTC ; 

    setlocale(LC_ALL, "Russian");

    std::cout <<"введите время UTS в форматее 0011 где 00 часы 11 минуты \n" ;
    std::cin >> timeUTC ;

    int hours = timeUTC / 100;   
    int minutes = timeUTC % 100; 

    // Каждый час = 15, каждая минута = 0.25
    double middayMeridian = 180.0 - (hours * 15.0 + minutes * 0.25);
    
    if (hours >24 || minutes >60 )
    {
        std::cout << "Ошибка!" << std::endl;
    }else {
        

        
        while (middayMeridian > 180.0) middayMeridian -= 360.0;
        while (middayMeridian < -180.0) middayMeridian += 360.0;

        
        std::cout << hours << std::endl;
        std::cout << minutes << std::endl;
    } 

    std::cout << middayMeridian << std::endl;

    return 0;
}
