void(string, ...) print = #1;
string(float) ftos = #2;

void(float a, float b) main = {
    if (a == b) print("eq,");
    if (a != b) print("ne,");
    if (a >  b) print("gt,");
    if (a <  b) print("lt,");
    if (a >= b) print("ge,");
    if (a <= b) print("le,");
};
