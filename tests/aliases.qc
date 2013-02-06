float alias_1 = 3.14;
void  alias_2() {
    print("alias_2\n");
}

[[alias("alias_2")]] void  alias_2_aliased();
[[alias("alias_1")]] float alias_1_aliased;

// expected output
// alias_2
// 3.14
void main() {
    alias_2_aliased();

    print(
        ftos(
            alias_1_aliased
        ),
        "\n"
    );
}
