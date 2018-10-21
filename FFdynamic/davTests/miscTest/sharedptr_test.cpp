#include <iostream>
#include <memory>

using namespace std;

int expectIntPtr(int *p){
    std::cout << *p << "\n";
    return 0;
}

int main () {
  std::shared_ptr<int> a,b,c,d,e;

  a = std::make_shared<int> (10);
  b = std::make_shared<int> (10);
  c = b;
  a.get()[0] = 255;
  std::cout << "comparisons:\n" << std::boolalpha;

  std::cout << "a == b: " << (a==b) << '\n';
  std::cout << "b == c: " << (b==c) << '\n';
  std::cout << "c == d: " << (c==d) << '\n';

  std::cout << "a != nullptr: " << (a!=nullptr) << '\n';
  std::cout << "b != nullptr: " << (b!=nullptr) << '\n';
  std::cout << "c != nullptr: " << (c!=nullptr) << '\n';
  std::cout << "d != nullptr: " << (d!=nullptr) << '\n';
  std::cout << "e != nullptr: " << (e==nullptr) << '\n';
  std::cout << "a addr  " << (&a) << '\n';
  std::cout << "e addr  " << (&e) << '\n';
  expectIntPtr(a.get());
  if (!e) cout << "can do " << "\n";
  return 0;
}
