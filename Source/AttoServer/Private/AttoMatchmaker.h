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

	TSortedMap<int32, TArray<FAttoMatchmakerMember>> QueuesByPlayersNum;

	// Stored here to reduce allocations in FAttoMatchmaker::Tick
	TArray<decltype(QueuesByPlayersNum)::ValueType::TIterator> Match;

	TMap<uint64, FAttoMatchmakerSessionInfo> Sessions;

	bool Remove(const FGuid& Token);

public:
	/** Minimum percentage of session capacity that must be filled to accept a partial match (0.0 to 1.0) */
	float PartialMatchThreshold = 0.5f;
	
	/** Minimum number of players required for any match, regardless of session capacity */
	int32 MinPlayersForPartialMatch = 2;

	~FAttoMatchmaker();

	/** Minimum percentage of session capacity that must be filled to accept a partial match (0.0 to 1.0) */
	float PartialMatchThreshold = 0.5f;
	
	/** Minimum number of players required for any match, regardless of session capacity */
	int32 MinPlayersForPartialMatch = 2;

	bool Cancel(FGuid& Token);

	bool CreateSession(uint64 OwningUserId, const FAttoSessionInfo& SessionInfo);

	bool UpdateSession(uint64 OwningUserId, const FAttoSessionUpdatableInfo& UpdatableInfo);

	bool DestroySession(uint64 OwningUserId);

	TFuture<FAttoStartMatchmakingRequest::Result> Enqueue(FGuid& Token,
	                                                      const int32 NumPlayers,
	                                                      const FAttoStartMatchmakingRequest& Message);

	void Tick(float DeltaSeconds);

	void FindSessions(int32 NumPlayers,
	                  const FAttoFindSessionsRequest& Message,
	                  TArray<FAttoSessionInfo>& OutResult);
};
