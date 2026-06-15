#include "EnemyBoss/BlackKnight/BossBlackKnightAIController.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBoss/BlackKnight/BossBlackKnight.h"

void ABossBlackKnightAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ControlledEnemy = Cast<ABossBlackKnight>(GetPawn());
}

void ABossBlackKnightAIController::SetBlackboardKey()
{
	Super::SetBlackboardKey();

	if ( BlackboardComp == nullptr || ControlledEnemy == nullptr ) return;

	BlackboardComp->SetValueAsBool("CanAttack", true);
	BlackboardComp->SetValueAsFloat("AttackDelay", ControlledEnemy->AttackDelay);

	BlackboardComp->SetValueAsObject(TEXT("TargetCharacter"), TargetPawn);
	BlackboardComp->SetValueAsFloat(TEXT("W_RushAttack"), ControlledEnemy->AttackStruct.RushAttackWeight);
	BlackboardComp->SetValueAsFloat(TEXT("W_Guard"), ControlledEnemy->AttackStruct.GuardWeight);
	BlackboardComp->SetValueAsFloat( TEXT("W_NormalAttack"), ControlledEnemy->AttackStruct.NormalAttackWeight );
	BlackboardComp->SetValueAsFloat( TEXT("W_ChargeAttack"), ControlledEnemy->AttackStruct.ChargeAttackWeight);
	BlackboardComp->SetValueAsFloat( TEXT("W_ZapAttack"), ControlledEnemy->AttackStruct.ZapAttackWeight );
}
