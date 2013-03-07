float pow(float x, float y) {
    return __builtin_pow(x, y);
}

void main() {
    float hundy = pow(10, 2); // 10^2 == 100
    print(ftos(hundy), "\n");      // prints: 100

    hundy -= 90; // 100-90 = 10
    print(ftos(hundy ** 2), "\n"); // prints: 100
}
