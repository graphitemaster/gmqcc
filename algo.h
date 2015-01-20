#ifndef GMQCC_ALGO_HDR
#define GMQCC_ALGO_HDR

namespace algo {

template<typename ITER>
void shiftback(ITER element, ITER end) {
    //typename ITER::value_type backup(move(*element)); // hold the element
    typename std::remove_reference<decltype(*element)>::type backup(move(*element)); // hold the element
    ITER p = element++;
    for (; element != end; p = element++)
        *p = move(*element);
    *p = move(backup);
}

} // ::algo

#endif
