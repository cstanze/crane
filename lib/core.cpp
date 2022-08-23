#include "commands.hpp"
#include "config.hpp"
#include "context.hpp"
#include "contributions.hpp"
#include "prompt.hpp"
#include <_ctype.h>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <readline/readline.h>
#include <sys/stat.h>

contributableCommand(open) {
  if (context->interfaceMode == CraneInterfaceMode::Edit) {
    printf("Cannot open a file in edit mode\n");
    return 1;
  }

  std::string filePath = command->arguments[0]->value;
  std::string fileAlias = command->arguments[1]->value;

  // check if the file exists
  struct stat fileStat;
  if (stat(filePath.c_str(), &fileStat) != 0) {
    printf("File '%s' does not exist\n", filePath.c_str());
    return 1;
  }

  // check if the file is a directory
  if (S_ISDIR(fileStat.st_mode)) {
    printf("File '%s' is a directory\n", filePath.c_str());
    return 1;
  }

  // check if the file is already open
  if (context->fileMap.find(fileAlias) != context->fileMap.end()) {
    printf("File '%s' is already open\n", fileAlias.c_str());
    return 0;
  }

  // open the file
  FILE *file = fopen(filePath.c_str(), "rb");
  if (!file) {
    printf("Failed to open file '%s'\n", filePath.c_str());
    return 1;
  }

  // add the file to the file map
  context->fileMap[fileAlias] = new CraneOpenFile(filePath, fileAlias, file);

  if (context->openedFile == nullptr) {
    context->openedFile = context->fileMap[fileAlias];
  } else {
    printf("A file is already selected, would you like to unselect it? (y/n) ");
    char *answer = readline("");
    if (answer && (answer[0] == 'y' || answer[0] == 'Y')) {
      context->openedFile = context->fileMap[fileAlias];
    }

    free(answer);

    if (context->openedFile != context->fileMap[fileAlias]) {
      printf("To select this file later, use 'select %s'\n", fileAlias.c_str());
    }
  }

  printf("Successfully opened '%s' as '%s'\n", filePath.c_str(), fileAlias.c_str());

  return 0;
}

contributableCommand(select) {
  if (context->interfaceMode == CraneInterfaceMode::Edit) {
    printf("Cannot select a file in edit mode\n");
    return 1;
  }

  if (command->arguments.size() == 0) {
    printf("Unselecting file '%s' (%s)\n", context->openedFile->alias.c_str(),
           context->openedFile->path.c_str());
    context->openedFile = nullptr;
    return 0;
  }

  std::string fileAlias = command->arguments[0]->value;

  // check if the file is already open
  if (context->fileMap.find(fileAlias) == context->fileMap.end()) {
    printf("File '%s' is not open\n", fileAlias.c_str());
    return 1;
  }

  // check if the file is already selected
  if (context->openedFile == context->fileMap[fileAlias]) {
    printf("File '%s' is already selected\n", fileAlias.c_str());
    return 0;
  }

  // unselect the current file
  if (context->openedFile != nullptr) {
    printf("Unselecting file '%s' (%s)\n", context->openedFile->alias.c_str(),
           context->openedFile->path.c_str());
  }

  // select the new file
  context->openedFile = context->fileMap[fileAlias];
  printf("Selected file '%s' (%s)\n", context->openedFile->alias.c_str(),
         context->openedFile->path.c_str());

  return 0;
}

