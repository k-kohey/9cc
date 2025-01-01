#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./9cc "$input" > tmp.s
  cc -no-pie -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "✅️ $input => $actual"
  else
    echo "❌️ $input => $expected expected, but got $actual"
    echo "log:"
    cat log.txt
    exit 1
  fi
}

assert 0 '{ 0; }'
assert 42 '{ 42; }'
assert 21 '{ 5+20-4; }'
assert 41 '{ 12 + 34 - 5 ; }'
assert 47 '{ 5+6*7; }'
assert 15 '{ 5*(9-6); }'
assert 4 '{ (3+5)/2; }'
assert 10 '{ -10+20; }'
assert 10 '{ - -10; }'
assert 10 '{ - - +10; }'

assert 0 '{ 0==1; }'
assert 1 '{ 42==42; }'
assert 1 '{ 0!=1; }'
assert 0 '{ 42!=42; }'

assert 1 '{ 0<1; }'
assert 0 '{ 1<1; }'
assert 0 '{ 2<1; }'
assert 1 '{ 0<=1; }'
assert 1 '{ 1<=1; }'
assert 0 '{ 2<=1; }'

assert 1 '{ 1>0; }'
assert 0 '{ 1>1; }'
assert 0 '{ 1>2; }'
assert 1 '{ 1>=0; }'
assert 1 '{ 1>=1; }'
assert 0 '{ 1>=2; }'

assert 3 '{ a=1; a+2; }'
assert 8 '{ a=3; z=5; a+z; }'
assert 3 '{ hoge=1; hoge+2; }'
assert 2 '{ hoge=3; fuga=1; hoge-fuga; }'
assert 2 '{ return 2; }'
assert 3 '{ return a=3; }'
assert 4 '{ hoge=3; fuga=1; hoge+fuga; }'

assert 1 '{ return 1; 2; 3; }'
assert 2 '{ 1; return 2; 3; }'
assert 3 '{ 1; 2; return 3; }'
assert 3 '{ {1; {2;} return 3;} }'

assert 3 '{ if (0) return 2; return 3; }'
assert 2 '{ if (1) return 2; return 3; }'
assert 3 '{ if (1-1) return 2; return 3; }'
assert 2 '{ if (2-1) return 2; return 3; }'
assert 4 '{ if (0) { 1; 2; return 3; } else { return 4; } }'
assert 3 '{ if (1) { 1; 2; return 3; } else { return 4; } }'

assert 55 '{ i=0; j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }'
assert 3 '{ for (;;) {return 3;} return 5; }'

echo OK