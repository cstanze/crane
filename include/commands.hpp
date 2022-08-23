#ifndef commands_hpp
#define commands_hpp

#include "context.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

struct CraneCommand;

typedef int (*CraneCommandHandler)(CraneCommand *, CraneContext *);

enum class CraneArgumentType {
  Boolean,
  String,
  Number
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
// clangd doesn't like when this isn't here, it thinks it's unused
static const char *CraneArgumentTypeNames[] = {
  "Boolean",
  "File",
  "String",
  "Number"
};
#pragma clang diagnostic pop

struct CraneCommandArgument {
public:
  std::string name; 
  CraneArgumentType type;
  bool isOptional;

  CraneCommandArgument(std::string name, bool isOptional, CraneArgumentType type)
    : name(name),
      type(type),
      isOptional(isOptional) {}
};

struct CraneCommandEntry {
public:
  std::string name;
  std::string description;
  bool isVariadic;
  bool requiresOpenFile;
  bool shouldOverride;
  CraneCommandHandler handler;
  CraneCommandEntry *overridenEntry;
  std::vector<CraneCommandArgument*> arguments;

  CraneCommandEntry(std::string name, CraneCommandHandler handler, bool isVariadic)
    : name(name),
      isVariadic(isVariadic),
      handler(handler) {}
  
  inline void setCommandDescription(std::string desc) {
    this->description = desc;
  }

  inline void setRequiresOpenFile(bool requiresOpenFile = true) {
    this->requiresOpenFile = requiresOpenFile;
  }

  inline void addArgument(std::string name, bool isOptional, CraneArgumentType type) {
    auto arg = new CraneCommandArgument(name, isOptional, type);
    arguments.push_back(arg);
  }

  inline size_t argumentCount() {
    return arguments.size();
  }

  inline bool hasOptionalArgs() {
    for (auto &arg : arguments) {
      if (arg->isOptional) {
        return true;
      }
    }

    return false;
  }

  inline int adjustedArgumentCount() {
    // for each optional argument, we need to subtract one from the argument count
    int aac = argumentCount();
    for (auto &arg : arguments) {
      if (arg->isOptional) {
        aac--;
      }
    }

    return aac;
  }
};

#endif

