// Reads .rrgroom, the strand file `redrocks sasquatch` writes, into a hair
// description, so the normal groom import (HairStrandsFactory) can build a
// Groom asset from it. The format, little-endian:
//   char[8]  "RRGROOM1"
//   uint32   strand count S, point count P
//   int32    points per strand [S]
//   float32  positions [P][3]   centimetres, Unreal mesh space (Z up)
//   float32  widths    [P]      centimetres
//   float32  colours   [P][3]   linear RGB
//   float32  root UVs  [S][2]
//   int32    group ids [S]
// Positions are written in the space the skeletal mesh imports into, so the
// import's conversion settings are ignored.
#pragma once

#include "HairStrandsTranslator.h"

class FRRGroomTranslator : public IGroomTranslator
{
public:
	virtual bool Translate(const FString& FilePath, FHairDescription& OutHairDescription, const FGroomConversionSettings& ConversionSettings) override;
	virtual bool CanTranslate(const FString& FilePath) override;
	virtual bool IsFileExtensionSupported(const FString& FileExtension) const override;
	virtual FString GetSupportedFormat() const override;
};
