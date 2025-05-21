#pragma once

#include "OnlineSubsystemTypes.h"

class FUserOnlineAccountAtto final : public FUserOnlineAccount
{
	TSharedRef<const FUniqueNetId> UserId;

public:
	explicit FUserOnlineAccountAtto(const TSharedRef<const FUniqueNetId>& UserId);

	virtual TSharedRef<const FUniqueNetId> GetUserId() const override;

	virtual FString GetRealName() const override;

	virtual FString GetDisplayName(const FString& Platform = FString()) const override;

	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;

	virtual FString GetAccessToken() const override;

	virtual bool GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const override;

	virtual bool SetUserAttribute(const FString& AttrName, const FString& AttrValue) override;
};
