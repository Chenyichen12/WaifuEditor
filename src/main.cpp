#include <nlohmann/json.hpp>

#include "editor/app.h"


int main(int argc, char* argv[]) {
  editor::App app(argc, argv);
  app.Exec();

  return 0;
}