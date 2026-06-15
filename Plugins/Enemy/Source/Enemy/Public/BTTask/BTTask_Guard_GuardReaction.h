// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_Guard_GuardReaction.generated.h"

/**
 * 
 */
UCLASS()
class ENEMY_API UBTTask_Guard_GuardReaction : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
public:
	UBTTask_Guard_GuardReaction();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
