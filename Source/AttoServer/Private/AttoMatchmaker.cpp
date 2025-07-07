#include "AttoMatchmaker.h"
#include "AttoProtocol.h"
#include "Online/OnlineSessionNames.h"

bool FAttoMatchmakerSessionInfo::IsEmpty() const
{
	return SessionInfo.UpdatableInfo.NumOpenPublicConnections >= SessionInfo.UpdatableInfo.NumPublicConnections;
}

template<typename T>
static bool EvalOrderedComparisonOp(const EOnlineComparisonOp::Type ComparisonOp, const T& A, const T& B)
{
	switch (ComparisonOp)
	{
		case EOnlineComparisonOp::Equals:
			return A == B;

		case EOnlineComparisonOp::NotEquals:
			return A != B;

		case EOnlineComparisonOp::GreaterThan:
			return A > B;

		case EOnlineComparisonOp::GreaterThanEquals:
			return A >= B;

		case EOnlineComparisonOp::LessThan:
			return A < B;

		case EOnlineComparisonOp::LessThanEquals:
			return A <= B;

		default:
			return false;
	}
}

template<typename T>
static bool EvalLogicalComparisonOp(const EOnlineComparisonOp::Type ComparisonOp, const T& A, const T& B)
{
	switch (ComparisonOp)
	{
		case EOnlineComparisonOp::Equals:
			return A == B;

		case EOnlineComparisonOp::NotEquals:
			return A != B;

		default:
			return false;
	}
}

static bool EvalComparisonOp(const FAttoFindSessionsParam& Param, const FAttoSessionSettingValue& SessionValue)
{
	struct FSessionSettingValueComparator
	{
		explicit FSessionSettingValueComparator(const EOnlineComparisonOp::Type ComparisonOp)
		    : ComparisonOp{ComparisonOp}
		{}

		EOnlineComparisonOp::Type ComparisonOp;

		bool operator()(const bool bA, const bool bB) const
		{
			return EvalLogicalComparisonOp(ComparisonOp, bA, bB);
		}

		bool operator()(const FString& A, const FString& B) const
		{
			return EvalOrderedComparisonOp(ComparisonOp, A, B);
		}

		bool operator()(const TArray<uint8>& A, const TArray<uint8>& B) const
		{
			return EvalLogicalComparisonOp(ComparisonOp, A, B);
		}

		bool operator()(const int32 A, const int32 B) const
		{
			return EvalOrderedComparisonOp(ComparisonOp, A, B);
		}

		bool operator()(const int64 A, const int64 B) const
		{
			return EvalOrderedComparisonOp(ComparisonOp, A, B);
		}

		bool operator()(const uint32 A, const uint32 B) const
		{
			return EvalOrderedComparisonOp(ComparisonOp, A, B);
		}

		bool operator()(const uint64 A, const uint64 B) const
		{
			return EvalOrderedComparisonOp(ComparisonOp, A, B);
		}

		bool operator()(const float A, const float B) const
		{
			return EvalOrderedComparisonOp(ComparisonOp, A, B);
		}

		bool operator()(const double A, const double B) const
		{
			return EvalOrderedComparisonOp(ComparisonOp, A, B);
		}
	};

	return Visit(
	    [&]<typename T>(const T& SearchValue) {
		    if (const auto* Value = SessionValue.TryGet<std::remove_const_t<typename TRemoveReference<T>::Type>>())
		    {
			    return FSessionSettingValueComparator{Param.ComparisonOp}.operator()(*Value, SearchValue);
		    }

		    return false;
	    },
	    Param.Value);
}

bool FAttoMatchmakerSessionInfo::IsJoinable() const
{
	if (MatchmakerCooldown > FTimespan::Zero())
	{
		return false;
	}

	if (!SessionInfo.UpdatableInfo.bShouldAdvertise)
	{
		return false;
	}

	if (!SessionInfo.UpdatableInfo.bAllowJoinInProgress && SessionInfo.UpdatableInfo.State == EOnlineSessionState::InProgress)
	{
		return false;
	}

	return true;
}

bool FAttoMatchmakerSessionInfo::HasCapacity(const int32 NumNeededConnections) const
{
	return SessionInfo.UpdatableInfo.NumOpenPublicConnections >= NumNeededConnections;
}

