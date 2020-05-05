struct A { int x; };
struct B { int y; };
struct C { struct A a; struct B b; };

extern int bar(int*);
extern int baz(struct B*);

int foo(struct C* c)
{
  bar(&c->a.x);
  baz(&c->b);
  return c->a.x - c->b.y;
}