#include <stdio.h>
#include <stdlib.h>

typedef struct {
     float q; // Process noise
     float r; // Measurement noise
     float x; // Estimated value
     float p; // Estimation error
     float k; // Kalman gain
} KalmanFilter;

int main() {
    KalmanFilter f = {0.1, 0.5, 0.0, 1.0, 0.0};
    float measurements[10];
    srand(6);
    
    // Simulating 10 readings
    for (int count = 0; count < 10; count++)
        measurements[count] = (rand() % 100 + 1);
    // Generated measurements: {42.0, 86.0, 13.0, 66.0, 9.0, 86.0, 87.0, 44.0, 3.0, 79.0};

    printf("Starting trajectory estimation...\n");
    for (int i = 0; i < 10; i++) {

        f.p = f.p + f.q;
        // Update phase
        f.k = f.p / (f.p + f.r);
        f.x = f.x + f.k * (measurements[i] - f.x);

        f.p = (1 - f.k) * f.p;

        printf("\tStep %d - Measurement: %.1f, Calculated estimate: %.2f\n", i, measurements[i], f.x);
    }

    return 0;
}