bool FAttoMatchmakerSessionInfo::Matches(const TMap<FString, FAttoFindSessionsParam>& Params) const
{
	for (const auto& [ParamName, ParamValue] : Params)
	{
		if (ParamName == SEARCH_DEDICATED_ONLY)
		{
			if (!EvalComparisonOp(ParamValue, FAttoSessionSettingValue{TInPlaceType<bool>{}, SessionInfo.bIsDedicated}))
			{
				return false;
			}
		}
		else if (ParamName == SEARCH_NONEMPTY_SERVERS_ONLY)
		{
			if (!EvalComparisonOp(ParamValue, FAttoSessionSettingValue{TInPlaceType<bool>{}, !IsEmpty()}))
			{
				return false;
			}
		}
		else if (ParamName == SEARCH_EMPTY_SERVERS_ONLY)
		{
			if (!EvalComparisonOp(ParamValue, FAttoSessionSettingValue{TInPlaceType<bool>{}, IsEmpty()}))
			{
				return false;
			}
		}
		else if (ParamName == SEARCH_MINSLOTSAVAILABLE)
		{
			if (!EvalComparisonOp(ParamValue, FAttoSessionSettingValue{TInPlaceType<int32>{}, SessionInfo.UpdatableInfo.NumOpenPublicConnections}))
			{
				return false;
			}
		}
		else if (ParamName == SEARCH_SECURE_SERVERS_ONLY)
		{
			if (!EvalComparisonOp(ParamValue, FAttoSessionSettingValue{TInPlaceType<bool>{}, SessionInfo.bAntiCheatProtected}))
			{
				return false;
			}
		}
		else if (auto* SessionSetting = SessionInfo.Settings.Find(ParamName))
		{
			if (!EvalComparisonOp(ParamValue, *SessionSetting))
			{
				return false;
			}
		}
		else
		{
			// If session does not have some parameter, we do not filter it out.
			// This is what AdvancedSessionsPlugin does in UFindSessionsCallbackProxyAdvanced::FilterSessionResults
		}
	}

	return true;
}

FAttoMatchmaker::~FAttoMatchmaker()
{
	for (auto& [_, Queue] : QueuesByPlayersNum)
	{
		for (const auto& Member : Queue)
		{
			Member.Promise->EmplaceValue(TInPlaceType<FString>{}, TEXT("Shutdown"));
		}
	}
}

TFuture<FAttoStartMatchmakingRequest::Result> FAttoMatchmaker::Enqueue(FGuid& Token,
                                                                       const int32 NumPlayers,
                                                                       const FAttoStartMatchmakingRequest& Message)
{
	if (!ensure(!Token.IsValid()))
	{
		return MakeFulfilledPromise<FAttoStartMatchmakingRequest::Result>(TInPlaceType<FString>{}, TEXT("Already in queue")).GetFuture();
	}

	auto RequiredCapacity = NumPlayers;
	if (const auto* MinSlots = Message.Params.Find(SEARCH_MINSLOTSAVAILABLE.ToString()))
	{
		if (const auto* MinSlotsValue = MinSlots->Value.TryGet<int32>())
		{
			RequiredCapacity = FMath::Max(NumPlayers, *MinSlotsValue);
		}
	}

	Token = FGuid::NewGuid();
	return QueuesByPlayersNum.FindOrAdd(RequiredCapacity)
	    .Add_GetRef(FAttoMatchmakerMember{
	        .Token = Token,
	        .Timeout = Message.Timeout,
	        .Params = Message.Params,
	        .Promise = MakeShared<TPromise<FAttoStartMatchmakingRequest::Result>>()})
	    .Promise->GetFuture();
}

bool FAttoMatchmaker::Remove(const FGuid& Token)
{
	// TODO: Optimize
	for (auto& [_, Queue] : QueuesByPlayersNum)
	{
		for (auto MemberIt = Queue.CreateIterator(); MemberIt; ++MemberIt)
		{
			if (MemberIt->Token != Token)
			{
				continue;
			}

			MemberIt->Promise->EmplaceValue(TInPlaceType<FAttoStartMatchmakingRequest::FCanceled>{}, FAttoStartMatchmakingRequest::FCanceled{});
			MemberIt.RemoveCurrent();
			return true;
		}
	}

	return false;
}

bool FAttoMatchmaker::Cancel(FGuid& Token)
{
	if (Token.IsValid() && ensure(Remove(Token)))
	{
		Token.Invalidate();
		return true;
	}

	return false;
}

