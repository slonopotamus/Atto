#pragma once

#include "Online/CoreOnline.h"

class FUniqueNetIdAtto final : public FUniqueNetId
{
	uint64 Value;

	FUniqueNetIdAtto()
	    : Value(0)
	{
	}

	explicit FUniqueNetIdAtto(const uint64 Value)
	    : Value(Value)
	{
	}

public:
	static TSharedRef<const FUniqueNetId> Create(uint64 Value);

	static TSharedPtr<const FUniqueNetId> Create(const FString& Str);

	static TSharedPtr<const FUniqueNetId> Create(const uint8* Bytes, int32 Size);

	static FUniqueNetIdAtto Invalid;

	virtual FName GetType() const override;

	virtual const uint8* GetBytes() const override;

	virtual int32 GetSize() const override;

	virtual bool IsValid() const override;

	virtual FString ToString() const override;

	virtual FString ToDebugString() const override;
};
