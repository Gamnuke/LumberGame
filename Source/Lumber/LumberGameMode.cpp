// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumberGameMode.h"
#include "LumberHUD.h"
#include "LumberCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Tree.h"

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

void ALumberGameMode::Tick(float DeltaSeconds) {
	Super::Tick(DeltaSeconds);
	CurrentTime += DeltaSeconds;

	if (Started == false && CurrentTime > 3) {
		Started = true;

		//PlantTrees();
	}

	Cast<ATree::BuildTreeMeshRecursive()>TaskQueue.Pop()();
}

void ALumberGameMode::PlantTrees() {
	// return if there are no trees to spawn
	if (TreeClasses.Num() == 0) { return; }

	TArray<FVector> Points = MakeCircleGrid(5, 10000);

	double Time = FPlatformTime::Seconds();

	for (int i = 0; i < Points.Num(); i++)
	{
		FHitResult Hit;
		GetWorld()->LineTraceSingleByChannel(Hit, Points[i] + FVector(0, 0, 1000000), Points[i] - FVector(0, 0, 1000000), ECollisionChannel::ECC_WorldStatic);

		if (Hit.bBlockingHit) {
			GEngine->AddOnScreenDebugMessage(FMath::Rand(), 5, FColor::Cyan, FString("Spawned Tree"));
			ATree* NewTree = GetWorld()->SpawnActor<ATree>(TreeClasses[0], Hit.ImpactPoint, FRotator());
			NewTree->Seed = FMath::Rand();
			NewTree->FirstTree = true;
			NewTree->StartGeneration();
		}
	}

	Time = FPlatformTime::Seconds() - Time;
	GEngine->AddOnScreenDebugMessage(FMath::Rand(), 5, FColor::Green, FString() + "Took " + FString::SanitizeFloat(Time) + " seconds to make tree");
}

void ALumberGameMode::BeginPlay() {
	Super::BeginPlay();
}

TArray<FVector> ALumberGameMode::MakeCircleGrid(int HalfDimesion, int GapSize) {
	/* Make a circle of points as a grid */
	TArray<FVector> Points;

	// makes an array of points, 2 * halfdimesion rows and columns, that are only in a circle (less than halfdimesnion * gapsize)
	for (int x = -HalfDimesion; x < HalfDimesion; x++)
	{
		for (int y = -HalfDimesion; y < HalfDimesion; y++)
		{
			FVector NewPoint = FVector(x * GapSize, y * GapSize, 0);

			if (NewPoint.Length() <= HalfDimesion * GapSize) {
				Points.Add(NewPoint);
			}
		}
	}

	return Points;
}
