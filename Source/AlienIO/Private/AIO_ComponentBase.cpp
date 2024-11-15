#include "AIO_ComponentBase.h"
#include "AIO_Group.h"
#include "AlienIO.h"
#include "Components/BoxComponent.h"

void UAIO_ComponentBase::BeginPlay() {
	Super::BeginPlay();
	const auto World = GetWorld();
	if (!IsValid(World)) UE_LOG(LogAlienIO, Type::Fatal, TEXT("No world found!"));
	Owner = Cast<AFGBuildableManufacturer>(GetOwner());
	InputInventory = Owner->GetInputInventory();
	OutputInventory = Owner->GetOutputInventory();
	for (auto Connections = Owner->GetConnectionComponents(); auto Connection : Connections) {
		if (Connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT) {
			Outputs.Add(Connection);
		}
	}
	GroupWithNeighbors();
	OnRecipeChanged(Owner->GetCurrentRecipe());
	Owner->.AddDynamic(this, &UAIO_ComponentBase::OnRecipeChanged);
}

void UAIO_ComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
	LeaveGroup();
}

void UAIO_ComponentBase::AioTickPre() {
	// update provided ingredients
	for (const auto& MinItemAmount : MinItemAmounts) {
		auto Need = std::min(MinItemAmount.Value * 2, UFGItemDescriptor::GetStackSize(MinItemAmount.Key));
		if (InputInventory->GetNumItems(MinItemAmount.Key) > Need) {
			Group->AddProvider(MinItemAmount.Key, this);
		}
		else {
			Group->RemoveProvider(MinItemAmount.Key, this);
		}
	}
	// update provided products
	for (const auto& Product : Products) {
		if (OutputInventory->GetNumItems(Product) > 0) {
			Group->AddProvider(Product, this);
		}
		else {
			Group->RemoveProvider(Product, this);
		}
	}
}

void UAIO_ComponentBase::AioTick() {
	// pull ingredients
	for (const auto& MinItemAmount : MinItemAmounts) {
		const auto Have = InputInventory->GetNumItems(MinItemAmount.Key);
		if (Have > MinItemAmount.Value) {
			continue;
		}
		const auto CanFit = UFGItemDescriptor::GetStackSize(MinItemAmount.Key) - Have;
		const auto Take = std::min(MinItemAmount.Value, CanFit);
		if (Take == 0) {
			return;
		}
		const auto Taken = Group->PullItem(MinItemAmount.Key, Take);
		if (Taken == 0) {
			return;
		}
		InputInventory->AddStack(FInventoryStack(Taken, MinItemAmount.Key));
	}

	// update output connections
	auto bBeltConnected = false;
	auto bPipeConnected = false;
	for (const auto& Output : Outputs) {
		if (Output->IsConnected()) {
			if (const auto Type = Output->GetConnector(); Type == EFactoryConnectionConnector::FCC_CONVEYOR) {
				bBeltConnected = true;
			}
			else if (Type == EFactoryConnectionConnector::FCC_PIPE) {
				bPipeConnected = true;
			}
		}
	}

	// Pull Outputs
	for (const auto& Product : Products) {
		const auto Form = UFGItemDescriptor::GetForm(Product);
		if (!(bBeltConnected && Form == EResourceForm::RF_SOLID)
			&& !(bPipeConnected && Form == EResourceForm::RF_LIQUID)) {
			continue;
		}
		auto Have = OutputInventory->GetNumItems(Product);
		auto CanFit = UFGItemDescriptor::GetStackSize(Product) - Have;
		auto Got = Group->PullItem(Product, CanFit);
		OutputInventory->AddStack(FInventoryStack(Got, Product));
	}
}

int32 UAIO_ComponentBase::AddItem(TSubclassOf<class UFGItemDescriptor> Item, int32 Amount) {
	UE::TScopeLock Lock(InventoryLock);
	return Owner->GetPotentialInventory()->AddStack(FInventoryStack(Amount, Item), true);
}

int32 UAIO_ComponentBase::RemoveItem(TSubclassOf<class UFGItemDescriptor> Item, int32 Amount) {
	UE::TScopeLock Lock(InventoryLock);
	const auto IsProduct = Products.Contains(Item);
	const auto Inventory = IsProduct ? OutputInventory : InputInventory;
	const auto Need = IsProduct ? 0 : MinItemAmounts.FindRef(Item);
	const auto Have = Inventory->GetNumItems(Item) - Need;
	if (Have < Amount) {
		Amount = Have;
		Group->RemoveProvider(Item, this);
	}
	if (Amount <= 0) {
		return 0;
	}
	Inventory->Remove(Item, Amount);
	return Amount;
}

void UAIO_ComponentBase::OnRecipeChanged(const TSubclassOf<UFGRecipe> Recipe) {
	UE_LOG(LogAlienIO, Log, TEXT("%s: Recipe changed to %s"), *GetName(), *UFGRecipe::GetRecipeName(Recipe).ToString());

	// store min item amounts
	const auto Ingredients = UFGRecipe::GetIngredients(Recipe);
	MinItemAmounts.Empty(Ingredients.Num());
	for (const auto& Ingredient : Ingredients) {
		MinItemAmounts.Add(Ingredient.ItemClass, Ingredient.Amount);
	}
	// store products
	const auto P = UFGRecipe::GetProducts(Recipe);
	Products.Empty(P.Num());
	for (const auto& Product : P) {
		Products.Add(Product.ItemClass);
	}
}

void UAIO_ComponentBase::JoinGroup(AAIO_Group* NewGroup) {
	LeaveGroup();
	Group = NewGroup;
	Group->AddMember(this);
}

void UAIO_ComponentBase::LeaveGroup() {
	if (!IsValid(Group)) {
		return;
	}
	for (const auto& MinItemAmount : MinItemAmounts) {
		Group->RemoveProvider(MinItemAmount.Key, this);
	}
	for (const auto& Product : Products) {
		Group->RemoveProvider(Product, this);
	}
	Group->RemoveMember(this);
}

void UAIO_ComponentBase::GroupWithNeighbors() {
	auto Clearance = Owner->GetCombinedClearanceBox();
	TArray<AActor*> OverlapResults;

	// DrawDebugBox(Owner, Clearance.GetCenter(), Clearance.GetExtent() * 2 + 200);
	// DrawDebugBox(Owner, Clearance.GetCenter(), Clearance.GetExtent() * 2);

	auto Box = Cast<UBoxComponent>(Owner->AddComponentByClass(
		UBoxComponent::StaticClass(), false,
		FTransform(FRotator::ZeroRotator, Clearance.GetCenter()), false
	));

	Box->SetBoxExtent(Clearance.GetExtent() + 200);
	Box->GetOverlappingActors(OverlapResults, AFGBuildableManufacturer::StaticClass());
	Box->DestroyComponent();

	UE_LOG(LogAlienIO, Type::Log, TEXT("Found %d neighbors"), OverlapResults.Num() - 1);

	for (const auto& Result : OverlapResults) {
		if (Result == Owner) {
			continue;
		}
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
		if (Group == Component->Group) {
			continue;
		}
		if (IsValid(Group) && IsValid(Component->Group)) {
			Component->Group->MergeGroup(Group);
		}
		else if (IsValid(Component->Group)) {
			JoinGroup(Component->Group);
		}
	}
	if (!IsValid(Group)) {
		JoinGroup(GetWorld()->SpawnActor<AAIO_Group>());
	}
}
