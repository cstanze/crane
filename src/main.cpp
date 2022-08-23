/**
 * To quickly outline the Plugin API for Crane:
 *
 * You need to have a dynamic library compiled with the following functions:
 *
 * CraneContributedCommands *crane_init();
 * int crane_version();
 *
 * Use the `CraneContributedCommands` type to contribute commands to Crane.
 * Loading the library is provided by Crane (AKA Crane Core):
 *
 * load </path/to/contributedLibrary>
 */

#include "commands.hpp"
#include "config.hpp"
#include "context.hpp"
#include "contributions.hpp"
#include "prompt.hpp"
#include <cstring>
#include <dlfcn.h>
#include <readline/readline.h>
#include <termios.h>
#include <unistd.h>
#include <unordered_map>

#if kUsingCraneDarwin
static char *kCraneCoreLocation = (char *)"extern/core.dylib";
static char *kCraneStagingLocation = (char *)"extern/staging.dylib";
#else
static char *kCraneCoreLocation = (char *)"extern/core.so";
static char *kCraneStagingLocation = (char *)"extern/staging.so";
#endif

int Crane_load(CraneCommand *command, CraneContext *context);
int Crane_QMark(CraneCommand *command, CraneContext *context);
int Crane_help(CraneCommand *command, CraneContext *context);
int Crane_explain(CraneCommand *command, CraneContext *context);

static CraneContext *context;

char *commandCompletionEngine(const char *text, int state) {
  // static int index;
  // static std::vector<CraneCommandEntry *> commands;
  // CraneCommandEntry *command;

  // if (state == 0) {
  //   index = 0;
  //   commands.clear();
  //   for (auto &entry : context->commandMap) {
  //     commands.push_back(entry.second);
  //   }
  // }

  // if (strlen(text) == 0) {
  //   return nullptr;
  // }

  // while ((command = commands[index++])) {
  //   if (strncmp(command->name.c_str(), text, strlen(text)) == 0) {
  //     return strdup(command->name.c_str());
  //   }
  // }

  // TODO: Implement this
  return nullptr;
}

char *argumentCompletionEngine(const char *text, int state) {
  // static char *opts[] = {(char *)"true", (char *)"false"};
  // static int index = 0;
  // char *opt;

  // if (!state)
  //   index = 0;

  // while ((opt = opts[index++])) {
  //   if (strncmp(opt, text, strlen(text)) == 0)
  //     return strdup(opt);
  // }

  // TODO: Implement this
  return nullptr;
}

char **completionGenerator(const char *text, int start, int end) {
  rl_attempted_completion_over = 1;

  CraneCommand *command = CraneCommand::parseCommand(context, text);
  if (!command) {
    free(command);
    return nullptr;
  }

  if (command->arguments.size() == 0) {
    return rl_completion_matches(text, commandCompletionEngine);
  } else {
    CraneCommandEntry *possibleCommand = context->commandMap[command->name];
    if (!possibleCommand) {
      return nullptr;
    }

    if (possibleCommand->argumentCount() <= command->arguments.size()) {
      return nullptr;
    }

    if (possibleCommand->arguments[command->arguments.size()]->type ==
        CraneArgumentType::String) {
      rl_attempted_completion_over = 0; // Re-enable file completion
    } else if (possibleCommand->arguments[command->arguments.size()]->type ==
               CraneArgumentType::Boolean) {
      return rl_completion_matches(text, argumentCompletionEngine);
    }

    return nullptr;
  }
}

void cleanup() {
  // Cleanup Steps
  if (context->openedFile != NULL)
    fclose(context->openedFile->handle);
  for (auto &fileEntry : context->fileMap) {
    fclose(fileEntry.second->handle);
  }
  for (auto &sharedEntry : context->sharedHandleMap) {
    dlclose(sharedEntry.second);
  }

  // NOTE: This caused a segfault (but not during program run but after which is
  // odd).
  // deleteMap(context->dynHandleMap);
  // free(context->commandMap);
  // free(context);
}

