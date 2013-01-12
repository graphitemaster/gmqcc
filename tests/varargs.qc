void(string...)   print  = #1;
string(float)     ftos   = #2;

void nbva(float a, string...count) {
    print("You gave me ", ftos(count), " additional parameters\n");
    print("First: ", ...(0, string), "\n");
    print("You chose: ", ...(a, string), "\n");
    for (a = 0; a < count; ++a)
        print("Vararg ", ftos(a), " = ", ...(a, string), "\n");
}

void main() {
    nbva(1, "Hello", "You", "There");
}
