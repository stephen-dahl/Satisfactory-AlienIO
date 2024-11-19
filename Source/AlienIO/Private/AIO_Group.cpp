#include "AIO_Group.h"
#include "AlienIO.h"
#include "AIO_ComponentBase.h"

void AAIO_Group::BeginPlay() {
	Super::BeginPlay();
	const auto Delay = FMath::FRandRange(.0f, 1.0f);
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &AAIO_Group::RunAioTick, 1.0f, true, Delay);
	UE_LOG(LogAlienIO, Verbose, TEXT("%s created with delay %f"), *GetName(), Delay);
}

void AAIO_Group::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	UE_LOG(LogAlienIO, Verbose, TEXT("%s destroyed"), *GetName());
}

void AAIO_Group::RunAioTick() {
	const auto Num = Members.Num();
	if (Num == 0) {
		return;
	}
	ParallelFor(8, [this, Num](int32 ThreadIndex) {
		int32 StartIndex = (Num * ThreadIndex) / 8;
		int32 EndIndex = (Num * (ThreadIndex + 1)) / 8;
		for (int32 Index = StartIndex; Index < EndIndex; ++Index) {
			if (IsValid(Members[Index])) {
				Members[Index]->AioTickPre();
			}
		}
	}, false);
	ParallelFor(8, [this, Num](int32 ThreadIndex) {
		int32 StartIndex = (Num * ThreadIndex) / 8;
		int32 EndIndex = (Num * (ThreadIndex + 1)) / 8;
		for (int32 Index = StartIndex; Index < EndIndex; ++Index) {
			if (IsValid(Members[Index])) {
				Members[Index]->AioTick();
			}
		}
	}, false);
}

void AAIO_Group::AddMember(UAIO_ComponentBase* Component) {
	if (!IsValid(Component)) {
		UE_LOG(LogAlienIO, Type::Error, TEXT("Invalid component provided"));
		return;
	}
	if (Members.Find(Component) != INDEX_NONE) {
		UE_LOG(LogAlienIO, Type::Warning, TEXT("Component already in group"));
		return;
	}
	Members.Add(Component);
	auto GroupName = GetName();
	auto ComponentName = Component->GetName();
	auto Num = Members.Num();
	UE_LOG(LogAlienIO, Type::Log, TEXT("%s Joined group %s with %d members"), *ComponentName, *GroupName, Num);
}

void AAIO_Group::RemoveMember(UAIO_ComponentBase* Component) {
	if (!IsValid(Component)) {
		UE_LOG(LogAlienIO, Type::Error, TEXT("Invalid component provided"));
		return;
	}
	Members.Remove(Component);
	auto GroupName = GetName();
	auto ComponentName = Component->GetName();
	auto Num = Members.Num();
	UE_LOG(LogAlienIO, Type::Log, TEXT("%s Left group %s with %d members left"), *ComponentName, *GroupName, Num);
	if (Num == 0 && !IsActorBeingDestroyed()) {
		Destroy();
	}
}

void AAIO_Group::MergeGroup(AAIO_Group* Other) {
	if (!IsValid(Other)) {
		return;
	}
	while (Other->Members.Num() > 0) {
		auto Member = Other->Members.Pop(false);
		if (!IsValid(Member)) {
			UE_LOG(LogAlienIO, Type::Error, TEXT("Invalid member found in group"));
			continue;
		}
		Member->JoinGroup(this);
	}
}

void AAIO_Group::AddProvider(TSubclassOf<class UFGItemDescriptor> Item, UAIO_ComponentBase* Component) {
	UE::TScopeLock Lock(IngredientProvidersLock);
	IngredientProviders.FindOrAdd(Item).Key.AddUnique(Component);
}

void AAIO_Group::RemoveProvider(TSubclassOf<class UFGItemDescriptor> Item,
                                UAIO_ComponentBase* Component) {
	UE::TScopeLock Lock(IngredientProvidersLock);
	auto Pair = IngredientProviders.Find(Item);
	if (Pair == nullptr) {
		return;
	}
	Pair->Key.Remove(Component);
}

int32 AAIO_Group::PullItem(TSubclassOf<class UFGItemDescriptor> Item, int32 Count) {
	if (!IngredientProviders.Contains(Item)) {
		return 0;
	}
	int32 RemainingCount = Count;
	auto i = IngredientProviders.FindRef(Item).Key.Num();
	if (i == 0) {
		return 0;
	}
	while (RemainingCount > 0 && --i >= 0) {
		const auto Provider = GetNextProvider(Item);
		if (Provider == nullptr || Provider->IsOutput()) {
			continue;
		}
		RemainingCount -= Provider->RemoveItem(Item, RemainingCount);
	}
	return Count - RemainingCount;
}

UAIO_ComponentBase* AAIO_Group::GetNextProvider(TSubclassOf<class UFGItemDescriptor> Item) {
	UE::TScopeLock Lock(IngredientProvidersLock);
	auto Providers = IngredientProviders.Find(Item);
	if (Providers == nullptr) {
		return nullptr;
	}
	const auto Num = Providers->Key.Num();
	Providers->Value++;
	if (Providers->Value >= Num) {
		Providers->Value = 0;
	}
	return Providers->Key[Providers->Value];
}
