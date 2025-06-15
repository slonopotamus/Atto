#include "AttoProtocol.h"
#include "Online/OnlineSessionNames.h"

bool FAttoSessionInfo::Matches(const TMap<FName, FAttoFindSessionsParam>& Params) const
{
	if (const auto* SearchDedicatedOnly = Params.Find(SEARCH_DEDICATED_ONLY))
	{
		// TODO: Respect ComparisonOp and Value
		if (!bIsDedicated)
		{
			return false;
		}
	}

	if (const auto* SearchNonEmptyOnly = Params.Find(SEARCH_NONEMPTY_SERVERS_ONLY))
	{
		// TODO: Respect ComparisonOp and Value
		if (IsEmpty())
		{
			return false;
		}
	}

	if (const auto* SearchEmptyOnly = Params.Find(SEARCH_EMPTY_SERVERS_ONLY))
	{
		// TODO: Respect ComparisonOp and Value
		if (!IsEmpty())
		{
			return false;
		}
	}

	if (const auto* SearchMinSlots = Params.Find(SEARCH_MINSLOTSAVAILABLE))
	{
		// TODO: Respect ComparisonOp
		if (const auto* Min = SearchMinSlots->Value.TryGet<int32>())
		{
			if (UpdatableInfo.NumOpenPublicConnections < *Min)
			{
				return false;
			}
		}
	}

	if (const auto* SearchAntiCheatProtected = Params.Find(SEARCH_SECURE_SERVERS_ONLY))
	{
		// TODO: Respect ComparisonOp
		if (!bAntiCheatProtected)
		{
			return false;
		}
	}

	// TODO: Support matching custom params
	return true;
}