void printHelp() {
  printf("Crane v1.0.0 - (c) Jules Amalie\n");
  printf("\n");
  printf("OPTIONS\n");
  printf("    --no-core                      Disregards loading the Core "
         "module\n");
  printf("    --core </path/to/staging>      Sets the location of the Core "
         "module\n");
  printf("    --staging [/path/to/staging]   Sets the location of the Staging\n"
         "                                   module and/or loads the staging "
         "module\n");
}

int main(int argc, char **argv) {
  rl_attempted_completion_function = completionGenerator;
  atexit(cleanup);

  bool noCore = false;
  bool coreOverride = false;
  bool stagingOverride = false;
  context = new CraneContext();

  CraneCommandEntry *loadCommand = new CraneCommandEntry("load", Crane_load, false);
  loadCommand->setCommandDescription("Loads a module from a dynamic library");
  loadCommand->addArgument("module", false, CraneArgumentType::String);
  context->commandMap.emplace("load", loadCommand);

  context->commandMap.emplace("?", new CraneCommandEntry("?", Crane_QMark, false));
  context->commandMap["?"]->setCommandDescription(
      "Prints the result of the last command");

  CraneCommandEntry *helpCommand = new CraneCommandEntry("help", Crane_help, true);
  helpCommand->setCommandDescription("Prints help information for commands");
  helpCommand->addArgument("command", true, CraneArgumentType::String);
  context->commandMap.emplace("help", helpCommand);

  CraneCommandEntry *explainCommand =
      new CraneCommandEntry("explain", Crane_explain, false);
  explainCommand->setCommandDescription("Prints the description of an error");
  explainCommand->addArgument("error", false,
                              CraneArgumentType::String); // E0001, E0002, etc
  context->commandMap.emplace("explain", explainCommand);

  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--no-core") {
      noCore = true;
    } else if (std::string(argv[i]) == "--staging") {
      if (stagingOverride) {
        printf("Error: --staging can only be specified once\n");
        return 1;
      }

      if (i + 1 < argc) {
        kCraneStagingLocation = argv[i + 1];
        i++;
      } else {
        printf("Error: --staging requires a path\n");
        return 1;
      }

      int result = loadCommand->handler(
          new CraneCommand("load", {new CraneArgument(kCraneStagingLocation)}), context);
      if (result != 0) {
        printf("%serr%s: Staging library not found in '%s'\n", kColorRed, kColorReset,
               kCraneStagingLocation);
        return 1;
      }
      stagingOverride = true;
    } else if (std::string(argv[i]) == "--core") {
      if (noCore) {
        printf("--core and --no-core cannot be used together\n");
      } else if (coreOverride) {
        printf("Duplicate --core argument found\n");
      }

      if (i + 1 < argc) {
        kCraneCoreLocation = argv[i + 1];
        i++;
      } else {
        printf("Error: --core requires a path\n");
        return 1;
      }

      int result = loadCommand->handler(
          new CraneCommand("load", {new CraneArgument(kCraneCoreLocation)}), context);
      if (result != 0) {
        printf("%serr%s: Core library not found in '%s'\n", kColorRed, kColorReset,
               kCraneCoreLocation);
        return 1;
      }
      coreOverride = true; // avoid loading the core module again
    } else if (std::string(argv[i]) == "--help") {
      printHelp();
      return 0;
    } else {
      printf("%serr%s: Unknown option '%s'\n", kColorRed, kColorReset, argv[i]);
      return 1;
    }
  }

  if (!noCore && !coreOverride) {
    loadCommand->handler(new CraneCommand("load", {new CraneArgument("Core")}), context);
  }

  while (true) {
    CraneCommand *command = CraneCommand::fromUser(context);

    if (command == nullptr) {
      continue;
    }

    if (command->name == "exit") {
      break;
    }

    auto cmdRes = context->commandMap.find(command->name);
    if (cmdRes == context->commandMap.end()) {
      context->lastCommandResult = -1;
      printf("%serr%s: Command '%s' not found (E0001)\n", kColorRed, kColorReset,
             command->name.c_str());
      continue;
    }

    CraneCommandEntry *cmd = cmdRes->second;

    if (cmd->isVariadic) {
      if (command->arguments.size() < cmd->adjustedArgumentCount()) {
        context->lastCommandResult = -1;
        printf("%serr%s: Too few arguments for command '%s' (E0002)\n", kColorRed,
               kColorReset, command->name.c_str());
        continue;
      }
    } else {
      if (cmd->hasOptionalArgs()) {
        if (command->arguments.size() < cmd->adjustedArgumentCount()) {
          context->lastCommandResult = -1;
          printf("%serr%s: Too few arguments for command '%s' (E0002)\n", kColorRed,
                 kColorReset, command->name.c_str());
          continue;
        } else if (command->arguments.size() > cmd->argumentCount()) {
          context->lastCommandResult = -1;
          printf("%serr%s: Too many arguments for command '%s' (E0002)\n", kColorRed,
                 kColorReset, command->name.c_str());
          continue;
        }
      } else {
        if (command->arguments.size() != cmd->argumentCount()) {
          context->lastCommandResult = -1;
          printf("%serr%s: Invalid number of arguments for command '%s' (E0002)\n",
                 kColorRed, kColorReset, command->name.c_str());
          continue;
        }
      }
    }

    if (cmd->requiresOpenFile && !context->openedFile) {
      context->lastCommandResult = -1;
      printf("%serr%s: No file open (E0006)\n", kColorRed, kColorReset);
      continue;
    }

    printf("\n");
    int res = cmd->handler(command, context);
    context->lastCommandResult = res;

    if (res != 0) {
      // TODO: add error messages
      // printf("%serr%s: some error occurred with: '%s'\n", kColorRed,
      // kColorReset, command->name.c_str());
    }

    printf("\n");
  }

  return 0;
}

