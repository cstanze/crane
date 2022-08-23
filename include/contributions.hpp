#ifndef contributions_hpp
#define contributions_hpp

#include "commands.hpp"
#include "context.hpp"

#define _concat(x, y) x ## y

/* typedef int (*CraneCommandHandler)(CraneCommand*, CraneContext *context); */
#define contributableCommand(commandName)                                                \
  extern "C" int _concat(,commandName) (CraneCommand *command, CraneContext *context)

// TODO: do more with this...
struct CraneContributedCommands {
  std::vector<CraneCommandEntry *> contributedCommands;
};

typedef CraneContributedCommands *(*CraneContributionInitialiser)();
typedef int (*CraneContributionVersion)();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
// same as in "commands.hpp", -Wunused-function is annoying...
inline static CraneCommandEntry *
contributeCommand(CraneContributedCommands *contributions, const char *name,
                  CraneCommandHandler handler, bool isVariadic) {
  CraneCommandEntry *entry = new CraneCommandEntry(name, handler, isVariadic);
  contributions->contributedCommands.push_back(entry);

  return entry;
}
#pragma clang diagnostic pop

#endif
