// Fill out your copyright notice in the Description page of Project Settings.


#include "MyWorld.h"
#include "FileHelpers.h"
#include "JsonUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonSerializerMacros.h"

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
