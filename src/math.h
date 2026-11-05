#ifndef _MATH_H
#define _MATH_H

// Constante matematice standard
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830961
#define M_E        2.71828182845904523536

// Macro-uri pentru clasificarea numerelor în virgulă mobilă
#define NAN (__builtin_nan(""))
#define INFINITY (__builtin_inff())

int isnan(double x);
int isinf(double x);
int finite(double x);

// Funcții trigonometrice de bază
double sin(double x);
double cos(double x);
double tan(double x);
double atan2(double y, double x);
double asin(double x);
double acos(double x);

// Funcții exponențiale, logaritmice și puteri
double sqrt(double x);
double pow(double base, double exp);
double exp(double x);
double log(double x);
double log10(double x);

// Funcții de rotunjire și manipulare
double ceil(double x);
double floor(double x);
double fabs(double x);
double fmod(double x, double y);
double ldexp(double x, int exp);
double frexp(double value, int *exp);

#ifndef HUGE_VAL
#define HUGE_VAL (__builtin_inf())
#endif

double log2(double x);

#endif // _MATH_H