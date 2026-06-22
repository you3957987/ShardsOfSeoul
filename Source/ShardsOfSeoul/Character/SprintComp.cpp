#include "SprintComp.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

USprintComp::USprintComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USprintComp::BeginPlay()
{
	Super::BeginPlay();
	
	// 소유하고 있는 캐릭터와 무브먼트 컴포넌트를 미리 캐싱해둡니다.
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		MovementComponent = OwnerCharacter->GetCharacterMovement();
		if (MovementComponent)
		{
			// 시작할 때 기본 속도를 설정해둡니다.
			MovementComponent->MaxWalkSpeed = WalkSpeed;
		}
	}
}

void USprintComp::StartSprint()
{
	if (MovementComponent && !bIsSprinting)
	{
		bIsSprinting = true;
		MovementComponent->MaxWalkSpeed = SprintSpeed;
	}
}

void USprintComp::StopSprint()
{
	if (MovementComponent && bIsSprinting)
	{
		bIsSprinting = false;
		MovementComponent->MaxWalkSpeed = WalkSpeed;
	}
}
