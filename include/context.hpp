#ifndef context_hpp
#define context_hpp

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

struct CraneCommandEntry;

struct CraneOpenFile {
public:
  std::string path;
  std::string alias;
  FILE *handle;

  CraneOpenFile(std::string filePath, std::string alias, FILE *handle)
    : path(filePath), alias(alias), handle(handle) {}
};

enum class CraneInterfaceMode {
  Normal,
  Edit,
  Template,
};

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

struct CraneContext {
public:
  int lastCommandResult;
  CraneOpenFile *openedFile;
  std::map<std::string, CraneOpenFile*> fileMap;
  std::map<std::string, void*> sharedHandleMap;
  std::map<std::string, CraneCommandEntry*> commandMap;
  CraneInterfaceMode interfaceMode;
  u8 *fileBuffer;
  size_t fileSize;

  CraneContext()
    : lastCommandResult(0),
      openedFile(nullptr),
      fileMap(),
      sharedHandleMap(),
      commandMap(),
      interfaceMode(CraneInterfaceMode::Normal),
      fileBuffer(nullptr),
      fileSize(0) {}
};

#endif

