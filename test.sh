#!/bin/bash
cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
int add(int x, int y) { return x+y; }
int sub(int x, int y) { return x-y; }
int add6(int a, int b, int c, int d, int e, int f) {
  return a+b+c+d+e+f;
}
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

assert 0 'int main() { return 0; }'
assert 42 'int main() { return 42; }'
assert 21 'int main() { return 5+20-4; }'
assert 21 'int main() { return 5  + 20  - 4  ; }'
assert 85 'int main() { return 5 + 20 * 4; }'
assert 10 'int main() { return 5 + 20 / 4; }'
assert 2 'int main() { return (1 + 2) * 3 /4; }'
assert 47 'int main() { return 5+6*7; }'
assert 15 'int main() { return 5*(9-6); }'
assert 4 'int main() {return ( 3+5)/2; }'
assert 5 'int main() { return 3 - (-2); }'
assert 10 'int main() { return - 10+20; }'
assert 10 'int main() { return - -10; }'
assert 10 'int main() {return -  - +10; }'

assert 1 'int main() { return 1 == 1; }'
assert 1 'int main() { return 42 == 42; }'
assert 0 'int main() { return 1 == 2; }'
assert 1 'int main() { return 12 != 34; }'
assert 0 'int main() { return 5000 != 5000; }'
assert 1 'int main() { return 34 != 56; }'
assert 0 'int main() { return 1 < 1; }'
assert 1 'int main() { return 1 < 2; }'
assert 1 'int main() { return 1 <= 1; }'
assert 1 'int main() { return 1 <= 2; }'
assert 1 'int main() { return 2 > 1; }'
assert 0 'int main() { return 2 > 2; }'
assert 1 'int main() { return 2 >= 2; }'
assert 1 'int main() { return 3 >= 2; }'
assert 1 'int main() { return 1 == 1 == 1; }'
assert 0 'int main() { return 0 == 1 == 1; }'
assert 1 'int main() { return 0 == 1 != 1; }'

assert 1 'int main() { 3;2;1; }'
assert 3 'int main() { 1;2;3; }'

assert 3 'int main() { int a=3; return a; }'
assert 0 'int main() { int x, y; x=y=10; return x-y==0; }'
assert 1 'int main() { int x=5; return x == 5; }'
assert 7 'int main() { int my_num = 7; return my_num; }'
assert 8 'int main() { int foo123=3; int bar=5; return foo123+bar; }'

assert 1 'int main() { return 1; }';
assert 4 'int main() { return 2 + 2; }';
assert 1 'int main() { return 1; 2; 3; }'
assert 2 'int main() { 1; return 2; 3; }'
assert 3 'int main() { 1; 2; return 3; }'

assert 3 'int main() { {1; {2;} return 3;} }'
assert 1 'int main() { {;;;;;;} return 1; }'
assert 5 'int main() { ;;; return 5; }'

assert 3 'int main() { if (0) return 2; return 3; }'
assert 3 'int main() { if (1-1) return 2; return 3; }'
assert 2 'int main() { if (1) return 2; return 3; }'
assert 2 'int main() { if (2-1) return 2; return 3; }'
assert 4 'int main() { if (0) { 1; 2; return 3; } else { return 4; } }'
assert 3 'int main() { if (1) { 1; 2; return 3; } else { return 4; } }'
assert 5 'int main() { if (1) ; 5; }'

assert 5 'int main() {int i; for (i = 1; i < 5; i = i + 1); return i; }'
assert 8 'int main() { int i = 2, j = 0; for (; i < 10; i = i + 1) j = j + 1; return j; }'
assert 55 'int main() { int i=0, j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }'
assert 3 'int main() { for (;;) {return 3;} return 5; }'

assert 0 'int main() { int i = 3; while (i) { i = i - 1; } return i; }'
assert 10 'int main() { int i=0; while(i<10) { i=i+1; } return i; }'

assert 3 'int main() { int x=3; return *&x; }'
assert 3 'int main() { int x=3; int *y=&x; int **z=&y; return **z; }'

assert 5 'int main() { int x=3, y=5; return *(&x+1); }'
assert 3 'int main() { int x=3, y=5; return *(&y-1); }'
assert 5 'int main() { int x=3, y=5; return *(&x-(-1)); }'
assert 5 'int main() { int x=3, *y=&x; *y=5; return x; }'
assert 7 'int main() { int x=3, y=5; *(&x+1)=7; return y; }'
assert 7 'int main() { int x=3, y=5; *(&y-2+1)=7; return x; }'
assert 5 'int main() { int x=3; return (&x+2)-&x+3; }'

assert 8 'int main() { int x, y; x=3; y=5; return x+y; }'
assert 8 'int main() { int x=3, y=5; return x+y; }'
assert 0 'int main() {int; return 0;}'

assert 3 'int main() { return ret3(); }'
assert 5 'int main() { return ret5(); }'
assert 8 'int main() { return ret3() + ret5(); }'

assert 8 'int main() { return add(3, 5); }'
assert 2 'int main() { return sub(5, 3); }'
assert 21 'int main() { return add6(1,2,3,4,5,6); }'
assert 66 'int main() { return add6(1,2,add6(3,4,5,6,7,8),9,10,11); }'
assert 136 'int main() { return add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16); }'
assert 2 'int main() { int x = 1; int *y = &x; return add(x, *y); }'
assert 12 'int main() { return add(4 + 3, 5); }'

assert 2 'int ret2() { return 1 + 1; } int main() { return ret2(); }'

echo OK
