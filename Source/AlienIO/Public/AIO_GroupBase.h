// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SpinLock.h"
#include "GameFramework/Actor.h"
#include "AIO_GroupBase.generated.h"

class UAIO_ComponentBase;

/**
 * AAIO_GroupBase is a class that manages a group of item providers.
 * It extends the AActor class and provides functionality to add, remove,
 * and pull items from the item providers.
 */
UCLASS()
class ALIENIO_API AAIO_GroupBase : public AActor {
	GENERATED_BODY()

protected:
	/** Critical section to ensure thread safety when accessing ItemProviders. */
	UE::FSpinLock IngredientProvidersLock;
	UE::FSpinLock ProductOutputLock;
	/** Map of item providers, where each item type is associated with a list of components and an index. */
	TMap<TSubclassOf<class UFGItemDescriptor>, TPair<TArray<TWeakObjectPtr<UAIO_ComponentBase>>, int32>>
	IngredientProviders;
	TMap<TSubclassOf<class UFGItemDescriptor>, TPair<TArray<TWeakObjectPtr<UAIO_ComponentBase>>, int32>> ProductOutputs;
	UPROPERTY()
	TArray<TWeakObjectPtr<UAIO_ComponentBase>> Members;
	UPROPERTY()
	FTimerHandle TimerHandle;

public:
	AAIO_GroupBase();

	// AIO_Tickable
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	void RunAioTick();
	void AddMember(UAIO_ComponentBase* Component);
	void RemoveMember(UAIO_ComponentBase* Component);
	UFUNCTION(BlueprintCallable)
	void MergeGroup(AAIO_GroupBase* Other);

	// Ingredients
	void AddIngredientProvider(TSubclassOf<class UFGItemDescriptor> Item, UAIO_ComponentBase* Component);
	void RemoveIngredientProvider(TSubclassOf<class UFGItemDescriptor> Item, UAIO_ComponentBase* Component);
	int32 PullIngredient(TSubclassOf<class UFGItemDescriptor> Item, int32 Count);
	UAIO_ComponentBase* GetNextIngredientProvider(TSubclassOf<class UFGItemDescriptor> Item);

	// Products
	void AddProductOutput(TSubclassOf<class UFGItemDescriptor> Item, UAIO_ComponentBase* Component);
	void RemoveProductOutput(TSubclassOf<class UFGItemDescriptor> Item, UAIO_ComponentBase* Component);
	int32 PushProductToOutput(TSubclassOf<class UFGItemDescriptor> Item, int32 Count);
	UAIO_ComponentBase* GetNextProductOutput(TSubclassOf<class UFGItemDescriptor> Item);
};
