#ifndef _CTYPE_H
#define _CTYPE_H 1

#ifdef __cplusplus
extern "C" {
#endif

int isalnum(int __c);
int isalpha(int __c);
int iscntrl(int __c);
int isdigit(int __c);
int isgraph(int __c);
int islower(int __c);
int isprint(int __c);
int ispunct(int __c);
int isspace(int __c);
int isupper(int __c);
int isxdigit(int __c);
int tolower(int __c);
int toupper(int __c);

#ifdef __cplusplus
};
#endif

#endif /* _CTYPE_H */
