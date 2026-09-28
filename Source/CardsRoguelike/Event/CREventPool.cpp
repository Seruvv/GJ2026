#include "CREventPool.h"

#include "CREventDefinition.h"

TSoftObjectPtr<UCREventDefinition> UCREventPool::PickEvent() const
{
	float Total = 0.f;
	for (const FCREventPoolEntry& Entry : Entries)
	{
		if (!Entry.Event.IsNull() && Entry.Weight > 0.f)
		{
			Total += Entry.Weight;
		}
	}
	if (Total <= 0.f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.f, Total);
	for (const FCREventPoolEntry& Entry : Entries)
	{
		if (Entry.Event.IsNull() || Entry.Weight <= 0.f)
		{
			continue;
		}
		if (Roll < Entry.Weight)
		{
			return Entry.Event;
		}
		Roll -= Entry.Weight;
	}

	// Floating-point edge (Roll == Total): the last valid entry.
	for (int32 i = Entries.Num() - 1; i >= 0; --i)
	{
		if (!Entries[i].Event.IsNull() && Entries[i].Weight > 0.f)
		{
			return Entries[i].Event;
		}
	}
	return nullptr;
}
