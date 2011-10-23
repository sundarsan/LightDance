// -*- C++ -*-

#ifndef LIGHTINFO_H
#define LIGHTINFO_H

struct LightInfo {
public:
  enum LightKind {
    kLightKind_Pinspot,
    kLightKind_Strobe
  };

  enum LightColor {
    kLightColor_Red,
    kLightColor_Blue,
    kLightColor_Green,
    kLightColor_Yellow,
    kLightColor_White
  };

  LightKind Kind;
  LightColor Color;
  unsigned Index;

  static LightInfo Make(LightKind Kind, LightColor Color, unsigned Index) {
    LightInfo Result;
    Result.Kind = Kind;
    Result.Color = Color;
    Result.Index = Index;
    return Result;
  }
};

#endif
