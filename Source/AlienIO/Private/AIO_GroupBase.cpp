#include "AIO_GroupBase.h"
#include "AlienIO.h"
#include "AIO_ComponentBase.h"

AAIO_GroupBase::AAIO_GroupBase() {}

void AAIO_GroupBase::BeginPlay() {
	Super::BeginPlay();
	const auto Delay = FMath::FRandRange(.0f, 1.0f);
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &AAIO_GroupBase::RunAioTick, 1.0f, true, Delay);
}

void AAIO_GroupBase::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
}

void AAIO_GroupBase::RunAioTick() {
	const auto Num = Members.Num();
	UE_LOG(LogAlienIO, Verbose, TEXT("Ticking AlienIO subsystem with %d members in group %s"), Num, *GetName());
	if (Num == 0) return;
	ParallelFor(Num, [this](int32 Index) {
		if (Members[Index].IsValid()) Members[Index]->AioTickPre();
	});
	ParallelFor(Num, [this](const int32 Index) {
		if (Members[Index].IsValid()) Members[Index]->AioTick();
	});
}

void AAIO_GroupBase::AddMember(UAIO_ComponentBase* Component) {
	Members.Add(Component);
	UE_LOG(LogAlienIO, Type::Log, TEXT("%s Joined group %s with %d members"), *Component->GetName(), *GetName(),
	       Members.Num());
}

void AAIO_GroupBase::RemoveMember(UAIO_ComponentBase* Component) {
	Members.Remove(Component);
	const auto Num = Members.Num();
	UE_LOG(LogAlienIO, Type::Log, TEXT("%s Left group %s with %d members"), *Component->GetName(), *GetName(), Num);
	if (Num == 0) Destroy();
}

void AAIO_GroupBase::MergeGroup(AAIO_GroupBase* Other) {
	if (!IsValid(Other)) return;
	for (const auto& Member : Other->Members)
		Member->JoinGroup(this);
}

void AAIO_GroupBase::AddIngredientProvider(TSubclassOf<class UFGItemDescriptor> Item, UAIO_ComponentBase* Component) {
	UE::TScopeLock Lock(IngredientProvidersLock);
	if (IngredientProviders.Contains(Item))
		IngredientProviders[Item].Key.AddUnique(Component);
	else
		IngredientProviders.Add(Item, TPair<TArray<UAIO_ComponentBase*>, int32>(TArray{Component}, 0));
}

void AAIO_GroupBase::RemoveIngredientProvider(TSubclassOf<class UFGItemDescriptor> Item,
                                              UAIO_ComponentBase* Component) {
	UE::TScopeLock Lock(IngredientProvidersLock);
	if (!IngredientProviders.Contains(Item)) return;
	IngredientProviders[Item].Key.Remove(Component);
	if (IngredientProviders[Item].Key.Num() == 0) IngredientProviders.Remove(Item);
}

int32 AAIO_GroupBase::PullIngredient(TSubclassOf<class UFGItemDescriptor> Item, int32 Count) {
	if (!IngredientProviders.Contains(Item)) return 0;
	int32 RemainingCount = Count;
	auto i = IngredientProviders[Item].Key.Num();
	if (i == 0) return 0;
	while (RemainingCount > 0 && --i >= 0) {
		const auto Provider = GetNextIngredientProvider(Item);
		if (Provider == nullptr) break;
		RemainingCount -= Provider->RemoveItem(Item, RemainingCount);
	}
	return Count - RemainingCount;
}

UAIO_ComponentBase* AAIO_GroupBase::GetNextIngredientProvider(TSubclassOf<class UFGItemDescriptor> Item) {
	UE::TScopeLock Lock(IngredientProvidersLock);
	auto& Providers = IngredientProviders[Item];
	const auto Num = Providers.Key.Num();
	Providers.Value++;
	if (Providers.Value >= Num) Providers.Value = 0;
	return Providers.Key[Providers.Value].Get();
}

void AAIO_GroupBase::AddProductOutput(TSubclassOf<class UFGItemDescriptor> Item, UAIO_ComponentBase* Component) {
	UE::TScopeLock Lock(IngredientProvidersLock);
	if (ProductOutputs.Contains(Item))
		ProductOutputs[Item].Key.AddUnique(Component);
	else
		ProductOutputs.Add(Item, TPair<TArray<UAIO_ComponentBase*>, int32>(TArray{Component}, 0));
}

void AAIO_GroupBase::RemoveProductOutput(TSubclassOf<class UFGItemDescriptor> Item, UAIO_ComponentBase* Component) {
	UE::TScopeLock Lock(IngredientProvidersLock);
	if (!ProductOutputs.Contains(Item)) return;
	ProductOutputs[Item].Key.Remove(Component);
	if (ProductOutputs[Item].Key.Num() == 0) ProductOutputs.Remove(Item);
}

int32 AAIO_GroupBase::PushProductToOutput(TSubclassOf<class UFGItemDescriptor> Item, int32 Count) {
	if (!ProductOutputs.Contains(Item)) return 0;
	int32 RemainingCount = Count;
	auto i = ProductOutputs[Item].Key.Num();
	if (i == 0) return 0;
	while (RemainingCount > 0 && --i >= 0) {
		const auto Provider = GetNextProductOutput(Item);
		if (Provider == nullptr) break;
		RemainingCount -= Provider->AddItem(Item, RemainingCount);
	}
	return Count - RemainingCount;
}

UAIO_ComponentBase* AAIO_GroupBase::GetNextProductOutput(TSubclassOf<class UFGItemDescriptor> Item) {
	UE::TScopeLock Lock(ProductOutputLock);
	auto& Output = ProductOutputs[Item];
	const auto Num = Output.Key.Num();
	Output.Value++;
	if (Output.Value >= Num) Output.Value = 0;
	return Output.Key[Output.Value].Get();
}
