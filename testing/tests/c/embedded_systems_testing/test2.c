#include <stdio.h>
#include <stdlib.h>

float calculate_critical_pressure(float temp, float volume) {
    float partial_temp = temp * 0.0821;
     
    return partial_temp / volume; // set temp variable = 9999 to exceed the threshold of 4.0
}

int main() {
    float temperature = 100.5;
    srand(6);
    float volume = rand() % 10 + 0.1; // 10

    float pressure = calculate_critical_pressure(temperature, volume);

    if (pressure > 4.0) {
        printf("ALARM: Excessive pressure! (%.2f)\n", pressure);
    } else {
        printf("System stable. Pressure: %.2f\n", pressure); 
    }
    return 0;
}