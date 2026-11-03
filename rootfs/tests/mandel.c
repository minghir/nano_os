#include "nano_libc.h"

// O mică funcție ajutătoare pentru a printa un singur caracter
void print_char(char c) {
    char buf[2];
    buf[0] = c;
    buf[1] = '\0';
    nano_print(buf);
}

int main() {
    nano_print("\n=== Fractal Mandelbrot in Nano OS ===\n\n");

    // Dimensiunile ecranului (lăsăm câteva rânduri libere pentru prompt)
    int width = 76;
    int height = 20; 
    int max_iter = 30; // De câte ori calculăm ecuația (mai mult = mai precis)

    // Parcurgem ecranul rând cu rând și coloană cu coloană
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            
            // Mapăm consolă VGA la planul complex (matematic)
            // Lărgim pe X (aprox -2.0 la 1.0) și pe Y (aprox -1.0 la 1.0)
            float c_re = (col - width / 1.5f) * 3.0f / width;
            float c_im = (row - height / 2.0f) * 2.2f / height;

            float x = 0, y = 0;
            int iter = 0;

            // Z = Z^2 + C (Magia Mandelbrot)
            while (x * x + y * y <= 4.0f && iter < max_iter) {
                float x_new = x * x - y * y + c_re;
                y = 2.0f * x * y + c_im;
                x = x_new;
                iter++;
            }

            // Alegem caracterul în funcție de cât de repede a "scăpat" la infinit
            if (iter < max_iter) {
                // Paleta noastră de culori text (de la cel mai slab la cel mai "dens")
                char charset[] = " .,~:;=!*#$@";
                
                // Calculăm indexul potrivit din paletă
                int index = (iter * 11) / max_iter;
                print_char(charset[index]);
            } else {
                // Dacă nu a scăpat, e în centrul setului (desenăm spațiu gol)
                print_char(' ');
            }
        }
        nano_print("\n"); // Trecem la rândul următor
    }

    nano_print("\nWow! Matematica ruleaza pe bare-metal!\n");
    return 0;
}