#include "CRProfileSaveGame.h"

#include "Misc/Guid.h"

namespace CRMeta
{
	FCRHamsterPersistentState* FindHamster(UCRProfileSaveGame& Profile, FName HamsterId)
	{
		return Profile.Hamsters.FindByPredicate([HamsterId](const FCRHamsterPersistentState& H) { return H.HamsterId == HamsterId; });
	}

	const FCRHamsterPersistentState* FindHamster(const UCRProfileSaveGame& Profile, FName HamsterId)
	{
		return Profile.Hamsters.FindByPredicate([HamsterId](const FCRHamsterPersistentState& H) { return H.HamsterId == HamsterId; });
	}

	const FCRHamsterPersistentState* FindSelectedHamster(const UCRProfileSaveGame& Profile)
	{
		const FCRHamsterPersistentState* Hamster = FindHamster(Profile, Profile.SelectedHamsterId);
		return Hamster && Hamster->bAlive ? Hamster : nullptr;
	}

	TArray<const FCRHamsterPersistentState*> GetLivingHamsters(const UCRProfileSaveGame& Profile)
	{
		TArray<const FCRHamsterPersistentState*> Living;
		for (const FCRHamsterPersistentState& Hamster : Profile.Hamsters)
		{
			if (Hamster.bAlive)
			{
				Living.Add(&Hamster);
			}
		}
		return Living;
	}

	int32 CountLivingHamsters(const UCRProfileSaveGame& Profile)
	{
		return Profile.Hamsters.FilterByPredicate([](const FCRHamsterPersistentState& H) { return H.bAlive; }).Num();
	}

	TArray<const FCRHamsterPersistentState*> GetGraveyard(const UCRProfileSaveGame& Profile)
	{
		TArray<const FCRHamsterPersistentState*> Dead;
		// Walk backwards so, for equal timestamps, the later roster entry (the later death) comes first.
		for (int32 i = Profile.Hamsters.Num() - 1; i >= 0; --i)
		{
			if (!Profile.Hamsters[i].bAlive)
			{
				Dead.Add(&Profile.Hamsters[i]);
			}
		}
		Dead.StableSort([](const FCRHamsterPersistentState& A, const FCRHamsterPersistentState& B)
		{
			return A.Death.DeathTimestamp > B.Death.DeathTimestamp;
		});
		return Dead;
	}

	FString GetRevealedEpitaph(const FCRHamsterPersistentState& Hamster)
	{
		return Hamster.bAlive ? FString() : Hamster.EpitaphText;
	}

	int32 CountUnseenGraves(const UCRProfileSaveGame& Profile)
	{
		return FMath::Max(0, (Profile.Hamsters.Num() - CountLivingHamsters(Profile)) - Profile.GraveyardSeenCount);
	}

	bool SelectHamster(UCRProfileSaveGame& Profile, FName HamsterId)
	{
		const FCRHamsterPersistentState* Hamster = FindHamster(Profile, HamsterId);
		if (!Hamster || !Hamster->bAlive)
		{
			return false;
		}
		Profile.SelectedHamsterId = HamsterId;
		return true;
	}

	bool InitializeHamsterRoster(UCRProfileSaveGame& Profile, const UCRHubCatalog* Catalog)
	{
		if (Profile.Hamsters.Num() > 0 || !Catalog || !Catalog->DefaultRoster)
		{
			return false;
		}
		for (const FCRHamsterDefinition& Definition : Catalog->DefaultRoster->Hamsters)
		{
			if (!Definition.HamsterId.IsNone() && !FindHamster(Profile, Definition.HamsterId))
			{
				Profile.Hamsters.Add(MakeHamster(Definition));
			}
		}
		return Profile.Hamsters.Num() > 0;
	}

	bool EnsureValidHamsterSelection(UCRProfileSaveGame& Profile)
	{
		if (FindSelectedHamster(Profile))
		{
			return false;
		}
		// Never a dead hamster: the first living one, or nobody (empty roster; the hub offers recruitment).
		const FCRHamsterPersistentState* FirstAlive = Profile.Hamsters.FindByPredicate([](const FCRHamsterPersistentState& H) { return H.bAlive; });
		const FName NewId = FirstAlive ? FirstAlive->HamsterId : NAME_None;
		const bool bChanged = NewId != Profile.SelectedHamsterId;
		Profile.SelectedHamsterId = NewId;
		return bChanged;
	}

