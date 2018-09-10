#include <string.h>

#define MIN(a, b) ((a)<(b)?(a):(b))

//based on the relevant wiki article: wikipedia.org/wiki/Levenshtein_distance
int leven_table(char * w1, char * w2){
    int l1 = strlen(w1),
        l2 = strlen(w2);
    if(!l1) return l2;
    if(!l2) return l1;
    char table[l1][l2];
    for(int i1=0; i1<l1; i1++) for(int i2=0; i2<l2; i2++){
        int d = 0;
        if(w1[i1] != w2[i2]) d++;
        if(i1 && !i2) d += table[i1-1][i2];
        if(!i1 && i2) d += table[i1][i2-1];
        if(i1 && i2) d += MIN(MIN(table[i1-1][i2-1],
                                  table[i1-1][i2]),
                              table[i1][i2-1]);
        table[i1][i2] = d;
    }
    return table[l1-1][l2-1];
}
