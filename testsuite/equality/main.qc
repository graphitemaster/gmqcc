void(string, ...) print = #1;
string(float)     ftos  = #2;

void(float a, float b) main = {
    if (a == b) print("eq\n");
    if (a != b) print("ne\n");
    if (a >  b) print("gt\n");
    if (a <  b) print("lt\n");
    if (a >= b) print("ge\n");
    if (a <= b) print("le\n");
};
