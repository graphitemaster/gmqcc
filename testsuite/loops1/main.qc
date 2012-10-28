void(string, ...) print = #1;
string(float)     ftos  = #2;

void(float n) main = {
    local float i;

    for (i = 0; i < n; i += 1) {
        print("for ", ftos(i), "\n");
    }

    i = 0;
    while (i < n) {
        print("while ", ftos(i), "\n");
        i += 1;
    }

    i = 0;
    do {
        print("do ", ftos(i), "\n");
        i += 1;
    } while (i < n);
};
