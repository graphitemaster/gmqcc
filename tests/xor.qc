vector swap(float x, float y) {
    vector ret = '0 0 0';
    // everyone knows this trick
    ret.x = x;
    ret.y = y;
    
    ret.x = ret.x ^ ret.y;
    ret.y = ret.y ^ ret.x;
    ret.x = ret.x ^ ret.y;
    
    return ret;
}

void main() {
    float x = 5;
    float y = 3;
    float z = x ^ y; // 6
    
    float a = 2;
    float b = 10;
    float c = a ^ b; // 8
    
    print(ftos(z), "\n");
    print(ftos(c), "\n");
    
    // commutative?
    if (x ^ y == y ^ x)
        print("commutative\n");
    
    // assocative?
    if (x ^ (y ^ z) == (x ^ y) ^ z)
        print("assocative\n");
        
    // elements are their own inverse?
    if (x ^ 0 == x)
        print("inverse\n");
        
    // vector ^ vector
    // vector ^ float
    // are legal in constant expressions (currently)
    const vector v3 = '5 2 5' ^ '3 10 3';
    const vector v4 = '5 2 5' ^ 10;
    
    print("vv: ", vtos(v3), "\n");
    print("vf: ", vtos(v4), "\n");
    
    // good olde xor swap test too
    float swap_x = 100;
    float swap_y = 200;
    vector swaps = swap(swap_x, swap_y);
    print("100:200 swapped is: ", ftos(swaps.x), ":", ftos(swaps.y), "\n");
}
