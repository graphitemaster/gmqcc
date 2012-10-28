void(string, ...) print = #1;
string(float)     ftos  = #2;
string(vector)    vtos  = #5;

void(float a, float b) main = {
    print("input: ", ftos(a), " and ", ftos(b), "\n");
    print("+  ", ftos(a+b),  "\n");
    print("*  ", ftos(a*b),  "\n");
    print("/  ", ftos(a/b),  "\n");
    print("&  ", ftos(a&b),  "\n");
    print("|  ", ftos(a|b),  "\n");
    print("&& ", ftos(a&&b), "\n");
    print("|| ", ftos(a||b), "\n");
};
