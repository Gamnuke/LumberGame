// Fill out your copyright notice in the Description page of Project Settings.


#include "MyWorld.h"

MyWorld::MyWorld()
{
}

FJsonObject MyWorld::SerializeObject()
{
	TSharedPtr<FJsonObject> newJsonObject = MakeShareable(new FJsonObject());
	// Serialize world

	return FJsonObject();
}

void MyWorld::DeserializeAndLoadObject(FJsonObject ObjectData)
{
}
