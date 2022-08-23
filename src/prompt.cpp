#include "prompt.hpp"
#include "commands.hpp"
#include "context.hpp"
#include <cctype>
#include <readline/history.h>
#include <readline/readline.h>
#include <string>

std::string generatePrompt(CraneContext *context) {
  /**
   * The expected prompt should be clean and a bit colorful:
   *
   * {cyan}crane {magenta}(openFile)? {white if didNotFail else red}>{reset}
   */

  std::string out;

  // \33[2K\r
  out += "\33[2K\r";

  std::string interfaceColor;
  if (context->interfaceMode == CraneInterfaceMode::Edit) {
    interfaceColor = kColorYellow;
  } else if (context->interfaceMode == CraneInterfaceMode::Template) {
    interfaceColor = kColorWhite;
    interfaceColor += kColorBold;
  } else {
    interfaceColor = kColorCyan;
  }

  // {interfaceColor}crane
  out += interfaceColor + "crane ";

  // {magenta}{openFile}?
  if (context->openedFile != nullptr) {
    out += std::string(kColorReset) + std::string(kColorMagenta) + "(" + context->openedFile->alias + ") ";
  }

  out += context->lastCommandResult != 0 ? kColorRed : kColorWhite;
  out += "> " + std::string(kColorReset);

  return out;
}

CraneCommand *CraneCommand::parseCommand(CraneContext *context, std::string buffer) {
  CraneCommand *command = new CraneCommand("", {});
  std::string argumentBuffer = "";
  bool escapeNext = false;
  bool inString = false;
  bool hasCommandName = false;
  char lastQuote = '\0';
  char currentChar = '\0';
  int bufferLength = buffer.length();
  int lastQuoteIndex = 0;
  int i = 0;

  while (bufferLength) {
    currentChar = buffer[i++];
    bufferLength--;

    if (escapeNext) {
      argumentBuffer.push_back(currentChar);
      continue;
    }

    if (currentChar == '\\' && !escapeNext) {
      escapeNext = true;
      continue;
    }

    if (currentChar == '"' || currentChar == '\'') {
      if (!inString) {
        inString = true;
        lastQuoteIndex = i;
        lastQuote = currentChar;
        continue;
      } else if (currentChar == lastQuote) {
        inString = false;

        if (!hasCommandName) {
          command->name = argumentBuffer;
          hasCommandName = true;
        } else {
          command->arguments.push_back(
              new CraneArgument(argumentBuffer, CraneArgumentType::String));
        }
        argumentBuffer.clear();
      } else {
        argumentBuffer.push_back(currentChar);
      }
    }

    if (isspace(currentChar) && !inString) {
      if (!hasCommandName) {
        command->name = argumentBuffer;
        hasCommandName = true;
      } else {
        command->arguments.push_back(
            new CraneArgument(argumentBuffer, CraneArgumentType::String));
      }
      argumentBuffer.clear();

      continue;
    }

    argumentBuffer.push_back(currentChar);
  }

  if (inString) {
    printf("Expected closing quote (%c) to string at %d:\n", lastQuote,
           lastQuoteIndex);
    printf("%s\n", buffer.c_str());
    printf("%s^\n", std::string(lastQuoteIndex - 1, ' ').c_str());
    return nullptr;
  }

  if (argumentBuffer.length()) {
    if (!hasCommandName) {
      command->name = argumentBuffer;
      hasCommandName = true;
    } else {
      command->arguments.push_back(
          new CraneArgument(argumentBuffer, CraneArgumentType::String));
    }
    argumentBuffer.clear();
  }

  // classifications

  for (auto &arg : command->arguments) {
    if (arg->value == "true" || arg->value == "false") {
      arg->type = CraneArgumentType::Boolean;
      continue;
    } else if (arg->value.find(".") != std::string::npos) {
      // check if it's a number
      bool hasDot = false;
      arg->type = CraneArgumentType::Number;
      for (auto &c : arg->value) {
        if (!isdigit(c) && c != '.') {
          arg->type = CraneArgumentType::String;
          break;
        }

        if (c == '.' && !hasDot) {
          hasDot = true;
        } else if (c == '.' && hasDot) {
          printf("Argument '%s' is not a number, it can only have one '.' to "
                 "denote decimals.\n",
                 arg->value.c_str());
          arg->type = CraneArgumentType::String;
          break;
        }
      }
    } else {
      arg->type = CraneArgumentType::Number;
      for (auto &c : arg->value) {
        if (!isdigit(c)) {
          arg->type = CraneArgumentType::String;
          break;
        }
      }

      if (arg->type == CraneArgumentType::String) {
        if (context->fileMap.find(arg->value) != context->fileMap.end()) {
          arg->type = CraneArgumentType::File;
        }
      }
    }
  }

  return command;
}

CraneCommand *CraneCommand::fromUser(CraneContext *context) {
  std::string prompt = generatePrompt(context);
  char *commandBuffer = readline(prompt.c_str());

  if (!commandBuffer) {
    return nullptr;
  }

  // NOTE: theres a weird bug where the prompt is
  //       reprinted when navigating through the history
  //       ex:
  // prompt | action   | prompt
  // -------+----------+-------
  //      1 | (normal) | "> foo"
  //      2 | (up)     | "> bar"
  //      3 | (down)   | "> bar        foo"
  //
  //                           ^^^^^^^^ there is a bunch of space
  //                                 here, not sure why...
  //
  // As you can see, the prompt is reprinted. I'm not sure why.
  // But this is an annoying bug, so I'm disabling history for now.

  // if (commandBuffer && *commandBuffer)
  //   add_history(commandBuffer);

  return CraneCommand::parseCommand(context, std::string(commandBuffer));
}
