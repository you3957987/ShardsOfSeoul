#pragma once

#include "CoreMinimal.h"
#include "PetState.generated.h"

// 펫의 위치랑 매칭시킬 펫의 상태 열거형
UENUM(BlueprintType)
enum class EPetState : uint8
{
	EPS_Follow UMETA(DisplayName = "Follow"),
	EPS_Battle UMETA(DisplayName = "Battle"),
	EPS_BossBattle UMETA(DisplayName = "Boss Battle"),
	EPS_Conversation UMETA(DisplayName = "Conversation"),
	
	EPT_MAX UMETA(DisplayName = "Default") // 최대값, 추가적인 값을 위한 공간 헤더
};
