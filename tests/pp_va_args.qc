void print(...) = #1;

// method 0
#define METHOD__(...) __VA_ARGS__
#define METHOD_0(F,A) F METHOD__(A)

// method 1
#define METHOD_1(F,A) F(METHOD__ A)

// method 2
#define METHOD_2(F,...) F __VA_ARGS__##[0]

// method 3
#define METHOD_3(F,...) F __VA_ARGS__

void main() {
    METHOD_0(print, ("Method", " <zero>\n"));
    METHOD_1(print, ("Method", " <one>\n"));
    METHOD_2(print, ("Method", " <two>\n"));
    METHOD_3(print, ("Method", " <three>\n"));
}