contributableCommand(close) {
  if (context->interfaceMode == CraneInterfaceMode::Edit) {
    printf("Cannot close a file while in edit mode\n");
    return 1;
  }

  if (command->arguments.size() == 0) {
    if (context->openedFile == nullptr) {
      printf("No file is selected\n");
      return 1;
    }

    printf("Closing file '%s'\n", context->openedFile->alias.c_str());
    delete context->fileMap[context->openedFile->alias];
    context->fileMap.erase(context->openedFile->alias);
    context->openedFile = nullptr;
    return 0;
  }

  std::string fileAlias = command->arguments[0]->value;

  if (fileAlias == "all") {
    for (auto it = context->fileMap.begin(); it != context->fileMap.end(); it++) {
      printf("Closing file '%s'\n", it->first.c_str());
      delete it->second;
    }

    context->fileMap.clear();
    context->openedFile = nullptr;
    return 0;
  }

  // check if the file is already open
  if (context->fileMap.find(fileAlias) == context->fileMap.end()) {
    printf("File '%s' is not open\n", fileAlias.c_str());
    return 1;
  }

  // check if the file is already selected
  if (context->openedFile == context->fileMap[fileAlias]) {
    printf("File '%s' is currently selected, unselect it first or use 'close' with no "
           "arguments to close this file\n",
           fileAlias.c_str());
    return 1;
  }

  // close the file
  printf("Closing file '%s'\n", fileAlias.c_str());
  fclose(context->fileMap[fileAlias]->handle);
  delete context->fileMap[fileAlias];
  context->fileMap.erase(fileAlias);

  return 0;
}

contributableCommand(files) {
  printf("All Open Files:\n");
  for (auto &file : context->fileMap) {
    printf("  %s (%s)", file.second->alias.c_str(), file.second->path.c_str());
    if (context->openedFile == file.second) {
      printf(" - selected");
    }

    if (context->interfaceMode == CraneInterfaceMode::Edit &&
        context->openedFile == file.second) {
      printf(" - editing (%zu bytes)", context->fileSize);
    }

    printf("\n");
  }

  return 0;
}

#define kHexDumpWidth 16

contributableCommand(dump) {
  printf("Dumping file '%s' (%s) as %s:\n\n", context->openedFile->alias.c_str(),
         context->openedFile->path.c_str(), command->arguments[0]->value.c_str());

  if (context->interfaceMode != CraneInterfaceMode::Edit) {
    // read file size
    fseek(context->openedFile->handle, 0, SEEK_END);
    context->fileSize = ftell(context->openedFile->handle);
    fseek(context->openedFile->handle, 0, SEEK_SET);

    // read the file
    context->fileBuffer = new u8[context->fileSize];
    fread(context->fileBuffer, 1, context->fileSize, context->openedFile->handle);
  }

  // format and print the file
  std::string format = command->arguments[0]->value;

  if (format == "hex") {
    // a hex dump looks like this:
    /*
     *
     * 4C 6F 72 65 6D 20 49 73 70 75            Lorem Ipsu
     * 6D 20 44 6F 6C 6F 72 20 53 69            m Dolor Si
     * 74 20 41 6D 65 74 .. .. .. ..            t Amet....
     *                   |                            ^^^^ placeholder characters for
     *                   |                                 non-printable chars
     *                   ^^ this means that there
     *                      is a gap of 4 bytes
     *
     */

    std::vector<std::pair<u8 *, size_t>> chunks;

    // split the file into chunks of kHexDumpWidth bytes,
    // anything less then kHexDumpWidth bytes will have a size associated with it
    for (size_t i = 0; i < context->fileSize; i += kHexDumpWidth) {
      u8 *ptr = context->fileBuffer + i;
      // if the chunk is less then kHexDumpWidth bytes,
      // then the size is the remaining bytes
      size_t size =
          (i + kHexDumpWidth > context->fileSize) ? context->fileSize - i : kHexDumpWidth;
      chunks.push_back(std::make_pair(ptr, size));
    }

    // print the chunks
    for (auto &chunk : chunks) {
      // print the address
      printf("%08X: ", (u32)(chunk.first - context->fileBuffer));

      for (size_t i = 0; i < chunk.second; i++) {
        printf("%02X ", chunk.first[i]);
      }

      // print the placeholder characters if the chunk is less then kHexDumpWidth bytes
      for (size_t i = chunk.second; i < kHexDumpWidth; i++) {
        printf(".. ");
      }

      // print the characters
      printf("      ");
      for (size_t i = 0; i < chunk.second; i++) {
        if (isprint(chunk.first[i])) {
          printf("%c", chunk.first[i]);
        } else {
          printf(".");
        }
      }

      // placeholder characters for filling the gap
      for (size_t i = chunk.second; i < kHexDumpWidth; i++) {
        printf(".");
      }

      printf("\n");
    }
  } else {
    printf("Unknown format '%s'\n", format.c_str());

    if (context->interfaceMode != CraneInterfaceMode::Edit) {
      delete[] context->fileBuffer;
      context->fileBuffer = nullptr;
      context->fileSize = 0;
    }

    return 1;
  }

  // clean up
  if (context->interfaceMode != CraneInterfaceMode::Edit) {
    delete[] context->fileBuffer;
    context->fileBuffer = nullptr;
    context->fileSize = 0;
  }

  return 0;
}

