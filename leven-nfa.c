#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/**leven-nfa.c
 * stolen shamelessly from gist.github.com/491973
 * this method has an advantage over the previous of only being slow on the
 * creation of the dfa for one word, after which it will be quite quick to
 * match against any further word. It has the disadvantage of being much slower
 * than them for that first step, and of requiring some fixed `k`, beyond which
 * it won't match. Any increase in k will, of course, slow the creation of the
 * dfa down still. This method is useful in theory for matching against some
 * large corpus of potential words, as discussed in the blogpost accompanying
 * the gist. Unfortunately it is not fast enough to be done as a person types
 * (for practically large strings and `k`s). 
 */
#include "uthash.h"
//can be found at raw.githubusercontent.com/troydhanson/uthash/master/src/uthash.h

// the HASH_* macros have a tendency to evaluate their arguments several times
#define ADD_INT(head, field, ...) do{                       \
        typeof(__VA_ARGS__) _tmp= __VA_ARGS__;              \
        HASH_ADD_INT(head, field, _tmp);                    \
    }while(0)
#define REP_INT(head, field, ...) do{                       \
        typeof(__VA_ARGS__) _tmp= __VA_ARGS__;              \
        typeof(__VA_ARGS__) _pt;                            \
        HASH_REPLACE_INT(head, field, _tmp, _pt);           \
        free(_pt);                                          \
    }while(0)
#define GET_INT(head, val, field, arg) do{                                      \
        HASH_FIND_INT(head, &val, arg);                                         \
        if(!arg)ADD_INT(head, field, (arg=NEW((typeof(*head)){.field=val})));   \
    }while(0)

#define FOR_ALL(head, i, ...) do{typeof(head) i, _tmp; HASH_ITER(hh, head, i, _tmp){__VA_ARGS__ }}while(0)
#define DEL_ALL(head) FOR_ALL(head, _it, HASH_DEL(head, _it); free(_it);)
#define DEL_ALL_2(head, field) FOR_ALL(head, _it2, DEL_ALL(_it2->field); HASH_DEL(head, _it2); free(_it2);)

#define NEW(...) ({typeof(__VA_ARGS__) * p = malloc(sizeof(__VA_ARGS__)); *p = __VA_ARGS__; p;})

#define NOP do{}while(0)

const int NFA_EPS = -1;
const int NFA_ANY = -2;

typedef struct {
    int state;
    UT_hash_handle hh;
} states;

typedef struct {
    int state;
    struct nfa_inputs {
        int match;
        states * sts;
        UT_hash_handle hh;
    } * inp;
    UT_hash_handle hh;
} nfa_trans;

typedef struct nfa_inputs nfa_inputs;

states * state_dup(states * s){
    states * new = NULL;
    FOR_ALL(s, i, 
        ADD_INT(new, state, NEW((states){.state=i->state}));
    );
    return new;
}

void state_free(states * sts){
    DEL_ALL(sts);
}

void choose_gen(int max, int **space){
    if(*space)return;
    *space = malloc((max+1)*(max+1) * sizeof(int));
    int k, f, n, i;
    for(n=0; n<=max; n++){
        for(k=0; k<=n; k++){
            f = 1;
            for(i=k+1; i<=n; i++){
                (*space)[n*(max+1)+k] = f;
                f *= i;
                f /= i-k;
            }
            (*space)[n*(max+1)+k] = f;
        }
    }
}

int choose(int n, int k, int max, int * space){
    return space[n*(max+1)+k];
}

int state_cmp(states * a, states * b){
    return a->state - b->state;
}

int state_freeze(states ** sts, int max){
    static int * space;
    choose_gen(max, &space);
    states * i, * tmp;
    int k = HASH_COUNT(*sts),
        a = 0,
        ne = max,
        pb = -1,
        ind = 0,
        diff, ie;
    HASH_SORT(*sts, state_cmp);
    HASH_ITER(hh, *sts, i, tmp){
        diff = i->state - pb - 1 + k - ind;
        pb = i->state;
        ie = ne - diff;
        if(ie>0)
            a += choose(ie-1, k-ind, max, space);
        ind++;
        ne = max - i->state - 1;
    }
    return a*(max+1) + k;
}

typedef struct NFA {
    nfa_trans * trans;
    states * fin;
    int start;
} NFA;

NFA * nfa_init(int start){
    return NEW((NFA){.start=start});
}

