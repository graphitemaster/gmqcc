void(string, ...) print = #1;
string(float)     ftos  = #2;

void(vector a, vector b) main = {
    print("dot = ", ftos(a*b), "\n");
};
