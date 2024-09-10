// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumberGameMode.h"
#include "LumberHUD.h"
#include "LumberCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Tree.h"
#include "TreeClasses/TreeRoot.h"

ALumberGameMode::ALumberGameMode()
	: Super()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ALumberHUD::StaticClass();
}

void ALumberGameMode::BeginPlay() {
	Super::BeginPlay();
	Points = MakeCircleGrid(5, 2000);
	iPoint = 0;

	nextSpawnTime = 10;
}

void ALumberGameMode::Tick(float DeltaSeconds) {
	Super::Tick(DeltaSeconds);
	CurrentTime += DeltaSeconds;
	if (CurrentTime > nextSpawnTime && nextSpawnTime != -1) {
		nextSpawnTime = -1;
		//StartPlanting();
	}

	/*if (CurrentTime >= nextSpawnTime && nextSpawnTime != -1)
	{
		nextSpawnTime = -1;

		for (FVector Point : Points) {
			PlantTree(Point);
		}
	}*/
	/*if (CurrentTime >= nextSpawnTime && iPoint < Points.Num()) {
		Started = true;

		PlantTree(Points[iPoint]);

		nextSpawnTime = CurrentTime;
		iPoint++;
	}*/

	//Cast<ATree::BuildTreeMeshRecursive()>TaskQueue.Pop()();
}

void ALumberGameMode::StartPlanting() {
	// return if there are no trees to spawn
	GEngine->AddOnScreenDebugMessage(FMath::Rand(), 5, FColor::Orange, FString("Planting trees"));

	if (TreeClasses.Num() == 0) { return; }
	TArray<ATreeRoot*> NewTrees;

	for (int i = 0; i < Points.Num(); i++)
	{
		FHitResult Hit;
		GetWorld()->LineTraceSingleByChannel(Hit, Points[i] + FVector(0, 0, 1000000), Points[i] - FVector(0, 0, 1000000), ECollisionChannel::ECC_WorldStatic);

		if (Hit.bBlockingHit) {
			ATreeRoot *NewTree = GetWorld()->SpawnActor<ATreeRoot>(TreeRootBlueprintClass, Hit.ImpactPoint, FRotator());
			NewTree->TreeClass = TreeClasses[0];
			NewTrees.Add(NewTree);
		}
	}

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, NewTrees]() {
		for (ATreeRoot* NewTree : NewTrees) {
			NewTree->GenerateTree();
		}
	});
}


void ALumberGameMode::PlantTree(FVector LocationToPlant) {
	

}

TArray<FVector> ALumberGameMode::MakeCircleGrid(int HalfDimesion, int GapSize) {
	/* Make a circle of points as a grid */
	TArray<FVector> newPoints;

	// makes an array of points, 2 * halfdimesion rows and columns, that are only in a circle (less than halfdimesnion * gapsize)
	for (int x = -HalfDimesion; x < HalfDimesion; x++)
	{
		for (int y = -HalfDimesion; y < HalfDimesion; y++)
		{
			FVector Randomness = FVector(FMath::RandRange(-200, 200), FMath::RandRange(-200, 200), 0);
			FVector NewPoint = FVector(x * GapSize, y * GapSize, 0) + Randomness;

			if (NewPoint.Length() <= HalfDimesion * GapSize) {
				newPoints.Add(NewPoint);
			}
		}
	}

	return newPoints;
}