bool FAttoMatchmaker::CreateSession(const uint64 OwningUserId, const FAttoSessionInfo& SessionInfo)
{
	// TODO: Check if entry already exists? For any of users in the same connection? But what if they login later?
	Sessions.Add(OwningUserId, {{}, SessionInfo});
	return true;
}

bool FAttoMatchmaker::UpdateSession(uint64 OwningUserId, const FAttoSessionUpdatableInfo& UpdatableInfo)
{
	if (auto* Session = Sessions.Find(OwningUserId))
	{
		Session->SessionInfo.UpdatableInfo = UpdatableInfo;
		return true;
	}

	return false;
}

bool FAttoMatchmaker::DestroySession(const uint64 OwningUserId)
{
	return Sessions.Remove(OwningUserId) > 0;
}

void FAttoMatchmaker::Tick(const float DeltaSeconds)
{
	// TODO: Store sessions with cooldown in a separate collection
	for (auto& [_, Session] : Sessions)
	{
		if (Session.MatchmakerCooldown > FTimespan::Zero())
		{
			Session.MatchmakerCooldown -= FTimespan::FromSeconds(DeltaSeconds);
		}
	}

	// Build matches
	if (!QueuesByPlayersNum.IsEmpty())
	{
		for (auto& [_, Session] : Sessions)
		{
			// TODO: Store joinable sessions in a separate collection
			if (!Session.IsJoinable())
			{
				// Session cannot be joined by anyone
				// TODO: Maybe we want to store joinable and non-joinable sessions in separate collections?
				continue;
			}

			int32 RemainingCapacity = Session.SessionInfo.UpdatableInfo.NumOpenPublicConnections;

			// Reverse order so we first try to matchmake bigger teams
			for (auto& [NumPlayers, Queue] : ReverseIterate(QueuesByPlayersNum))
			{
				if (RemainingCapacity < NumPlayers)
				{
					// No member of current size will fit, move on to smaller sizes
					continue;
				}

				for (auto MemberIt = Queue.CreateIterator(); MemberIt; ++MemberIt)
				{
					if (!Session.Matches(MemberIt->Params))
					{
						continue;
					}

					Match.Add(MemberIt);
					if ((RemainingCapacity -= NumPlayers) < NumPlayers)
					{
						// No member of current size will fit, move on to smaller sizes
						break;
					}
				}
			}

			if (Match.IsEmpty())
			{
				continue;
			}

			if (RemainingCapacity > 0)
			{
				Match.Empty();
				// TODO: Add logic for partial matches
				continue;
			}

			// Reverse so that indexes do not invalidate when we remove members from queue
			for (auto Member : ReverseIterate(Match))
			{
				// Send session info
				Member->Promise->EmplaceValue(TInPlaceType<FAttoSessionInfo>{}, Session.SessionInfo);
				Member.RemoveCurrent();
			}

			Match.Empty();

			// Cooldown the session so players have time to connect to it
			Session.MatchmakerCooldown = MatchmakerCooldown;
		}

		// Expire timeout and cleanup queues
		for (auto QueueIt = QueuesByPlayersNum.CreateIterator(); QueueIt; ++QueueIt)
		{
			for (auto MemberIt = QueueIt.Value().CreateIterator(); MemberIt; ++MemberIt)
			{
				if (MemberIt->Timeout <= FTimespan::Zero())
				{
					// Infinite timeout
					continue;
				}

				if ((MemberIt->Timeout -= FTimespan::FromSeconds(DeltaSeconds)) >= FTimespan::Zero())
				{
					// Timeout hasn't expired yet
					continue;
				}

				MemberIt->Promise->EmplaceValue(TInPlaceType<FAttoStartMatchmakingRequest::FTimeout>{}, FAttoStartMatchmakingRequest::FTimeout{});
				MemberIt.RemoveCurrent();
			}

			if (QueueIt.Value().IsEmpty())
			{
				QueueIt.RemoveCurrent();
			}
		}
	}
}

void FAttoMatchmaker::FindSessions(const int32 NumPlayers,
                                   const FAttoFindSessionsRequest& Message,
                                   TArray<FAttoSessionInfo>& OutResult)
{
	const auto EffectiveMaxResults = FMath::Min(Message.MaxResults, MaxFindSessionsResults);

	for (const auto& [_, Session] : Sessions)
	{
		if (OutResult.Num() >= EffectiveMaxResults)
		{
			break;
		}

		if (!Session.CanJoin(Message.Params, NumPlayers))
		{
			continue;
		}

		OutResult.Add(Session.SessionInfo);
	}
}
