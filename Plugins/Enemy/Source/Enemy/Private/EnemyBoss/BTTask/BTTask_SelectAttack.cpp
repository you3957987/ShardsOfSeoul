#include "EnemyBoss/BTTask/BTTask_SelectAttack.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBoss/BaseBossEnemy.h"

UBTTask_SelectAttack::UBTTask_SelectAttack()
{
	NodeName = "Select Attack";

	// FName으로만 사용 가능
	BlackboardKey.AddNameFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SelectAttack, BlackboardKey));
}

EBTNodeResult::Type UBTTask_SelectAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AttackPatterns.Num() == 0)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if ( BlackboardComp == nullptr ) return EBTNodeResult::Failed;

	float TotalWeight = 0.0f;
	TArray<float> ResolvedWeights; // 블랙보드에서 가져온 가중치를 저장할 임시 배열
	
	for (const FAttackPattern& Pattern : AttackPatterns)
	{
		// WeightKey를 사용해 블랙보드에서 float 값을 가져옵니다.
		const float Weight = BlackboardComp->GetValueAsFloat(Pattern.WeightKey.SelectedKeyName);
		ResolvedWeights.Add(Weight);
		TotalWeight += Weight;
	}

	if (TotalWeight <= 0.0f)
	{
		return EBTNodeResult::Failed;
	}

	const float RandomWeight = FMath::FRandRange(0.0f, TotalWeight);

	float CurrentWeightSum = 0.0f;
	FName SelectedAttackName = NAME_None;

	for (int32 i = 0; i < AttackPatterns.Num(); ++i)
	{
		CurrentWeightSum += ResolvedWeights[i];
		if (RandomWeight <= CurrentWeightSum)
		{
			SelectedAttackName = AttackPatterns[i].AttackName;
			break;
		}
	}
	
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;

	APawn* ControlledPawn = AIController->GetPawn();
	if (!ControlledPawn) return EBTNodeResult::Failed;

	ABaseBossEnemy* Boss = Cast<ABaseBossEnemy>(ControlledPawn);
	
	if (SelectedAttackName != NAME_None)
	{
		BlackboardComp->SetValueAsName(GetSelectedBlackboardKey(), SelectedAttackName);
		
		if ( Boss )
		{
			Boss->AttackPatternLog(SelectedAttackName.ToString());
		}
		
		return EBTNodeResult::Succeeded;
	}
	
	return EBTNodeResult::Failed;
}
