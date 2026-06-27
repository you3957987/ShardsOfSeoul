#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PlayerDeadInterface.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDeadSignature);

// 인터페이스 클래스 정의
UINTERFACE(MinimalAPI, Blueprintable)
class UPlayerDeadInterface : public UInterface
{
	GENERATED_BODY()
};


class ENEMY_API IPlayerDeadInterface
{
	GENERATED_BODY()

public:
	
	// 구현부(AGSDCharacter)에서 실제 델리게이트 참조를 넘겨주게 됩니다.
	virtual FOnPlayerDeadSignature& ReturnOnPlayerDeadDelegate() = 0;
};

/*
헤더
class ~_API ~Character : public IPlayerDeadInterface
// 로그 관련 인터페이스 함수 구현
UPROPERTY()
FOnPlayerDeadSignature OnPlayerDead;
virtual FOnPlayerDeadSignature& ReturnOnPlayerDeadDelegate() override { return OnPlayerDead; }

CPP - Die() 같은 죽을떄 호출

if ( OnPlayerDead.IsBound() ) OnPlayerDead.Broadcast(); // 죽었다고 알리기
 */
