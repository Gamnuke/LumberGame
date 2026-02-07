// Fill out your copyright notice in the Description page of Project Settings.


#include "SerializableObject.h"

// Add default functionality here for any ISerializableObject functions that are not pure virtual.

TSharedPtr<FJsonObject> ISerializableObject::SerializeObject()
{
	return TSharedPtr<FJsonObject>();
}

void ISerializableObject::DeserializeAndLoadObject(TSharedPtr<FJsonObject> ObjectData)
{
}