void nfa_free(NFA * nfa){
    state_free(nfa->fin);
    FOR_ALL(nfa->trans, ti, 
        DEL_ALL_2(ti->inp, sts); 
        HASH_DEL(nfa->trans, ti);
        free(ti);
    );
    free(nfa);
}

void nfa_add_trans(NFA * nfa, int src, int inp, int dest){
    nfa_trans * t;
    nfa_inputs * i;
    GET_INT(nfa->trans, src, state, t);
    GET_INT(t->inp, inp, match, i);
    REP_INT(i->sts, state, NEW((states){.state=dest}));
}

void nfa_add_fin(NFA * nfa, int state){
    REP_INT(nfa->fin, state, NEW((states){.state=state}));
}

int nfa_is_fin(NFA * nfa, states * sts){
    states * s, * i, * tmp;
    HASH_ITER(hh, sts, i, tmp){
        HASH_FIND_INT(nfa->fin, &i->state, s);
        if(s) return 1;
    }
    return 0;
}

states * _nfa_expand(NFA * nfa, states * start){
    states * frontier = state_dup(start),
        * i, * i2, * tmp2, * new;
    nfa_trans * t;
    nfa_inputs * inp;
    while(frontier){
        i = frontier;
        HASH_FIND_INT(nfa->trans, &i->state, t);
        if(!t)goto skip;
        HASH_FIND_INT(t->inp, &NFA_EPS, inp);
        if(!inp)goto skip;
        HASH_ITER(hh, inp->sts, i2, tmp2){
            HASH_FIND_INT(start, &i2->state, new);
            if(new)continue;
            ADD_INT(frontier, state, NEW((states){.state=i2->state}));
            ADD_INT(start, state, NEW((states){.state=i2->state}));
        }
    skip:
        HASH_DEL(frontier, i);
        free(i);
    }
    return start;
}

states * nfa_start_state(NFA * nfa){
    states * st = NULL;
    ADD_INT(st, state, NEW((states){.state=nfa->start}));
    return _nfa_expand(nfa, st);
}

states * nfa_next(NFA * nfa,  states * sts, int input){
    states * dest = NULL, 
        *i, *tmp, * i2, * tmp2;
    nfa_trans * t;
    nfa_inputs * inp;
    HASH_ITER(hh, sts, i, tmp){
        HASH_FIND_INT(nfa->trans, &i->state, t);
        if(!t)continue;
        HASH_FIND_INT(t->inp, &input, inp);
        if(inp) HASH_ITER(hh, inp->sts, i2, tmp2) REP_INT(dest, state, NEW((states){.state=i2->state}));
        HASH_FIND_INT(t->inp, &NFA_ANY, inp);
        if(inp) HASH_ITER(hh, inp->sts, i2, tmp2) REP_INT(dest, state, NEW((states){.state=i2->state}));
    }
    return _nfa_expand(nfa, dest);
}

nfa_inputs * get_input(NFA * nfa, states * sts){
    nfa_inputs * inps = NULL, * ii, * itmp;
    states * i, * tmp;
    nfa_trans * t;
    HASH_ITER(hh, sts, i, tmp){
        HASH_FIND_INT(nfa->trans, &i->state, t);
        if(!t) continue;
        HASH_ITER(hh, t->inp, ii, itmp)
            REP_INT(inps, match, NEW((nfa_inputs){.match=ii->match}));
    }
    return inps;
}

typedef struct DFA {
    int start;
    struct dfa_trans {
        int state;
        struct dfa_inputs {
            int inp;
            int state;
            UT_hash_handle hh;
        } * inps;
        UT_hash_handle hh;
    } * trans;
    struct dfa_defaults {
        int src;
        int dest;
        UT_hash_handle hh;
    } * def;
    states * fin;
} DFA;

typedef struct dfa_trans dfa_trans;
typedef struct dfa_inputs dfa_inputs;
typedef struct dfa_defaults dfa_defaults;

DFA * dfa_init(int start){
    return NEW((DFA){.start=start});
}

void dfa_free(DFA * dfa){
    DEL_ALL_2(dfa->trans, inps);
    DEL_ALL(dfa->def);
    state_free(dfa->fin);
    free(dfa);
}

void dfa_add_trans(DFA * dfa, int src, int inp, int dest){
    dfa_trans * t;
    GET_INT(dfa->trans, src, state, t);
    REP_INT(t->inps, inp, NEW((dfa_inputs){.inp=inp, .state=dest}));
}

