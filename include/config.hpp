#ifndef config_hpp
#define config_hpp

//    100000 * major +
//      1000 * minor +
//         1 * patch
#define kCraneVersion 000001
#define kCraneVersionString "0.0.1"

#define kCraneVersionMatches(major, minor, patch) \
  (major * 100000 + minor * 1000 + patch) == kCraneVersion

#define kCraneVersionIsGreaterThan(major, minor, patch) \
  (major * 100000 + minor * 1000 + patch) > kCraneVersion

#endif
