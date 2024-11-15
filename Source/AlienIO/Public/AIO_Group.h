// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SpinLock.h"
#include "GameFramework/Actor.h"
#include "AIO_Group.generated.h"

class UAIO_ComponentBase;

/**
 * AAIO_GroupBase is a class that manages a group of item providers.
 * It extends the AActor class and provides functionality to add, remove,
 * and pull items from the item providers.
 */
UCLASS()
class ALIENIO_API AAIO_Group : public AActor {
	GENERATED_BODY()

protected:
	/** Critical section to ensure thread safety when accessing ItemProviders. */
	UE::FSpinLock IngredientProvidersLock;
	UE::FSpinLock ProductOutputLock;
	/** Map of item providers, where each item type is associated with a list of components and an index. */
	TMap<TSubclassOf<UFGItemDescriptor>, TPair<TArray<TObjectPtr<UAIO_ComponentBase>>, int32>> IngredientProviders;
	TMap<TSubclassOf<UFGItemDescriptor>, TPair<TArray<TObjectPtr<UAIO_ComponentBase>>, int32>> ProductOutputs;
	UPROPERTY()
	TArray<TObjectPtr<UAIO_ComponentBase>> Members;
	UPROPERTY()
	FTimerHandle TimerHandle;

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	void RunAioTick();

	// Group management
	UFUNCTION(BlueprintCallable)
	void MergeGroup(AAIO_Group* Other);
	void AddMember(UAIO_ComponentBase* Component);
	void RemoveMember(UAIO_ComponentBase* Component);

	// Items
	void AddProvider(TSubclassOf<class UFGItemDescriptor> Item, UAIO_ComponentBase* Component);
	void RemoveProvider(TSubclassOf<class UFGItemDescriptor> Item, UAIO_ComponentBase* Component);
	int32 PullItem(TSubclassOf<class UFGItemDescriptor> Item, int32 Count);
	UAIO_ComponentBase* GetNextProvider(TSubclassOf<class UFGItemDescriptor> Item);
};
