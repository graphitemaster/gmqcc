void(string, ...)   print  = #1;enum {
    // this behaviour is confusing, but I like that
    // we support it.
    __ = (__ - 1),
    A  = (__ + 1),

    B,
    C
};

enum {
    D = C + B,
    E = C + C,
    F = C + D,
};

enum {
    G = (B + F), H = (C + F),
    I = (D + F), J = (B + I)
};
enum {
    K = A + B - C + D - E + F *
        G - H + I - J + A - B -
        J + A,
    L,
    M,
    N
};

void main() {
    print(ftos(A), "\n");
    print(ftos(B), "\n");
    print(ftos(C), "\n");
    print(ftos(D), "\n");
    print(ftos(E), "\n");
    print(ftos(F), "\n");
    print(ftos(G), "\n");
    print(ftos(H), "\n");
    print(ftos(I), "\n");
    print(ftos(J), "\n");
    print(ftos(K), "\n");
    print(ftos(L), "\n");
    print(ftos(M), "\n");
    print(ftos(N), "\n");
};
