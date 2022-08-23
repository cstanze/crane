#ifndef prompt_hpp
#define prompt_hpp

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "commands.hpp"

#define kColorRed "\033[31m"
#define kColorGreen "\033[32m"
#define kColorYellow "\033[33m"
#define kColorBlue "\033[34m"
#define kColorMagenta "\033[35m"
#define kColorCyan "\033[36m"
#define kColorWhite "\033[37m"
#define kColorReset "\033[0m"
#define kColorBold "\033[1m"

struct CraneContext;

struct CraneArgument {
  std::string value;
  CraneArgumentType type;

  CraneArgument(std::string value, CraneArgumentType type)
    : value(value),
      type(type) {}
  
  CraneArgument(std::string value)
    : value(value),
      type(CraneArgumentType::String) {}
};

struct CraneCommand {
public:
  std::string name;
  std::vector<CraneArgument*> arguments;
  CraneCommand(std::string name, std::vector<CraneArgument*> arguments)
    : name(name), arguments(arguments) {}

  static CraneCommand *fromUser(CraneContext *context);
  static CraneCommand *parseCommand(CraneContext *context, std::string buffer);
};

#endif