int Crane_load(CraneCommand *command, CraneContext *context) {
  std::string fileToLoad = command->arguments[0]->value;

  if (fileToLoad == "Core") {
    fileToLoad = kCraneCoreLocation;
  } else if (fileToLoad == "Staging") {
    fileToLoad = kCraneStagingLocation;
  }

  if (access(fileToLoad.c_str(), R_OK) != 0) {
    printf("%serr%s: The module '%s' doesn't exist. (E0003)\n", kColorRed, kColorReset,
           fileToLoad.c_str());
    return 1;
  }

  if (context->sharedHandleMap.find(fileToLoad) != context->sharedHandleMap.end()) {
    printf("%swarn%s: The module '%s' has already been loaded! (W0001)\n", kColorYellow,
           kColorReset, command->arguments[0]->value.c_str());

    // Despite this being an "error", the file was loaded
    // successfully so there is no need to emit an error.
    return 0;
  }

  void *handle = dlopen(fileToLoad.c_str(), RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    printf("%serr%s: Failed to open the module. (E0003)\n%s\n", kColorRed, kColorReset,
           dlerror());
    dlclose(handle);
    return 1;
  }

  context->sharedHandleMap.emplace(fileToLoad, handle);

  CraneContributionInitialiser init =
      (CraneContributionInitialiser)dlsym(handle, "crane_init");
  CraneContributionVersion version =
      (CraneContributionVersion)dlsym(handle, "crane_version");

  if (!init) {
    printf("%serr%s: Failed to find the contribution initialiser. (E0004)\n%s\n",
           kColorRed, kColorReset, dlerror());
    dlclose(handle);
    return 1;
  }

  if (!version) {
    printf("%serr%s: Failed to find the contribution version. (E0004)\n%s\n", kColorRed,
           kColorReset, dlerror());
    dlclose(handle);
    return 1;
  }

  if (version() != kCraneVersion) {
    printf("%serr%s: The module '%s' is incompatible with this version of "
           "Crane. (E0005)\n",
           kColorRed, kColorReset, fileToLoad.c_str());
    dlclose(handle);
    return 1;
  }

  CraneContributedCommands *contrib = init();
  for (int i = 0; i < contrib->contributedCommands.size(); i++) {
    if (contrib->contributedCommands[i] == NULL)
      continue;

    context->commandMap[contrib->contributedCommands[i]->name] =
        contrib->contributedCommands[i];
  }

  return 0;
}

