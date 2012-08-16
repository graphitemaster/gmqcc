/* this is the WIP test for the parser...
 * constantly adding stuff here to see if things break
 */
void(string)        print  = #1;
void(string,string) print2 = #1;
void(string,string,string) print3 = #1;
string(float)       ftos   = #2;

float CONST_1 = 1;
float CONST_2 = CONST_1 + 1;

void() main = {
    print3("CONST_1 = ", ftos(CONST_1), "\n");
    print3("CONST_2 = ", ftos(CONST_2), "\n");
};
