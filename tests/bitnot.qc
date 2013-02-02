void main() {
    float a; a = 1;
    float b; b = 1;
    float c; c = 1;
    float d; d = 1;

    a &~= 1; // 0
    b &= ~1; // 0
    c &= ~d; // 0

    print("a: ", ftos(a), "\nb: ",
                 ftos(b), "\nc: ",
                 ftos(c), "\n");
}
