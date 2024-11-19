// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FGBuildableManufacturer.h"
#include "FGFactoryConnectionComponent.h"
#include "SpinLock.h"
#include "Async/ParallelFor.h"
#include "AIO_ComponentBase.generated.h"

class AAIO_Group;

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ALIENIO_API UAIO_ComponentBase : public UActorComponent {
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<AFGBuildableManufacturer> Owner;
	TObjectPtr<AAIO_Group> Group;
	UPROPERTY()
	TArray<TSoftObjectPtr<UFGFactoryConnectionComponent>> Outputs;
	UPROPERTY()
	TMap<TSubclassOf<class UFGItemDescriptor>, int32> MinItemAmounts;
	UE::FSpinLock InventoryLock;
	UPROPERTY()
	TSoftObjectPtr<UFGInventoryComponent> InputInventory;
	UPROPERTY()
	TSoftObjectPtr<UFGInventoryComponent> OutputInventory;
	UPROPERTY()
	TArray<TSubclassOf<UFGItemDescriptor>> Products = {};
	UPROPERTY()
	bool bBeltConnected;
	UPROPERTY()
	bool bPipeConnected;

public:
	void AioTickPre();
	void AioTick();

	UFUNCTION(BlueprintCallable, Category = "Group")
	void JoinGroup(AAIO_Group* NewGroup);

	UFUNCTION(BlueprintCallable, Category = "Group")
	void LeaveGroup();

	UFUNCTION(BlueprintCallable, Category = "Ingredient")
	int32 AddItem(TSubclassOf<class UFGItemDescriptor> Item, int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Ingredient")
	int32 RemoveItem(TSubclassOf<class UFGItemDescriptor> Item, int32 Amount);

	bool IsOutput() const {
		return bBeltConnected || bPipeConnected;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Item Configuration")
	void OnRecipeChanged(const TSubclassOf<UFGRecipe> Recipe);
	void GroupWithNeighbors();

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void DrawDebugBox(AActor* Actor, FVector Location, FVector Size);
};
