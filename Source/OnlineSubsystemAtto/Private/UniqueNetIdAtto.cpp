#include "UniqueNetIdAtto.h"
#include "OnlineSubsystemAtto.h"

FUniqueNetIdAtto FUniqueNetIdAtto::Invalid;

TSharedRef<const FUniqueNetIdAtto> FUniqueNetIdAtto::Create(const uint64 Value)
{
	return MakeShareable(new FUniqueNetIdAtto(Value));
}

TSharedPtr<const FUniqueNetIdAtto> FUniqueNetIdAtto::Create(const FString& Str)
{
	const auto Value = FCString::Strtoui64(*Str, nullptr, 16);
	return MakeShareable(new FUniqueNetIdAtto(Value));
}

TSharedPtr<const FUniqueNetIdAtto> FUniqueNetIdAtto::Create(const uint8* Bytes, const int32 Size)
{
	if (Size != sizeof(decltype(Value)) || !ensure(Bytes))
	{
		return nullptr;
	}

	return Create(*reinterpret_cast<const uint64*>(Bytes));
}

FName FUniqueNetIdAtto::GetType() const
{
	return ATTO_SUBSYSTEM;
}

const uint8* FUniqueNetIdAtto::GetBytes() const
{
	return reinterpret_cast<const uint8*>(&Value);
}

int32 FUniqueNetIdAtto::GetSize() const
{
	return sizeof(decltype(Value));
}

bool FUniqueNetIdAtto::IsValid() const
{
	return Value != 0;
}

FString FUniqueNetIdAtto::ToString() const
{
	return FString::Printf(TEXT("%016llx"), Value);
}

FString FUniqueNetIdAtto::ToDebugString() const
{
	return ToString();
}
