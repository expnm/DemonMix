#pragma once

#include "CoreMinimal.h"

namespace Common
{
const int32 DefaultPlayerIndex = 0;
}; // namespace Common

namespace LandscapeGenerator
{
// Смещение шума при расчете Z по перлину
const double RandomNoiseRange = 10000.f;

// Z триггера
const float TriggerHeight = 100000.f;

// Длина луча
const int32 LineTraceLength = 100000;

// Масштаб коробки при удалении лишних инстансов
const int8 InstancesOverlappingBoxScale = 100;

// 100 м
const int32 DefaultMeasurementsSize = 10000;
}; // namespace LandscapeGenerator
