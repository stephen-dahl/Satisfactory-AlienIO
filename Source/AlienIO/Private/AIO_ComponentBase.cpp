#include "AIO_ComponentBase.h"
#include "AIO_GroupBase.h"
#include "AlienIO.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMeshActor.h"


UAIO_ComponentBase::UAIO_ComponentBase() {
	PrimaryComponentTick.bCanEverTick = false;
}

void UAIO_ComponentBase::BeginPlay() {
	Super::BeginPlay();
	const auto World = GetWorld();
	if (!IsValid(World)) UE_LOG(LogAlienIO, Type::Fatal, TEXT("No world found!"));
	Owner = Cast<AFGBuildableManufacturer>(GetOwner());
	InputInventory = Owner->GetInputInventory();
	OutputInventory = Owner->GetOutputInventory();
	for (auto Connections = Owner->GetConnectionComponents(); auto Connection : Connections) {
		if (Connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT) Outputs.Add(Connection);
	}
	GroupWithNeighbors();
}

void UAIO_ComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
	LeaveGroup();
}

void UAIO_ComponentBase::AioTickPre() {
	// update provided ingredients
	for (const auto& MinItemAmount : MinItemAmounts) {
		if (InputInventory->GetNumItems(MinItemAmount.Key) > MinItemAmount.Value)
			Group->AddIngredientProvider(MinItemAmount.Key, this);
		else
			Group->RemoveIngredientProvider(MinItemAmount.Key, this);
	}

	// update output connections
	bBeltConnected = false;
	bPipeConnected = false;
	for (const auto& Output : Outputs) {
		if (Output->IsConnected()) {
			if (const auto Type = Output->GetConnector(); Type == EFactoryConnectionConnector::FCC_CONVEYOR)
				bBeltConnected = true;
			else if (Type == EFactoryConnectionConnector::FCC_PIPE)
				bPipeConnected = true;
		}
	}

	// update accepted products
	for (const auto& Product : Products) {
		const auto Form = UFGItemDescriptor::GetForm(Product);
		if (Form == EResourceForm::RF_SOLID)
			bBeltConnected ? Group->AddProductOutput(Product, this) : Group->RemoveProductOutput(Product, this);
		else if (Form == EResourceForm::RF_LIQUID)
			bPipeConnected ? Group->AddProductOutput(Product, this) : Group->RemoveProductOutput(Product, this);
	}
}

void UAIO_ComponentBase::AioTick() {
	// pull ingredients
	for (const auto& MinItemAmount : MinItemAmounts) {
		const auto Have = InputInventory->GetNumItems(MinItemAmount.Key);
		const auto CanFit = UFGItemDescriptor::GetStackSize(MinItemAmount.Key) - Have;
		const auto Take = std::min(MinItemAmount.Value, CanFit);
		if (Take == 0) return;
		const auto Taken = Group->PullIngredient(MinItemAmount.Key, Take);
		if (Taken == 0) return;
		InputInventory->AddStack(FInventoryStack(Taken, MinItemAmount.Key));
	}
	// push products
	TArray<FInventoryStack> Stacks;
	OutputInventory->GetInventoryStacks(Stacks);
	for (const auto& Stack : Stacks) {
		const auto Item = Stack.Item.GetItemClass();
		const auto Form = UFGItemDescriptor::GetForm(Item);
		if (bBeltConnected && Form == EResourceForm::RF_SOLID) continue;
		if (bPipeConnected && Form == EResourceForm::RF_LIQUID) continue;
		const auto NumberPushed = Group->PushProductToOutput(Item, Stack.NumItems);
		OutputInventory->Remove(Item, NumberPushed);
	}
}

int32 UAIO_ComponentBase::AddItem(TSubclassOf<class UFGItemDescriptor> Item, int32 Amount) {
	UE::TScopeLock Lock(InventoryLock);
	return Owner->GetPotentialInventory()->AddStack(FInventoryStack(Amount, Item), true);
}

int32 UAIO_ComponentBase::RemoveItem(TSubclassOf<class UFGItemDescriptor> Item, int32 Amount) {
	UE::TScopeLock Lock(InventoryLock);
	const auto Have = InputInventory->GetNumItems(Item);
	if (Have < Amount) Amount = Have;
	InputInventory->Remove(Item, Amount);
	return Amount;
}

void UAIO_ComponentBase::OnRecipeChanged(const TSubclassOf<UFGRecipe> Recipe) {
	// store min item amounts
	const auto Ingredients = UFGRecipe::GetIngredients(Recipe);
	MinItemAmounts.Empty(Ingredients.Num());
	for (const auto& Ingredient : Ingredients) MinItemAmounts.Add(Ingredient.ItemClass, Ingredient.Amount);
	// store products
	const auto P = UFGRecipe::GetProducts(Recipe);
	Products.Empty(P.Num());
	for (const auto& Product : P) Products.Add(Product.ItemClass);
}

void UAIO_ComponentBase::JoinGroup(AAIO_GroupBase* NewGroup) {
	LeaveGroup();
	Group = NewGroup;
	Group->AddMember(this);
}

void UAIO_ComponentBase::LeaveGroup() {
	if (!IsValid(Group)) return;
	for (const auto& MinItemAmount : MinItemAmounts)
		Group->RemoveIngredientProvider(MinItemAmount.Key, this);
	for (const auto& Product : Products)
		Group->RemoveProductOutput(Product, this);
	Group->RemoveMember(this);
}

void UAIO_ComponentBase::GroupWithNeighbors() {
	auto Clearance = Owner->GetCombinedClearanceBox();
	TArray<AActor*> OverlapResults;

	// auto Position = Owner->GetActorLocation() + Clearance.GetCenter();
	// auto Extents = (Clearance.GetExtent());
	// DrawDebugBox(Owner, Clearance.GetCenter(), (Extents * 2) + 200);
	// DrawDebugBox(Owner, Clearance.GetCenter(), (Extents * 2));

	auto Box = Cast<UBoxComponent>(Owner->AddComponentByClass(
		UBoxComponent::StaticClass(), false,
		FTransform(FRotator::ZeroRotator, Clearance.GetCenter()), false
	));
	
	Box->SetBoxExtent(Clearance.GetExtent() + 200);
	Box->GetOverlappingActors(OverlapResults, AFGBuildableManufacturer::StaticClass());
	Box->DestroyComponent();

	UE_LOG(LogAlienIO, Type::Log, TEXT("Found %d neighbors"), OverlapResults.Num() - 1);

	for (const auto& Result : OverlapResults) {
		if (Result == Owner) continue;
		auto Actor = Cast<AFGBuildableManufacturer>(Result);
		if (!IsValid(Actor)) {
			UE_LOG(LogAlienIO, Type::Log, TEXT("Invalid actor found"));
			continue;
		}
		auto* Component = Actor->FindComponentByClass<UAIO_ComponentBase>();
		if (!IsValid(Component)) {
			UE_LOG(LogAlienIO, Type::Log, TEXT("Invalid component found"));
			continue;
		}
		if (Group == Component->Group) continue;
		if (IsValid(Group) && IsValid(Component->Group)) Component->Group->MergeGroup(Group);
		else if (IsValid(Component->Group)) JoinGroup(Component->Group);
	}
	if(!IsValid(Group)) JoinGroup(NewObject<AAIO_GroupBase>());
}
