const string a = "hello world"; // 11
float b[] = { 1, 2, 3 };        // 3
float c[5] = { 5, 4, 3, 2, 1 }; // 5
const float d[] = { 1 };        // 1

void main() {
    print(ftos(length a), "\n"); // 11
    print(ftos(length b), "\n"); // 3
    print(ftos(length c), "\n"); // 5
    print(ftos(length d), "\n"); // 1

    static float al = length(a);
    static float bl = length(b);
    static float cl = length(c);
    static float dl = length(d);

    print(ftos(al), "\n"); // 11
    print(ftos(bl), "\n"); // 3
    print(ftos(cl), "\n"); // 5
    print(ftos(dl), "\n"); // 1

    print(ftos(length "hello world"), "\n"); // 11
}