	bool KillHamster(UCRProfileSaveGame& Profile, FName HamsterId, const FCRHamsterDeathRecord& Record)
	{
		FCRHamsterPersistentState* Hamster = FindHamster(Profile, HamsterId);
		if (!Hamster || !Hamster->bAlive)
		{
			return false;
		}
		Hamster->bAlive = false;
		Hamster->Death = Record;
		Hamster->Death.bValid = true;
		EnsureValidHamsterSelection(Profile);
		return true;
	}

	int32 GetKeepPercent(const UCRHubCatalog* Catalog, ECRRunEndReason Reason)
	{
		switch (Reason)
		{
		case ECRRunEndReason::Completed: return Catalog ? Catalog->CompletedRunKeepPercent : 100;
		case ECRRunEndReason::Failed:    return Catalog ? Catalog->FailedRunKeepPercent : 0;
		case ECRRunEndReason::Abandoned: return Catalog ? Catalog->AbandonedRunKeepPercent : 0;
		}
		return 0;
	}

	bool ApplyRunEnd(UCRProfileSaveGame& Profile, const UCRHubCatalog* Catalog, const FCRRunState& EndedRun, const FDateTime& Now)
	{
		if (!EndedRun.RunId.IsEmpty() && Profile.LastAppliedRunId == EndedRun.RunId)
		{
			return false; // this run's end is already in the profile
		}
		Profile.LastAppliedRunId = EndedRun.RunId;

		const int32 KeepPercent = GetKeepPercent(Catalog, EndedRun.EndReason);
		switch (EndedRun.EndReason)
		{
		case ECRRunEndReason::Completed: Profile.Stats.RunsCompleted++; break;
		case ECRRunEndReason::Failed:    Profile.Stats.RunsFailed++; break;
		case ECRRunEndReason::Abandoned: Profile.Stats.RunsAbandoned++; break;
		}

		FCRRunEndSummary Summary;
		Summary.bValid = true;
		Summary.Reason = EndedRun.EndReason;
		Summary.RunSeed = EndedRun.RunSeed;
		Summary.RoomsVisited = FMath::Max(0, EndedRun.VisitedNodeIds.Num() - 1);
		Summary.HamsterId = EndedRun.Hamster.HamsterId;
		Summary.HamsterName = EndedRun.Hamster.Name;
		Summary.DeathCause = EndedRun.EndReason == ECRRunEndReason::Failed ? EndedRun.DeathCause : ECRHamsterDeathCause::None;
		Summary.Carried.Silver = EndedRun.Carried.Silver;
		Summary.Carried.Food = EndedRun.Carried.Food;
		Summary.Carried.Wood = EndedRun.Carried.Wood;
		Summary.Delivered.Silver = ApplyKeepPercent(Summary.Carried.Silver, KeepPercent);
		Summary.Delivered.Food = ApplyKeepPercent(Summary.Carried.Food, KeepPercent);
		Summary.Delivered.Wood = ApplyKeepPercent(Summary.Carried.Wood, KeepPercent);
		Profile.Resources.Add(Summary.Delivered);
		Profile.LastRun = Summary;

		if (EndedRun.EndReason == ECRRunEndReason::Completed)
		{
			if (FCRHamsterPersistentState* Hamster = FindHamster(Profile, EndedRun.Hamster.HamsterId))
			{
				Hamster->Stats.FindOrAdd(HamsterStatRunsCompleted())++;
			}
		}
		else if (EndedRun.EndReason == ECRRunEndReason::Failed)
		{
			// Permanent death of exactly the hamster that went (abandoning or returning never kills).
			FCRHamsterDeathRecord Record;
			Record.DeathTimestamp = Now;
			Record.DeathCause = EndedRun.DeathCause;
			Record.RunSeed = EndedRun.RunSeed;
			Record.NodeId = EndedRun.CurrentNodeId;
			if (const FCRRunNodeData* Node = EndedRun.Nodes.FindByPredicate([&EndedRun](const FCRRunNodeData& N) { return N.NodeId == EndedRun.CurrentNodeId; }))
			{
				Record.RoomType = Node->RoomType;
			}
			Record.LostLoot = Summary.Carried;
			Record.RoomsVisited = Summary.RoomsVisited;
			KillHamster(Profile, EndedRun.Hamster.HamsterId, Record);
		}
		return true;
	}

