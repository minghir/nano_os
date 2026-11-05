#ifndef _CTYPE_H
#define _CTYPE_H

// Verificări de bază pentru caractere ASCII
int isalpha(int c);
int isdigit(int c);
int isalnum(int c);
int isspace(int c);
int isupper(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isxdigit(int c);

// Conversii majuscule / minuscule
int toupper(int c);
int tolower(int c);

#endif // _CTYPE_H