#include "UserOnlineAccountAtto.h"

FUserOnlineAccountAtto::FUserOnlineAccountAtto(const TSharedRef<const FUniqueNetId>& UserId)
    : UserId(UserId)
{
}

TSharedRef<const FUniqueNetId> FUserOnlineAccountAtto::GetUserId() const
{
	return UserId;
}

FString FUserOnlineAccountAtto::GetRealName() const
{
	return UserId->ToString();
}

FString FUserOnlineAccountAtto::GetDisplayName(const FString& Platform) const
{
	return UserId->ToString();
}

bool FUserOnlineAccountAtto::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	return false;
}

FString FUserOnlineAccountAtto::GetAccessToken() const
{
	return TEXT("");
}

bool FUserOnlineAccountAtto::GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	return false;
}

bool FUserOnlineAccountAtto::SetUserAttribute(const FString& AttrName, const FString& AttrValue)
{
	return false;
}