	void RecordHamsterRunStart(UCRProfileSaveGame& Profile, FName HamsterId)
	{
		if (FCRHamsterPersistentState* Hamster = FindHamster(Profile, HamsterId))
		{
			Hamster->Stats.FindOrAdd(HamsterStatRunsStarted())++;
		}
	}

	bool CanRecruit(const UCRProfileSaveGame& Profile, const UCRRecruitmentDefinition* Recruitment)
	{
		return Recruitment && CountLivingHamsters(Profile) < Recruitment->TargetLivingRosterSize;
	}

	namespace
	{
		bool IsHamsterIdTaken(const UCRProfileSaveGame& Profile, FName Id)
		{
			return FindHamster(Profile, Id) != nullptr
				|| Profile.RecruitCandidates.ContainsByPredicate([Id](const FCRHamsterPersistentState& C) { return C.HamsterId == Id; });
		}

		bool IsHamsterNameTaken(const UCRProfileSaveGame& Profile, const FString& Name)
		{
			const auto SameName = [&Name](const FCRHamsterPersistentState& H) { return H.DisplayName.Equals(Name, ESearchCase::IgnoreCase); };
			return Profile.Hamsters.ContainsByPredicate(SameName) || Profile.RecruitCandidates.ContainsByPredicate(SameName);
		}
	}