void dfa_add_def(DFA * dfa, int src, int dest){
    REP_INT(dfa->def, src, NEW((dfa_defaults){.src=src, .dest=dest}));
}

void dfa_add_fin(DFA * dfa, int state){
    REP_INT(dfa->fin, state, NEW((states){.state=state}));
}

int dfa_is_fin(DFA * dfa, int state){
    states * st;
    HASH_FIND_INT(dfa->fin, &state, st);
    return !!st;
}

int dfa_next(DFA * dfa, int src, int inp){
    dfa_trans * t;
    dfa_inputs * i;
    dfa_defaults * d;
    HASH_FIND_INT(dfa->trans, &src, t);
    if(!t) goto def;
    HASH_FIND_INT(t->inps, &inp, i);
    if(!i) goto def;
    return i->state;
def:
    HASH_FIND_INT(dfa->def, &src, d);
    if(d) return d->dest;
    return 0;
}

DFA * nfa_dfa(NFA * nfa, int max){
    states * starts = nfa_start_state(nfa),
        * seen = NULL, *found;
    nfa_inputs * ii, * itmp;
    int startsfr = state_freeze(&starts, max);
    DFA * dfa = dfa_init(startsfr);
    states ** queue = malloc(sizeof(states*) * 256);
    int queue_size = 256, i=0;
    queue[i] = starts;
    states * cur;
    while(i>=0){
        cur = queue[i--];
        int curfr = state_freeze(&cur, max);
        if(nfa_is_fin(nfa, cur))
            dfa_add_fin(dfa, curfr);
        nfa_inputs * inps = get_input(nfa, cur);
        HASH_ITER(hh, inps, ii, itmp){
            if(ii->match == NFA_EPS) goto cont;
            states * new = nfa_next(nfa, cur, ii->match);
            int newfr = state_freeze(&new, max);
            HASH_FIND_INT(seen, &newfr, found);
            if(!found){
                if(i > queue_size-3) 
                    queue = realloc(queue, sizeof(states*) * (queue_size *= 2));
                queue[++i] = new;
                ADD_INT(seen, state, NEW((states){.state=newfr}));
            }else{
                state_free(new);
            }
            if(ii->match == NFA_ANY) dfa_add_def(dfa, curfr, newfr);
            else dfa_add_trans(dfa, curfr, ii->match, newfr);
        cont:
            HASH_DEL(inps, ii);
            free(ii);
        }
        state_free(cur);
    }
    free(queue);
    state_free(seen);
    return dfa;
}

int max;

DFA * leven_nfa_init(char * w1, int k){
#define S(n, e) ((n) * (k+1) + (e))
    int i;
    NFA * nfa = nfa_init(S(0, 0));
    for(i=0; w1[i]; i++){
        for(int e=0; e<=k; e++){
            nfa_add_trans(nfa, S(i, e), w1[i], S(i+1, e));
            if(e<k){
                nfa_add_trans(nfa, S(i, e), NFA_ANY, S(i, e+1));
                nfa_add_trans(nfa, S(i, e), NFA_EPS, S(i+1, e+1));
                nfa_add_trans(nfa, S(i, e), NFA_ANY, S(i+1, e+1));
            }
        }
    }
    for(int e=0; e<=k; e++){
        if(e<k) nfa_add_trans(nfa, S(i, e), NFA_ANY, S(i, e+1));
        nfa_add_fin(nfa, S(i, e));
    }
    DFA * dfa = nfa_dfa(nfa, S(i, k+1));
    max = S(i, k+1);
    nfa_free(nfa);
    return dfa;
#undef S
}

int leven_nfa(DFA * dfa, char * w2){
    int state = dfa->start;
    for(int i=0; w2[i]; i++){
        state = dfa_next(dfa, state, w2[i]);
    }
    return dfa_is_fin(dfa, state); //alas, this method does not make it easy to determine the actual
                                   //distance. instead we return whether it is within k
}

//we create a filter, not unlike grep, that matches lines within argv[2] of argv[1]
#ifdef MAIN
int main(int argc, char ** argv){
    int n;
    char buf[1024];
    if(argc<3) return 1;
    DFA * dfa = leven_nfa_init(argv[1], atoi(argv[2]));
    while((n=read(0, buf, 1024))){
        if(buf[n-1]=='\n') buf[n-1] = '\0';
        else               buf[n] = '\0';
        if(leven_nfa(dfa, buf)) printf("%s\n", buf);
    }
    dfa_free(dfa);
}
#endif
