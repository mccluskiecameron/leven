#include <string.h>

#define MIN(a, b) ((a)<(b)?(a):(b))
#define SWAP(a, b) do{typeof(a) c = a; a=b; b=c;}while(0)

//more memory conscience version of leven-table.c
//based on the relevant wiki article: wikipedia.org/wiki/Levenshtein_distance
int leven_rows(char * w1, char * w2){
    int l1 = strlen(w1),
        l2 = strlen(w2);
    int i1, i2;
    if(!l1) return l2;
    if(!l2) return l1;
    char * r1 = alloca(l1+1); // preferred over a vla as it allows us to SWAP later
    char * r2 = alloca(l1+1);
    for(i1=0; i1<=l1; i1++) r1[i1] = i1;
    for(i2=0; i2<l2; i2++){
        r2[0] = i2+1;
        for(i1=0; i1<l1; i1++){
            int d_a = r1[i1+1] + 1,
                d_b = r2[i1] + 1,
                d_c = r1[i1] + (w1[i1]==w2[i2]?0:1);
            r2[i1+1] = MIN(MIN(d_a, d_b), d_c);
        }
        SWAP(r1, r2);
    }
    return r1[l1];
}
