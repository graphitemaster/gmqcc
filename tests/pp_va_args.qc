void print(...) = #1;

#define NOPARENS(...) __VA_ARGS__
#define callem(func, args) func(NOPARENS args)

void main() {
    print(NOPARENS("hello ", "world\n"));
    callem(print, ("Yay", ", there\n"));
}
