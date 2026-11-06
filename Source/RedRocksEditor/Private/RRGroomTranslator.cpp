#include "RRGroomTranslator.h"

#include "GroomSettings.h"
#include "HairAttributes.h"
#include "HairDescription.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	const char Magic[8] = {'R', 'R', 'G', 'R', 'O', 'O', 'M', '1'};

	struct FReader
	{
		const TArray<uint8>& Data;
		int64 Offset = 0;

		bool Read(void* Out, int64 Bytes)
		{
			if (Bytes < 0 || Offset + Bytes > Data.Num()) return false;
			FMemory::Memcpy(Out, Data.GetData() + Offset, Bytes);
			Offset += Bytes;
			return true;
		}

		template <typename T>
		bool ReadArray(TArray<T>& Out, int64 Count)
		{
			Out.SetNumUninitialized(Count);
			return Read(Out.GetData(), Count * sizeof(T));
		}
	};
}

bool FRRGroomTranslator::Translate(const FString& FilePath, FHairDescription& HairDescription, const FGroomConversionSettings& ConversionSettings)
{
	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *FilePath)) return false;
	FReader R{Data};

	char Header[8];
	uint32 NumStrands = 0, NumPoints = 0;
	if (!R.Read(Header, 8) || FMemory::Memcmp(Header, Magic, 8) != 0) return false;
	if (!R.Read(&NumStrands, 4) || !R.Read(&NumPoints, 4) || NumStrands == 0 || NumPoints == 0) return false;

	TArray<int32> Counts, Groups;
	TArray<float> Positions, Widths, Colors, RootUVs;
	if (!R.ReadArray(Counts, NumStrands) || !R.ReadArray(Positions, int64(NumPoints) * 3)
		|| !R.ReadArray(Widths, NumPoints) || !R.ReadArray(Colors, int64(NumPoints) * 3)
		|| !R.ReadArray(RootUVs, int64(NumStrands) * 2) || !R.ReadArray(Groups, NumStrands))
	{
		return false;
	}
	int64 Total = 0;
	for (int32 C : Counts)
	{
		if (C < 2) return false;
		Total += C;
	}
	if (Total != NumPoints) return false;

	TAttributesSet<FStrandID>& StrandAttrs = HairDescription.StrandAttributes();
	TAttributesSet<FVertexID>& VertexAttrs = HairDescription.VertexAttributes();
	if (!StrandAttrs.HasAttribute(HairAttribute::Strand::VertexCount)) StrandAttrs.RegisterAttribute<int>(HairAttribute::Strand::VertexCount);
	if (!StrandAttrs.HasAttribute(HairAttribute::Strand::RootUV)) StrandAttrs.RegisterAttribute<FVector2f>(HairAttribute::Strand::RootUV);
	if (!StrandAttrs.HasAttribute(HairAttribute::Strand::GroupID)) StrandAttrs.RegisterAttribute<int>(HairAttribute::Strand::GroupID);
	if (!VertexAttrs.HasAttribute(HairAttribute::Vertex::Position)) VertexAttrs.RegisterAttribute<FVector3f>(HairAttribute::Vertex::Position);
	if (!VertexAttrs.HasAttribute(HairAttribute::Vertex::Width)) VertexAttrs.RegisterAttribute<float>(HairAttribute::Vertex::Width);
	if (!VertexAttrs.HasAttribute(HairAttribute::Vertex::Color)) VertexAttrs.RegisterAttribute<FVector3f>(HairAttribute::Vertex::Color, 1, FVector3f::ZeroVector);

	TArray<FStrandID> StrandIDs;
	StrandIDs.Reserve(NumStrands);
	for (uint32 S = 0; S < NumStrands; ++S) StrandIDs.Add(HairDescription.AddStrand());
	TArray<FVertexID> VertexIDs;
	VertexIDs.Reserve(NumPoints);
	for (uint32 V = 0; V < NumPoints; ++V) VertexIDs.Add(HairDescription.AddVertex());

	TStrandAttributesRef<int> Count = StrandAttrs.GetAttributesRef<int>(HairAttribute::Strand::VertexCount);
	TStrandAttributesRef<FVector2f> RootUV = StrandAttrs.GetAttributesRef<FVector2f>(HairAttribute::Strand::RootUV);
	TStrandAttributesRef<int> Group = StrandAttrs.GetAttributesRef<int>(HairAttribute::Strand::GroupID);
	TVertexAttributesRef<FVector3f> Position = VertexAttrs.GetAttributesRef<FVector3f>(HairAttribute::Vertex::Position);
	TVertexAttributesRef<float> Width = VertexAttrs.GetAttributesRef<float>(HairAttribute::Vertex::Width);
	TVertexAttributesRef<FVector3f> Color = VertexAttrs.GetAttributesRef<FVector3f>(HairAttribute::Vertex::Color);
	if (!Count.IsValid() || !RootUV.IsValid() || !Group.IsValid() || !Position.IsValid() || !Width.IsValid() || !Color.IsValid()) return false;

	for (uint32 S = 0; S < NumStrands; ++S)
	{
		Count[StrandIDs[S]] = Counts[S];
		RootUV[StrandIDs[S]] = FVector2f(RootUVs[2 * S], RootUVs[2 * S + 1]);
		Group[StrandIDs[S]] = Groups[S];
	}
	for (uint32 V = 0; V < NumPoints; ++V)
	{
		Position[VertexIDs[V]] = FVector3f(Positions[3 * V], Positions[3 * V + 1], Positions[3 * V + 2]);
		Width[VertexIDs[V]] = Widths[V];
		Color[VertexIDs[V]] = FVector3f(Colors[3 * V], Colors[3 * V + 1], Colors[3 * V + 2]);
	}
	SetGroomAttribute(HairDescription, FGroomID(0), HairAttribute::Groom::MajorVersion, 1);
	SetGroomAttribute(HairDescription, FGroomID(0), HairAttribute::Groom::MinorVersion, 0);
	SetGroomAttribute(HairDescription, FGroomID(0), HairAttribute::Groom::Tool, FName(TEXT("redrocks.sasquatch")));
	return HairDescription.IsValid();
}

bool FRRGroomTranslator::CanTranslate(const FString& FilePath)
{
	if (!IsFileExtensionSupported(FPaths::GetExtension(FilePath))) return false;
	TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(*FilePath));
	if (!Ar || Ar->TotalSize() < 16) return false;
	char Header[8];
	Ar->Serialize(Header, 8);
	return FMemory::Memcmp(Header, Magic, 8) == 0;
}

bool FRRGroomTranslator::IsFileExtensionSupported(const FString& FileExtension) const
{
	return FileExtension.Equals(TEXT("rrgroom"), ESearchCase::IgnoreCase);
}

FString FRRGroomTranslator::GetSupportedFormat() const
{
	return TEXT("rrgroom;Red Rocks generated groom");
}