contributableCommand(mode) {
  if (command->arguments.size() == 0) {
    context->interfaceMode = CraneInterfaceMode::Normal;
    return 0;
  }

  std::string mode = command->arguments[0]->value;
  CraneInterfaceMode oldMode = context->interfaceMode;

  if (mode == "normal") {
    context->interfaceMode = CraneInterfaceMode::Normal;
  } else if (mode == "edit") {
    context->interfaceMode = CraneInterfaceMode::Edit;
  } else if (mode == "template") {
    context->interfaceMode = CraneInterfaceMode::Template;
  } else {
    printf("Unknown mode '%s'\n", mode.c_str());
    return 1;
  }

  if (context->interfaceMode == oldMode) {
    printf("Mode is already '%s'\n", mode.c_str());
    return 1;
  }

  // initialise the current mode if needed
  if (context->interfaceMode == CraneInterfaceMode::Edit) {
    if (context->openedFile == nullptr) {
      printf("No file is selected, refusing to enter edit mode\n");
      context->interfaceMode = oldMode;
      return 1;
    }

    // reopen the file in read/write binary mode
    fclose(context->openedFile->handle);
    context->openedFile->handle = fopen(context->openedFile->path.c_str(), "rb+");
    if (context->openedFile->handle == nullptr) {
      printf("Failed to open file '%s' for editing\n", context->openedFile->path.c_str());
      context->interfaceMode = oldMode;
      return 1;
    }

    if (context->fileBuffer != nullptr) {
      delete[] context->fileBuffer;
      context->fileBuffer = nullptr;
      context->fileSize = 0;
    }

    // read file size
    fseek(context->openedFile->handle, 0, SEEK_END);
    context->fileSize = ftell(context->openedFile->handle);
    fseek(context->openedFile->handle, 0, SEEK_SET);

    // read the file
    context->fileBuffer = new u8[context->fileSize];
    fread(context->fileBuffer, 1, context->fileSize, context->openedFile->handle);
  } else {
    if (context->fileBuffer != nullptr) {
      delete[] context->fileBuffer;
      context->fileBuffer = nullptr;
      context->fileSize = 0;
    }

    // close the file and reopen it in read-only binary mode
    fclose(context->openedFile->handle);
    context->openedFile->handle = fopen(context->openedFile->path.c_str(), "rb");
  }

  printf("Mode changed to '%s'\n", mode.c_str());

  return 0;
}

contributableCommand(save) {
  if (context->interfaceMode != CraneInterfaceMode::Edit) {
    printf("Not in edit mode\n");
    return 1;
  }

  printf("Saving file '%s' (%s)\n", context->openedFile->alias.c_str(),
         context->openedFile->path.c_str());

  // overwrite the entire file
  fseek(context->openedFile->handle, 0, SEEK_SET);

  if (fwrite(context->fileBuffer, 1, context->fileSize, context->openedFile->handle) !=
      context->fileSize) {
    printf("Failed to write file\n");
    perror("fwrite");
    return 1;
  }

  return 0;
}

