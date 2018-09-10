#include <string.h>

#define MIN(a, b) ((a)<(b)?(a):(b))

//based on the relevant wiki article: wikipedia.org/wiki/Levenshtein_distance
int leven_rec(char * w1, char * w2){
    int d=0;
    if(!*w1) return strlen(w2);
    if(!*w2) return strlen(w1);
    if(*w1 != *w2) d=1;
    int d_a = leven_rec2(w1+1, w2);
    int d_b = leven_rec2(w1, w2+1);
    int d_c = leven_rec2(w1+1, w2+1);
    return d + MIN(MIN(d_a, d_b), d_c);
}
