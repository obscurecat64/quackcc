#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./quackcc "$input" > tmp.s || exit
  gcc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 0
assert 42 42
assert 21 '5+20-4'
assert 21 ' 5  + 20  - 4  '
assert 85 '5 + 20 * 4'
assert 10 '5 + 20 / 4'
assert 2 '(1 + 2) * 3 /4'
assert 47 '5+6*7'
assert 15 '5*(9-6)'
assert 4 '(3+5)/2'
assert 5 '3 - (-2)'
assert 10 '-10+20'
assert 10 '- -10'
assert 10 '- - +10'

assert 1 '1 == 1'
assert 1 '42 == 42'
assert 0 '1 == 2'
assert 1 '12 != 34'
assert 0 '5000 != 5000'
assert 1 '34 != 56'
assert 0 '1 < 1'
assert 1 '1 < 2'
assert 1 '1 <= 1'
assert 1 '1 <= 2'
assert 1 '2 > 1'
assert 0 '2 > 2'
assert 1 '2 >= 2'
assert 1 '3 >= 2'
assert 1 '1 == 1 == 1'
assert 0 '0 == 1 == 1'
assert 1 '0 == 1 != 1'

echo OK
