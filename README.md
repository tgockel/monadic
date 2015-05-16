Monadic++
=========

Monadic++ is an experimental header-only C++ library designed to make working with C++ easier.
It is largely a result of random inspiration to solve some of the problems I face while writing C++.
There is somewhat okay [Doxygen documentation][doxygen].

 - `completion<T>`: An improved [`future<T>`][std_future]
 - `exceptional<T>`: A monadic exception object similar to [`expected`][std_expected]
 - `scope_exit<F>`: Execute arbitrary code at scope exit
 - `spin_mutex`: An implementation of a spin mutex

[![Build Status](https://travis-ci.org/tgockel/monadic.svg?branch=master)](https://travis-ci.org/tgockel/monadic)
[![Code Coverage](https://img.shields.io/coveralls/tgockel/monadic.svg)](https://coveralls.io/r/tgockel/monadic)
[![Github Issues](https://img.shields.io/github/issues/tgockel/monadic.svg)](http://github.com/tgockel/monadic/issues) 

 [doxygen]: http://tgockel.github.io/monadic/
 [std_future]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3558.pdf
 [std_expected]: http://www.hyc.io/boost/expected-proposal.pdf
