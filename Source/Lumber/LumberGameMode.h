// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LumberGameMode.generated.h"

class ATree;

UCLASS(minimalapi)
class ALumberGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALumberGameMode();
	virtual void BeginPlay();
	virtual void Tick(float DeltaSeconds) override;

	TQueue<void> TaskQueue;

	bool Started = false;
	float CurrentTime = 0;
// Default Variables
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<TSubclassOf<ATree>> TreeClasses;

// Functions
public:
	TArray<FVector> MakeCircleGrid(int HalfDimesion, int GapSize);
	void PlantTrees();
	
};