int Crane_QMark(CraneCommand *command, CraneContext *context) {
  printf("%d - %s%s%s\n", context->lastCommandResult,
         context->lastCommandResult == 0 ? kColorGreen : kColorRed,
         context->lastCommandResult == 0 ? "Successful" : "Failed", kColorReset);

  return 0;
}

int Crane_help(CraneCommand *command, CraneContext *context) {
  if (command->arguments.size() == 0) {
    printf("All available commands:\n");
    for (auto &cmd : context->commandMap) {
      printf("  %s%s%s -- %s\n", kColorBlue, cmd.second->name.c_str(), kColorReset,
             cmd.second->description.c_str());
    }
    return 0;
  }

  auto cmdRes = context->commandMap.find(command->arguments[0]->value);
  if (cmdRes == context->commandMap.end()) {
    printf("%serr%s: Command '%s' not found\n", kColorRed, kColorReset,
           command->arguments[0]->value.c_str());
    return 1;
  }

  CraneCommandEntry *cmd = cmdRes->second;

  printf("%sName: %s%s\n\n", kColorBold, kColorReset, cmd->name.c_str());
  printf("%sDescription: %s\n%s\n\n", kColorBold, kColorReset, cmd->description.c_str());

  printf("%sArguments:%s\n", kColorBold, kColorReset);
  for (auto &arg : cmd->arguments) {
    printf("  %s%s%s -- %s%s\n", kColorBold, arg->name.c_str(), kColorReset,
           CraneArgumentTypeNames[static_cast<int>(arg->type)],
           arg->isOptional ? " (optional)" : "");
  }

  return 0;
}

static std::unordered_map<std::string, std::string> errDescMap = {
    // Errors
    {"E0001", "The given command does not exist. This is likely due to a module that"
              " wasn't loaded"},
    {"E0002", "The given command has an invalid number of arguments.\n"
              "If the command is optional, you may omit arguments but\nyou must meet "
              "the minimum number of arguments and not exceed the maximum\nnumber of "
              "arguments. If the command is variadic, you must provide\nat least the "
              "minimum number of arguments"},
    {"E0003", "The given module doesn't exist"},
    {"E0004", "A required function does exist in the module but it failed to load"},
    {"E0005", "The module is incompatible with the current version of Crane.\n"
              "Currently, the version of Crane is: {version}"},
    {"E0006", "The command used expected an open file but there wasn't one selected.\n"
              "Use 'select <file>' to select a file and 'files' to see a list of "
              "files"},

    // Warnings
    {"W0001", "The given module has already been loaded."},
    {"W0002", "A certain command could been executed in multiple modes "
              "but the usage per mode was mismatched.\n This doesn't "
              "trigger an error but it is recommended that you fix accordingly."},
};

int Crane_explain(CraneCommand *command, CraneContext *context) {
  std::string errName = command->arguments[0]->value;

  auto errRes = errDescMap.find(errName);
  if (errRes == errDescMap.end()) {
    printf("%serr%s: Error '%s' not found\n", kColorRed, kColorReset, errName.c_str());
    return 1;
  }

  printf("%s%s%s%s\n", kColorBold, errName.find("E") == 0 ? kColorRed : kColorYellow,
         errName.c_str(), kColorReset);

  std::string replaced = errRes->second;
  std::string::size_type pos = replaced.find("{version}");
  if (pos != std::string::npos) {
    replaced.replace(pos, 9, kCraneVersionString);
  }
  printf("\n%s%s%s\n", kColorBold, kColorReset, replaced.c_str());

  return 0;
}
