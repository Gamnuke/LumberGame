// Fill out your copyright notice in the Description page of Project Settings.


#include "JobHandler.h"

// Called when the game starts or when spawned
void AJobHandler::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AJobHandler::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

/*
Add jobs to each batch without exeeding JobsPerBatch variable
*/
void AJobHandler::AddJobs(const TArray<TFunction<void()>>& Jobs) {
	int NextJobToAdd = 0;
	int JobBatchIndex = 0;

	for (int i = 0; i < Jobs.Num(); i++)
	{
		AddJob(Jobs[i]);
	}
}

void AJobHandler::AddJob(TFunction<void()> Job) {
	for (int i = 0; i < JobBatches.Num(); i++)
	{
		if (JobBatches[i].JobStatus == EJobStatus::NotRunning && JobBatches[i].Jobs.Num() < JobsPerBatch) {
			JobBatches[i].Jobs.Add(Job);
			return;
		}
	}

	FJobBatch NewBatch;
	NewBatch.Jobs.Add(Job);
	JobBatches.Add(NewBatch);
}

/*
Runs each batch of jobs on its seperate thread
*/
void AJobHandler::RunJobs() {
	for (int i = 0; i < JobBatches.Num(); i++)
	{
		if (JobBatches[i].JobStatus == EJobStatus::NotRunning) {
			JobBatches[i].JobStatus = EJobStatus::Running;

			AsyncTask(BackgroundPriority, [this, i]() {
				for (int j = 0; j < JobBatches[i].Jobs.Num(); j++)
				{
					// Run the job in this batch
					JobBatches[i].Jobs[j]();
				}
				JobBatches[i].Jobs.Empty();
				JobBatches[i].JobStatus = EJobStatus::NotRunning;
				});
		}
	}
}