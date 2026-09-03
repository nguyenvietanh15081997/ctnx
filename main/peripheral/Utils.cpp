
#include "math.h"
#include "Utils.h"

void Utils::convert_hsv_to_rgb(double H, double S, double V, uint8_t *Red, uint8_t *Green, uint8_t *Blue, uint8_t *Dim){
    double r, g, b;

    *Dim = V/10;
    H = H/10;
    S = S/10;
    V = 100;
    // Clamp inputs
    if (S < 0) S = 0; 
    if (S > 100) S = 100;
    if (V < 0) V = 0; 
    if (V > 100) V = 100;

    // Normalize H to [0, 360)
    H = fmod(H, 360.0);
    if (H < 0.0) H += 360.0;

    S /= 100.0;
    V /= 100.0;

    int i = (int)(H / 60.0);
    double f = (H / 60.0) - i;
    double p = V * (1 - S);
    double q = V * (1 - S * f);
    double t = V * (1 - S * (1 - f));

    switch (i) {
        case 0: r = V; g = t; b = p; break;
        case 1: r = q; g = V; b = p; break;
        case 2: r = p; g = V; b = t; break;
        case 3: r = p; g = q; b = V; break;
        case 4: r = t; g = p; b = V; break;
        case 5: r = V; g = p; b = q; break;
        default: r = g = b = 0; break;
    }

    *Red = (uint8_t)(r * 255.0 + 0.5);
    *Green = (uint8_t)(g * 255.0 + 0.5);
    *Blue = (uint8_t)(b * 255.0 + 0.5);

    // printf("[LC8823] R: %d G: %d B: %d\n", *Red, *Green, *Blue);
}