	FCRHamsterPersistentState MakeRecruitCandidate(UCRProfileSaveGame& Profile, const UCRRecruitmentDefinition& Recruitment, FRandomStream& Stream)
	{
		FCRHamsterPersistentState Candidate;

		// Identity: a generated id, never a definition id or a name.
		do
		{
			Candidate.HamsterId = FName(*FString::Printf(TEXT("Recruit_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		}
		while (IsHamsterIdTaken(Profile, Candidate.HamsterId));
		Candidate.AvatarId = Candidate.HamsterId;

		// Name: unused pool names first (living, dead and other candidates all count), then the numbered fallback.
		TArray<FString> FreeNames;
		for (const FString& Name : Recruitment.NamePool)
		{
			if (!Name.TrimStartAndEnd().IsEmpty() && !IsHamsterNameTaken(Profile, Name))
			{
				FreeNames.AddUnique(Name.TrimStartAndEnd());
			}
		}
		if (FreeNames.Num() > 0)
		{
			Candidate.DisplayName = FreeNames[Stream.RandRange(0, FreeNames.Num() - 1)];
		}
		else
		{
			const FString Pattern = Recruitment.FallbackNamePattern.Contains(TEXT("{N}")) ? Recruitment.FallbackNamePattern : FString(TEXT("Хомяк {N}"));
			int32 Number = FMath::Max(1, Profile.NextRecruitNumber);
			do
			{
				Candidate.DisplayName = Pattern.Replace(TEXT("{N}"), *FString::FromInt(Number++));
			}
			while (IsHamsterNameTaken(Profile, Candidate.DisplayName));
		}
		Profile.NextRecruitNumber = FMath::Max(1, Profile.NextRecruitNumber) + 1;

		// Stats: prefer a template no other candidate uses, so the offer shows different builds.
		TArray<int32> Templates;
		for (int32 i = 0; i < Recruitment.StatTemplates.Num(); ++i)
		{
			const FCRRecruitStatTemplate& T = Recruitment.StatTemplates[i];
			const bool bUsed = Profile.RecruitCandidates.ContainsByPredicate([&T](const FCRHamsterPersistentState& C)
			{
				return C.BaseMaxHP == T.BaseMaxHP && C.BaseManaPerTurn == T.BaseManaPerTurn;
			});
			if (!bUsed)
			{
				Templates.Add(i);
			}
		}
		if (Templates.Num() == 0)
		{
			for (int32 i = 0; i < Recruitment.StatTemplates.Num(); ++i)
			{
				Templates.Add(i);
			}
		}
		if (Templates.Num() > 0)
		{
			const FCRRecruitStatTemplate& T = Recruitment.StatTemplates[Templates[Stream.RandRange(0, Templates.Num() - 1)]];
			Candidate.BaseMaxHP = FMath::Max(1, T.BaseMaxHP);
			Candidate.BaseManaPerTurn = FMath::Max(1, T.BaseManaPerTurn);
		}

		if (Recruitment.TintOptions.Num() > 0)
		{
			Candidate.AvatarTint = Recruitment.TintOptions[Stream.RandRange(0, Recruitment.TintOptions.Num() - 1)];
		}

		// The epitaph is written now and stays secret until death (death only reveals it).
		const FString Epitaph = Recruitment.EpitaphPool.Num() > 0
			? Recruitment.EpitaphPool[Stream.RandRange(0, Recruitment.EpitaphPool.Num() - 1)]
			: FString(TEXT("{Name}. Ушёл в поход и не вернулся."));
		Candidate.EpitaphText = Epitaph.Replace(TEXT("{Name}"), *Candidate.DisplayName);
		Candidate.bAlive = true;
		return Candidate;
	}

	bool RefillRecruitCandidates(UCRProfileSaveGame& Profile, const UCRRecruitmentDefinition* Recruitment)
	{
		if (!Recruitment || Profile.RecruitCandidates.Num() >= Recruitment->CandidateCount)
		{
			return false;
		}
		// Only which candidate appears is random; once created it is saved and never rerolled.
		FRandomStream Stream(int32(FPlatformTime::Cycles() ^ uint32(Profile.NextRecruitNumber * 7919)));
		while (Profile.RecruitCandidates.Num() < Recruitment->CandidateCount)
		{
			Profile.RecruitCandidates.Add(MakeRecruitCandidate(Profile, *Recruitment, Stream));
		}
		return true;
	}

	bool RecruitCandidate(UCRProfileSaveGame& Profile, const UCRRecruitmentDefinition* Recruitment, FName CandidateId)
	{
		const int32 Index = Profile.RecruitCandidates.IndexOfByPredicate([CandidateId](const FCRHamsterPersistentState& C) { return C.HamsterId == CandidateId; });
		if (!CanRecruit(Profile, Recruitment) || Index == INDEX_NONE || FindHamster(Profile, CandidateId))
		{
			return false;
		}

		FCRHamsterPersistentState Recruit = Profile.RecruitCandidates[Index];
		Recruit.bAlive = true;
		Recruit.Death = FCRHamsterDeathRecord();
		Profile.Hamsters.Add(Recruit);
		Profile.RecruitCandidates.RemoveAt(Index);
		if (!FindSelectedHamster(Profile))
		{
			Profile.SelectedHamsterId = Recruit.HamsterId;
		}
		RefillRecruitCandidates(Profile, Recruitment);
		return true;
	}

	bool MigrateProfile(UCRProfileSaveGame& Profile, const UCRHubCatalog* Catalog)
	{
		bool bChanged = false;
		// Version 1 -> 2: the roster. The flag (not the version number) decides, because SaveGames do not store
		// properties that equal their defaults. If the catalog is missing the flag stays false and we retry later.
		if (!Profile.bHamsterRosterInitialized)
		{
			InitializeHamsterRoster(Profile, Catalog);
			if (Profile.Hamsters.Num() > 0)
			{
				Profile.bHamsterRosterInitialized = true;
				bChanged = true;
			}
		}
		// Version 2 -> 3: nothing existing changes (hamsters stay alive with their stats and epitaphs); the
		// profile only gains saved recruitment candidates. Also tops the list up for later versions.
		if (Profile.bHamsterRosterInitialized && Catalog)
		{
			bChanged |= RefillRecruitCandidates(Profile, Catalog->Recruitment);
		}
		if (Profile.bHamsterRosterInitialized && Profile.SaveVersion < CurrentProfileSaveVersion)
		{
			Profile.SaveVersion = CurrentProfileSaveVersion;
			bChanged = true;
		}
		bChanged |= EnsureValidHamsterSelection(Profile);
		return bChanged;
	}
}
