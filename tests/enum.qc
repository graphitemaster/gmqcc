void(string, ...)   print  = #1;
string(float)       ftos   = #2;

enum {
    FOO,
    BAR,
    BAZ,
    OMG = BAR,
    POO,
    LAST = OMG + POO
};

enum {A};
enum {B=A+1};
enum {C=A};

void main() {
    print(ftos(FOO), "\n");
    print(ftos(BAR), "\n");
    print(ftos(BAZ), "\n");
    print(ftos(OMG), "\n");
    print(ftos(POO), "\n");
    print(ftos(LAST), "\n");
    print(ftos(C), "\n");
}
