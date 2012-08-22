/* this is the WIP test for the parser...
 * constantly adding stuff here to see if things break
 */
void(string)        print  = #1;
void(string,string) print2 = #1;
void(string,string,string) print3 = #1;
string(float)       ftos   = #2;
entity()            spawn  = #3;
void(entity)        kill   = #4;

float multi, decla, ration;
.vector memvec;

.void(string x) printit;

float(vector different_name, vector b) dot;

float(vector a, vector b) dot = {
    return a * b;
};

void(string x) myprintit = {
    print3("-> ", x, "\n");
};

void(vector par) vecpar = {
    // vector-parameters need _x, _y, _z as well
    print3("par_y should be 5... = ", ftos(par_y), "\n");
};

void() main = {
    local entity pawn;
    local vector foovec;
    print3("should be 1: ", ftos(dot('1 1 0', '1 0 0')), "\n");

    foovec = '3 4 5';
    foovec_y = 9;
    pawn = spawn();
    pawn.printit = myprintit;
    pawn.printit("Hello");

    vecpar('1 5 9');
};
