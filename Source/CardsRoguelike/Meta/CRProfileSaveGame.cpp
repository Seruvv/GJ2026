#include "CRProfileSaveGame.h"

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
		const FCRHamsterPersistentState* FirstAlive = Profile.Hamsters.FindByPredicate([](const FCRHamsterPersistentState& H) { return H.bAlive; });
		const FName NewId = FirstAlive ? FirstAlive->HamsterId : NAME_None;
		const bool bChanged = NewId != Profile.SelectedHamsterId;
		Profile.SelectedHamsterId = NewId;
		return bChanged;
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
		if (Profile.bHamsterRosterInitialized && Profile.SaveVersion < CurrentProfileSaveVersion)
		{
			Profile.SaveVersion = CurrentProfileSaveVersion;
			bChanged = true;
		}
		bChanged |= EnsureValidHamsterSelection(Profile);
		return bChanged;
	}
}