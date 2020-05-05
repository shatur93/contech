
struct A { int x; };
struct B { int y; };
struct C { struct A a; struct B b; };

int hello()
{
  struct C c;
  foo(&c);
  return c.a.x - c.b.y;
}