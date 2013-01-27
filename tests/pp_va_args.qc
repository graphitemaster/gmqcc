void print(...) = #1;

#define NOPARENS(...) __VA_ARGS__
#define callem(func, args) func NOPARENS(args)

#define callen(func, ...) func __VA_ARGS__##[0]

void main() {
    print(NOPARENS("hello ", "world\n"));
    callem(print, ("Yay", ", there\n"));
    callen(print, ("Woah",", there\n"));
}
