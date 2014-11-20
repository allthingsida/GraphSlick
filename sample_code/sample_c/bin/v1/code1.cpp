#include "stdafx.h"
using std::string;

//--------------------------------------------------------------------------
int simple_loop1(int a)
{
  int sigma=0;
  for (int i=0;i<a;i++)
    sigma += i;

  return sigma;
}

//--------------------------------------------------------------------------
int simple_loop2(int a)
{
  int fact=0;
  for (int i=1;i<=a;i++)
    fact *= i;

  return fact;
}

//--------------------------------------------------------------------------
int __forceinline my_alloc(int a)
{
  void *p = (void *)a;
  if (a == 1)
  {
    p = malloc(a + 1024);
  }
  else if (a == 2)
  {
    p = realloc(p, a + 2048);
  }
  else
  {
    unsigned char *pc = (unsigned char *)p;
    for (int i=0;i<1024;i++)
    {
      pc[i] = 0xff;  
      pc[i] += 1;
    }
  }

  srand((unsigned int)time(NULL));
  unsigned char *pc = (unsigned char *)p;
  int k = 0;
  for (int i=0;i<1024;i++)
  {
    pc[i] += rand() % 0xeb;
    k += pc[i];
  }
  return k;
}

//--------------------------------------------------------------------------
int __forceinline f2(int a, int b)
{
  if (a == 1)
  {
    a *= b + 0x11223344;
  }
  else if (a == 2)
  {
    int c = (a + b) / 0xc00c;

    for (int k = 0;k<c*123;k++)
    {
      a = a ^ (int)DecodePointer((LPVOID)k);
    }
  }
  else if (a == 3)
  {
    a += my_alloc(a * b);
  }
  return a - 2;
}

//--------------------------------------------------------------------------
int __forceinline f1(int a)
{
  int L = 0;
  for (int i=0;i<a;i++)
  {
    PVOID x = DecodePointer((PVOID)i);

    x = DecodePointer(x);
    x = EncodePointer((PVOID)i);

    a = ((int)x + i) * 0x11223344;

    a /= 0x123;

    if (a % 2)
    {
      printf("odd!\n");
      a += 0xdeadbeef;
    }
    else
    {
      for (int j=0;j<a*1234;j++)
      {
        L += f2(j, i);
      }
    }
  }
  return L;
}

//--------------------------------------------------------------------------
int doit(int a)
{
  simple_loop1(a * 10);
  simple_loop2(a * 1981);

  f1(a);
  a *= 0x11223344;
  my_alloc(a ^ 123);
  a = (int) (((__int64)a * 0xaabbccddeeff1122) / 0x1122344);

  f2(1, a + 1);
  my_alloc(a ^ 1232);
  f1(a * 2);
  f2(a ^ 1232, f1(a * 123));

  f1(a);
  a *= 0x11223344;
  my_alloc(a ^ 123);
  a = (int) (((__int64)a * 0xaabbccddeeff1122) / 0x1122344);

  f2(1, a + 1);
  my_alloc(a ^ 1232);
  f1(a * 2);
  f2(a ^ 1232, f1(a * 123));

  f1(a);
  a *= 0x11223344;
  my_alloc(a ^ 123);
  a = (int) (((__int64)a * 0xaabbccddeeff1122) / 0x1122344);

  f2(1, a + 1);
  my_alloc(a ^ 1232);
  f1(a * 2);
  f2(a ^ 1232, f1(a * 123));

  f2(1, a + 3);
  my_alloc(a ^ 1232);
  f1(a * 2);
  f2(a ^ 1222, f1(a * 123));


  f2(1, a + 1);
  my_alloc(a ^ 12312);
  f1(a * 2);
  f2(a ^ 2, f1(a * 123));


  f1(a);
  a *= 0x112244;
  my_alloc(a ^ 123);
  a = (int) (((__int64)a * 0xaabbddeeff1122) / 0x112112344);

  f2(1, a + 3);
  my_alloc(a ^ 1232);
  f1(a * 2);
  f2(a ^ 1232, f1(a * 123));

  return f1(a / 2);
}

//--------------------------------------------------------------------------
int main(int argc)
{
  return doit(argc);
}