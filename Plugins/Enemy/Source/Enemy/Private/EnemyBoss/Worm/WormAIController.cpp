#include "EnemyBoss/Worm/WormAIController.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBoss/Worm/BossWorm.h"

void AWormAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	ControlledEnemy = Cast<ABossWorm>(GetPawn());
}

void AWormAIController::SetBlackboardKey()
{
	Super::SetBlackboardKey();

	if ( BlackboardComp == nullptr || ControlledEnemy == nullptr ) return;

	BlackboardComp->SetValueAsBool("CanAttack", true);
	BlackboardComp->SetValueAsFloat("AttackDelay", ControlledEnemy->AttackDelay);

	BlackboardComp->SetValueAsObject(TEXT("TargetCharacter"), TargetPawn);
	BlackboardComp->SetValueAsBool( TEXT("bIsBurrowing"), ControlledEnemy->bIsBurrowing);
	BlackboardComp->SetValueAsFloat(TEXT("W_NormalAttack"), ControlledEnemy->AttackStruct.NormalAttackWeight);
	BlackboardComp->SetValueAsFloat(TEXT("W_Burrow"), ControlledEnemy->AttackStruct.BurrowWeight);
	BlackboardComp->SetValueAsFloat(TEXT("W_Unburrow"), ControlledEnemy->AttackStruct.UnBurrowWeight);	
	BlackboardComp->SetValueAsFloat(TEXT("W_RangedAttack"), ControlledEnemy->AttackStruct.RangedAttackWeight);
	BlackboardComp->SetValueAsFloat(TEXT("W_LungeAttack"), ControlledEnemy->AttackStruct.LungeAttackWeight);
	BlackboardComp->SetValueAsFloat(TEXT("W_LinearFireBreath"), ControlledEnemy->AttackStruct.LinearFireBreathWeight);
	BlackboardComp->SetValueAsFloat(TEXT("W_FanFireBreath"), ControlledEnemy->AttackStruct.FanFireBreathWeight);
	BlackboardComp->SetValueAsFloat(TEXT("W_Suction"), ControlledEnemy->AttackStruct.SuctionAttackWeight);
}