bool isHex(std::string &s) {
  for (char c : s) {
    if (!isxdigit(c)) {
      return false;
    }
  }

  return true;
}

contributableCommand(byteAt) {
  std::string addrString = command->arguments[0]->value;
  size_t addr = strtoul(addrString.c_str(), nullptr, 0);

  if (context->interfaceMode == CraneInterfaceMode::Edit &&
      command->arguments.size() > 1) {
    std::string valueString = command->arguments[1]->value;
    std::string format = "hex";

    // check if the value is a valid hex number
    if (!isHex(valueString)) {
      format = valueString;
      valueString = command->arguments[2]->value;
    }

    if (format == "hex") {
      if (!isHex(valueString)) {
        printf("Invalid hex value '%s'\n", valueString.c_str());
        return 1;
      }

      if (valueString.size() > 2) {
        printf("Hex value '%s' is too long (max FF)\n", valueString.c_str());
        return 1;
      }

      u8 value = strtoul(valueString.c_str(), nullptr, 16);

      if (addr >= context->fileSize) {
        printf("Address out of bounds\n");
        return 1;
      }

      context->fileBuffer[addr] = value;
    } else if (format == "decimal") {
      if (!isdigit(valueString[0])) {
        printf("Invalid decimal value '%s'\n", valueString.c_str());
        return 1;
      }

      if (valueString.size() > 3) {
        printf("Decimal value '%s' is too long\n", valueString.c_str());
        return 1;
      }

      u8 value = strtoul(valueString.c_str(), nullptr, 10);

      if (value > 255) {
        printf("Decimal value '%s' is too large (max 255)\n", valueString.c_str());
        return 1;
      }

      if (addr >= context->fileSize) {
        printf("Address out of bounds\n");
        return 1;
      }

      context->fileBuffer[addr] = value;
    } else if (format == "ascii") {
      if (valueString.size() != 1) {
        printf("Invalid ASCII value '%s'\n", valueString.c_str());
        return 1;
      }

      if (addr >= context->fileSize) {
        printf("Address out of bounds\n");
        return 1;
      }

      context->fileBuffer[addr] = valueString[0];
    } else {
      printf("Unknown format '%s'\n", format.c_str());
      return 1;
    }

    printf("Value at 0x%zX set to %s\n", addr, valueString.c_str());

    // update the hex view
    dump(new CraneCommand("dump", {new CraneArgument("hex")}), context);

    return 0;
  }

  if (command->arguments.size() > 1) {
    printf("%swarn%s: only 1 argument required when not in edit mode, did you mean to "
           "switch to edit mode? (W0002)",
           kColorYellow, kColorReset);
  }

  if (context->interfaceMode != CraneInterfaceMode::Edit) {
    // read the file size
    fseek(context->openedFile->handle, 0, SEEK_END);
    context->fileSize = ftell(context->openedFile->handle);
    fseek(context->openedFile->handle, 0, SEEK_SET);

    // read the file
    context->fileBuffer = new u8[context->fileSize];
    fread(context->fileBuffer, 1, context->fileSize, context->openedFile->handle);
  }

  if (addr >= context->fileSize) {
    printf("Address out of bounds\n");
    return 1;
  }

  u8 value = context->fileBuffer[addr];

  printf("%02X (%d)", value, value);

  if (isprint(value)) {
    printf(" '%c'", value);
  }

  printf("\n");

  if (context->interfaceMode != CraneInterfaceMode::Edit) {
    // free the file buffer
    delete[] context->fileBuffer;
    context->fileBuffer = nullptr;
    context->fileSize = 0;
  }

  return 0;
}

