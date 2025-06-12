#pragma once

#include "Online/CoreOnline.h"

class FUniqueNetIdAtto final : public FUniqueNetId
{
	FUniqueNetIdAtto()
	    : Value(0)
	{
	}

	explicit FUniqueNetIdAtto(const uint64 Value)
	    : Value(Value)
	{
	}

public:
	static TSharedRef<const FUniqueNetIdAtto> Create(uint64 Value);

	static TSharedPtr<const FUniqueNetIdAtto> Create(const FString& Str);

	static TSharedPtr<const FUniqueNetIdAtto> Create(const uint8* Bytes, int32 Size);

	static FUniqueNetIdAtto Invalid;

	virtual FName GetType() const override;

	virtual const uint8* GetBytes() const override;

	virtual int32 GetSize() const override;

	virtual bool IsValid() const override;

	virtual FString ToString() const override;

	virtual FString ToDebugString() const override;

	const uint64 Value;
};
