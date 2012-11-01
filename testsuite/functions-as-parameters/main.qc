void(string, string) print = #1;

string() getter = {
    return "correct";
};

void(string() f) printer = {
    print(f(), "\n");
};

void() main = {
    printer(getter);
};