contributableCommand(insert) {
  if (context->interfaceMode != CraneInterfaceMode::Edit) {
    printf("Not in edit mode\n");
    return 1;
  }

  std::string addrString = command->arguments[0]->value;
  size_t addr = strtoul(addrString.c_str(), nullptr, 0);

  if (addr >= context->fileSize) {
    printf("Address out of bounds\n");
    return 1;
  }

  std::string valueString = command->arguments[1]->value;
  size_t length = valueString.size();

  size_t newLength = context->fileSize + length;
  u8 *newBuffer = new u8[newLength];

  // copy the old data before the address
  memcpy(newBuffer, context->fileBuffer, addr);

  // copy the new data
  memcpy(newBuffer + addr, valueString.c_str(), length);

  // copy the old data after the address, if any
  if (addr + length < context->fileSize) {
    memcpy(newBuffer + addr + length, context->fileBuffer + addr + length,
           context->fileSize - addr - length);
  }

  // free the old buffer
  delete[] context->fileBuffer;
  context->fileBuffer = newBuffer;
  context->fileSize = newLength;

  // update the hex view
  dump(new CraneCommand("dump", {new CraneArgument("hex")}), context);

  return 0;
}

contributableCommand(write) {
  if (context->interfaceMode != CraneInterfaceMode::Edit) {
    printf("Not in edit mode\n");
    return 1;
  }

  std::string addrString = command->arguments[0]->value;
  size_t addr = strtoul(addrString.c_str(), nullptr, 0);

  std::string valueString = command->arguments[1]->value;
  size_t length = valueString.size();

  size_t newLength = context->fileSize;
  if (addr + length > newLength) {
    newLength = addr + length;
  }

  u8 *newBuffer = new u8[newLength];

  // copy the old data before the address
  memcpy(newBuffer, context->fileBuffer, addr);

  if (addr > context->fileSize) {
    // fill the new data with zeros
    memset(newBuffer + context->fileSize, 0, addr - context->fileSize);
  }

  // copy the new data
  memcpy(newBuffer + addr, valueString.c_str(), length);

  // copy the old data after the address, if any
  if (addr + length < context->fileSize) {
    memcpy(newBuffer + addr + length, context->fileBuffer + addr + length,
           context->fileSize - addr - length);
  }

  delete[] context->fileBuffer;
  context->fileBuffer = newBuffer;
  context->fileSize = newLength;

  // update the hex view
  dump(new CraneCommand("dump", {new CraneArgument("hex")}), context);

  return 0;
}

contributableCommand(writeHex) {
  if (context->interfaceMode != CraneInterfaceMode::Edit) {
    printf("Not in edit mode\n");
    return 1;
  }

  std::string addrString = command->arguments[0]->value;

  std::vector<std::string> values;
  for (size_t i = 1; i < command->arguments.size(); i++) {
    values.push_back(command->arguments[i]->value);
  }

  std::string valueString = "";

  for (auto &value : values) {
    if (value.size() != 2) {
      printf("Invalid hex value '%s'\n", value.c_str());
      return 1;
    }

    u8 valueByte = strtoul(value.c_str(), nullptr, 16);
    valueString += valueByte;
  }

  return write(new CraneCommand("write", {new CraneArgument(addrString),
                                          new CraneArgument(valueString)}),
               context);
}

contributableCommand(truncate) {
  if (context->interfaceMode != CraneInterfaceMode::Edit) {
    printf("Not in edit mode\n");
    return 1;
  }

  std::string addrString = command->arguments[0]->value;
  size_t addr = strtoul(addrString.c_str(), nullptr, 0);

  if (addr >= context->fileSize) {
    printf("Address out of bounds\n");
    return 1;
  }

  size_t truncSize = context->fileSize - addr;
  size_t oldSize = context->fileSize;
  size_t newLength = addr;

  u8 *newBuffer = new u8[newLength];

  // copy the old data before the address
  memcpy(newBuffer, context->fileBuffer, addr);

  // free the old buffer
  delete[] context->fileBuffer;
  context->fileBuffer = newBuffer;
  context->fileSize = newLength;

  printf("Truncated %zu bytes from %zu bytes (now %zu bytes)\n", truncSize, oldSize, newLength);

  // update the hex view
  dump(new CraneCommand("dump", {new CraneArgument("hex")}), context);

  return 0;
}

