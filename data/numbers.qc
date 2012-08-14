/* this is the WIP test for the parser...
 * constantly adding stuff here to see if things break
 */
void(string)        print  = #1;
void(string,string) print2 = #1;
void(string,string,string) print3 = #1;
string(float)       ftos   = #2;

void() main = {
    float a, b, c;
    a = 3 + 4 + 5;
    b = (5 * 2) + 1;
    c = 3 & 1;
    c = 1 | 2;
    a = 3 && 4;
    b = 0 && 4;
    c = 4 && 0;
    a = 1 || 1;
    b = 1 || 0;
    c = 0 || 1;
    a = 0 || 0;
};
