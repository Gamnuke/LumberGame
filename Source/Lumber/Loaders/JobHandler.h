// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Loader.h"
#include "JobHandler.generated.h"

class ALumberGamemode;
enum EChunkQuality;

UENUM()
enum EJobStatus {
	NotRunning,
	Running,
	Completed
};

USTRUCT()
struct FJobBatch {
	GENERATED_BODY()
	TArray<TFunction<void()>> Jobs;
	EJobStatus JobStatus = EJobStatus::NotRunning;
};
/**
 * 
 */
UCLASS()
class LUMBER_API AJobHandler : public ALoader
{
	GENERATED_BODY()
	
public:
	virtual void BeginPlay();
	virtual void Tick(float DeltaSeconds) override;

public:
	// How many render jobs should be run in order at once, less jobs = faster computation & more lag, more jobs = slower computation & less lag
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int JobsPerBatch = 10;


public:

	void AddJobs(const TArray<TFunction<void()>>& Jobs);

	void AddJob(TFunction<void()> Job);

	void RunJobs();
private:
	// Stores arrays of array of jobs, which gets processed every tick
	TArray<FJobBatch> JobBatches;

};
