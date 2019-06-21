#ifndef RAMPUDDLE_PUDDLE_CORO_HPP
#define RAMPUDDLE_PUDDLE_CORO_HPP
/**
 * \internal
 * \file 
 * Very limited coroutine implementation.
 *
 * Kind of inspired by https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
 * 
 * Essentially, DECL_CORO_STATE holds an integer which is a line number of the 
 * function., RESET_CORO resets that value to 0. CORO_YIELD sets the current
 * line number so that the next time the function is called, a switch is used
 * to jump to the next line.
 *
 * Generally this means that this is not a truly general coroutine mechanic
 * since it does not remember any stack state between function invocations.
 * Any stack state has to be maintained outside the function. To some extent
 * this is automatically checked by the compiler due to the use of switch 
 * statements. Generally extra braces between CORO_YIELDs might be necessary
 * to get the compiler to accept the code.
 *
 * Furthermore, parallel invocations, or multiple simultaneous coroutine
 * invocations of the function is not allowed since there is a single global
 * variable which maintains the line number. However, wrapping the
 * DECL_CORO_STATE and the function in a struct/class will allow for it.
 *
 * Example:
 * \code
 *  DECL_CORO_STATE(integers)
 *  int ctr;
 *
 *  int integers() {
 *    // anything before CORO_BEGIN is run *everytime*
 *    CORO_BEGIN(integers)
 *    ctr = 0;
 *    while(1) {
 *      CORO_YIELD(ctr);
 *      ++ctr;
 *    };
 *    CORO_END
 *  }
 *
 *  int main() {
 *    while(1) {
 *      std::cout << CALL_CORO(integers) << "\n";
 *      getchar();
 *    }
 *  }
 * \endcode
 *
 * \note This is not recommended for use in general. 
 * It takes a little care to use right.
 */

#define DECL_CORO_STATE(f) int _coro_state ## f = 0;

#define RESET_CORO(f) _coro_state ## f = 0; 
#define RESET_CLASS_CORO(obj, f) obj._coro_state ## f = 0; 
#define CALL_CORO(f, ...) f(__VA_ARGS__)
#define CALL_CORO_RESET(f, ...) {_coro_state ## f = 0; f(__VA_ARGS__);}
#define CORO_BEGIN(f) int& _coro_state = _coro_state ## f; switch(_coro_state) { case 0:
#define CORO_YIELD(x) _coro_state=__LINE__; return x; \
 case __LINE__:
#define CORO_END _coro_state = 0; break; }
#define CORO_DONE(f) (_coro_state ##f == 0)
#define CORO_RUNNING(f) (_coro_state ##f != 0)
#define CLASS_CALL_CORO(obj, f, ...) (obj).f(__VA_ARGS__)
#define CLASS_CORO_DONE(obj, f) ((obj)._coro_state ##f == 0)
#define CLASS_CORO_RUNNING(obj, f) ((obj)._coro_state ##f != 0)

#endif
