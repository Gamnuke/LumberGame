// Fill out your copyright notice in the Description page of Project Settings.


#include "SerializableObject.h"

// Add default functionality here for any ISerializableObject functions that are not pure virtual.

FJsonObject ISerializableObject::SerializeObject()
{
	return FJsonObject();
}

void ISerializableObject::DeserializeAndLoadObject(FJsonObject ObjectData)
{
}
