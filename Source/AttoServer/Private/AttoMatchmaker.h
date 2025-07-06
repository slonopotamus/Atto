#pragma once

#include "AttoProtocol.h"

struct FAttoMatchmakerMember
{
	FGuid Token;

	FTimespan Timeout;

	TMap<FString, FAttoFindSessionsParam> Params;

	// TODO: Do we actually need to wrap this in a TSharedRef?
	TSharedRef<TPromise<FAttoStartMatchmakingRequest::Result>> Promise;
};

struct FAttoMatchmakerSessionInfo
{
	FTimespan MatchmakerCooldown;
	FAttoSessionInfo SessionInfo;

	[[nodiscard]] bool IsJoinable() const;

	[[nodiscard]] bool HasCapacity(int32 NumNeededConnections) const;

	[[nodiscard]] bool Matches(const TMap<FString, FAttoFindSessionsParam>& Params) const;

	[[nodiscard]] bool CanJoin(const TMap<FString, FAttoFindSessionsParam>& Params, const int32 NumNeededConnections) const
	{
		return IsJoinable() && HasCapacity(NumNeededConnections) && Matches(Params);
	}

	[[nodiscard]] bool IsEmpty() const;
};

class FAttoMatchmaker final : public FNoncopyable
{
	// Absolute limit on FindSessions result
	const int32 MaxFindSessionsResults = 100;

	// A very naive attempt to avoid race conditions
	// TODO: Can we do better?
	const FTimespan MatchmakerCooldown = FTimespan::FromSeconds(30);

	// Reverse order so we first try to matchmake bigger teams
	TSortedMap<int32, TArray<FAttoMatchmakerMember>, FDefaultAllocator, TGreater<int32>> QueuesByPlayersNum;

	TMap<uint64, FAttoMatchmakerSessionInfo> Sessions;

	bool Remove(const FGuid& Token);

public:
	~FAttoMatchmaker();

	bool Cancel(FGuid& Token);

	bool AddSession(uint64 OwningUserId, const FAttoSessionInfo& SessionInfo);

	bool UpdateSession(uint64 OwningUserId, const FAttoSessionUpdatableInfo& UpdatableInfo);

	bool RemoveSession(uint64 OwningUserId);

	TFuture<FAttoStartMatchmakingRequest::Result> Enqueue(FGuid& Token,
	                                                      const int32 NumPlayers,
	                                                      const FAttoStartMatchmakingRequest& Message);

	void Tick(float DeltaSeconds);

	void FindSessions(int32 NumPlayers,
	                  const FAttoFindSessionsRequest& Message,
	                  TArray<FAttoSessionInfo>& OutResult);
};