contributableCommand(templateNew) {
  if (context->interfaceMode != CraneInterfaceMode::Template) {
    printf("Not in template mode\n");
    return 1;
  }

  std::string name = command->arguments[0]->value;

  // if (context->templateMap.find(name) != context->templateMap.end()) {
  //   printf("Template '%s' already exists\n", name.c_str());
  //   return 1;
  // }
}

extern "C" CraneContributedCommands *crane_init() {
  CraneContributedCommands *contrib = new CraneContributedCommands();

  auto openEntry = contributeCommand(contrib, "open", open, false);
  openEntry->addArgument("path", false, CraneArgumentType::String);
  openEntry->addArgument("alias", false, CraneArgumentType::String);
  openEntry->setCommandDescription("Opens a new file with a given path and alias");

  auto selEntry = contributeCommand(contrib, "select", select, false);
  selEntry->addArgument("alias", true, CraneArgumentType::String);
  selEntry->setCommandDescription("Selects a file with a given alias");

  auto closeEntry = contributeCommand(contrib, "close", close, false);
  closeEntry->addArgument("alias", true, CraneArgumentType::String);
  closeEntry->setCommandDescription(
      "Closes a file with a given alias or the currently selected file");

  auto filesEntry = contributeCommand(contrib, "files", files, false);
  filesEntry->setCommandDescription("Lists all open files");

  auto dumpEntry = contributeCommand(contrib, "dump", dump, false);
  dumpEntry->addArgument("format", true, CraneArgumentType::String);
  dumpEntry->setCommandDescription("Dumps the currently selected file");
  dumpEntry->setRequiresOpenFile();

  auto modeEntry = contributeCommand(contrib, "mode", mode, false);
  modeEntry->addArgument("mode", true, CraneArgumentType::String);
  modeEntry->setCommandDescription("Changes the file editing mode");

  // Edit mode commands

  auto saveEntry = contributeCommand(contrib, "save", save, false);
  saveEntry->setCommandDescription("Saves the currently selected file");

  auto byteAtEntry = contributeCommand(contrib, "byteat", byteAt, true);
  byteAtEntry->addArgument("offset", false, CraneArgumentType::Number);
  byteAtEntry->setCommandDescription("Prints the byte at a given offset");
  byteAtEntry->setRequiresOpenFile();

  auto insertEntry = contributeCommand(contrib, "insert", insert, true);
  insertEntry->addArgument("offset", false, CraneArgumentType::Number);
  insertEntry->addArgument("value", false, CraneArgumentType::String);
  insertEntry->setCommandDescription("Inserts a string at a given offset");
  insertEntry->setRequiresOpenFile();

  auto writeEntry = contributeCommand(contrib, "write", write, true);
  writeEntry->addArgument("offset", false, CraneArgumentType::Number);
  writeEntry->addArgument("value", false, CraneArgumentType::String);
  writeEntry->setCommandDescription("Writes a string at a given offset");
  writeEntry->setRequiresOpenFile();

  auto writeHexEntry = contributeCommand(contrib, "writehex", writeHex, true);
  writeHexEntry->addArgument("offset", false, CraneArgumentType::Number);
  writeHexEntry->setCommandDescription("Writes a list of hex bytes at a given offset");

  auto truncateEntry = contributeCommand(contrib, "truncate", truncate, true);
  truncateEntry->addArgument("offset", false, CraneArgumentType::Number);
  truncateEntry->setCommandDescription("Truncates the file at a given offset, removing all data after it");

  // Template mode commands

  auto templateEntry = contributeCommand(contrib, "newtemplate", templateNew, false);
  templateEntry->addArgument("name", false, CraneArgumentType::String);
  templateEntry->setCommandDescription("Creates a new template with a given name");

  return contrib;
}

extern "C" int crane_version() { return kCraneVersion; }
