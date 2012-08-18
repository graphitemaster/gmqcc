void(string) print  = #1;

void()       correct;
void(string) incorrect;

void() correct = {
    print("Hello\n");
}

void() incorrect = {
    printf("The compiler should error about this function having a wrong type\n");
}
