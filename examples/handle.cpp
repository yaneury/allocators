#include <iostream>

#include <dmt/handle/owned.hpp>
#include <dmt/handle/provider.hpp>

using namespace dmt::handle;

struct Point {
  int x;
  int y;
};

int main(int argc, const char* argv[]) {
  Provider provider;
  auto pt = MakeOwned<Point>(provider);
  std::cout << "X: " << pt->x << std::endl;
  std::cout << "Y: " << pt->y << std::endl;

  return 0;
}
