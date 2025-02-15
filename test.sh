#!/bin/bash
cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
EOF

assert() {
  expected="$1"
  input="$2"

  ./quackcc "$input" > tmp.s || exit
  gcc -o tmp tmp.s tmp2.o
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 '{ 0; }'
assert 42 '{ 42; }'
assert 21 '{ 5+20-4; }'
assert 21 '{  5  + 20  - 4  ; }'
assert 85 '{ 5 + 20 * 4; }'
assert 10 '{ 5 + 20 / 4; }'
assert 2 '{ (1 + 2) * 3 /4; }'
assert 47 '{ 5+6*7; }'
assert 15 '{ 5*(9-6); }'
assert 4 '{( 3+5)/2; }'
assert 5 '{ 3 - (-2); }'
assert 10 '{ - 10+20; }'
assert 10 '{ - -10; }'
assert 10 '{-  - +10; }'

assert 1 '{ 1 == 1; }'
assert 1 '{ 42 == 42; }'
assert 0 '{ 1 == 2; }'
assert 1 '{ 12 != 34; }'
assert 0 '{ 5000 != 5000; }'
assert 1 '{ 34 != 56; }'
assert 0 '{ 1 < 1; }'
assert 1 '{ 1 < 2; }'
assert 1 '{ 1 <= 1; }'
assert 1 '{ 1 <= 2; }'
assert 1 '{ 2 > 1; }'
assert 0 '{ 2 > 2; }'
assert 1 '{ 2 >= 2; }'
assert 1 '{ 3 >= 2; }'
assert 1 '{ 1 == 1 == 1; }'
assert 0 '{ 0 == 1 == 1; }'
assert 1 '{ 0 == 1 != 1; }'

assert 1 '{ 3;2;1; }'
assert 3 '{ 1;2;3; }'

assert 3 '{ int a=3; a; }'
assert 0 '{ int x, y; x=y=10; x-y==0; }'
assert 1 '{ int x=5; x == 5; }'
assert 7 '{ int my_num = 7; my_num; }'
assert 8 '{ int foo123=3; int bar=5; foo123+bar; }'

assert 1 '{ return 1; }';
assert 4 '{ return 2 + 2; }';
assert 1 '{ return 1; 2; 3; }'
assert 2 '{ 1; return 2; 3; }'
assert 3 '{ 1; 2; return 3; }'

assert 3 '{ {1; {2;} return 3;} }'
assert 1 '{ {;;;;;;} 1; }'
assert 5 '{ ;;; return 5; }'

assert 3 '{ if (0) return 2; return 3; }'
assert 3 '{ if (1-1) return 2; return 3; }'
assert 2 '{ if (1) return 2; return 3; }'
assert 2 '{ if (2-1) return 2; return 3; }'
assert 4 '{ if (0) { 1; 2; return 3; } else { return 4; } }'
assert 3 '{ if (1) { 1; 2; return 3; } else { return 4; } }'
assert 5 '{ if (1) ; 5; }'

assert 5 '{int i; for (i = 1; i < 5; i = i + 1); return i; }'
assert 8 '{ int i = 2, j = 0; for (; i < 10; i = i + 1) j = j + 1; return j; }'
assert 55 '{ int i=0, j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }'
assert 3 '{ for (;;) {return 3;} return 5; }'

assert 0 '{ int i = 3; while (i) { i = i - 1; } return i; }'
assert 10 '{ int i=0; while(i<10) { i=i+1; } return i; }'

assert 3 '{ int x=3; return *&x; }'
assert 3 '{ int x=3; int *y=&x; int **z=&y; return **z; }'

assert 5 '{ int x=3, y=5; return *(&x+1); }'
assert 3 '{ int x=3, y=5; return *(&y-1); }'
assert 5 '{ int x=3, y=5; return *(&x-(-1)); }'
assert 5 '{ int x=3, *y=&x; *y=5; return x; }'
assert 7 '{ int x=3, y=5; *(&x+1)=7; return y; }'
assert 7 '{ int x=3, y=5; *(&y-2+1)=7; return x; }'
assert 5 '{ int x=3; return (&x+2)-&x+3; }'

assert 8 '{ int x, y; x=3; y=5; return x+y; }'
assert 8 '{ int x=3, y=5; return x+y; }'
assert 0 '{int; return 0;}'

assert 3 '{ return ret3(); }'
assert 5 '{ return ret5(); }'
assert 8 '{ return ret3() + ret5(); }'

echo OK
