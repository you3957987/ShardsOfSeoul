#pragma once

#include "CoreMinimal.h"
#include "PetType.generated.h"

// 펫의 위치랑 매칭시킬 펫의 상태 열거형
UENUM(BlueprintType)
enum class EPetType : uint8
{
	EPT_CuteWhale UMETA(DisplayName = "CuteWhale"),

	EPT_MAX UMETA(DisplayName = "Default") // 최대값, 추가적인 값을 위한 공간 헤더 
};