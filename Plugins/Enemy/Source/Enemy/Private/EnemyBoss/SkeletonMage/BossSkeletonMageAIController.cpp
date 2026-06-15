#include "EnemyBoss/SkeletonMage/BossSkeletonMageAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBoss/SkeletonMage/BossSkeletonMage.h"


void ABossSkeletonMageAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ControlledEnemy = Cast<ABossSkeletonMage>(GetPawn());
}

void ABossSkeletonMageAIController::SetBlackboardKey()
{
	Super::SetBlackboardKey();

	if ( BlackboardComp == nullptr || ControlledEnemy == nullptr ) return;
	
	BlackboardComp->SetValueAsFloat("W_Teleport", ControlledEnemy->AttackStruct.TeleportWeight);
	BlackboardComp->SetValueAsFloat("W_FireBall", ControlledEnemy->AttackStruct.FireBallWeight);
	BlackboardComp->SetValueAsFloat("W_Summon", ControlledEnemy->AttackStruct.SummonWeight);
	BlackboardComp->SetValueAsFloat("W_GroundAttack", ControlledEnemy->AttackStruct.GroundAttackWeight);
	BlackboardComp->SetValueAsFloat("W_PushTarget", ControlledEnemy->AttackStruct.PushTargetWeight);
	BlackboardComp->SetValueAsFloat("W_GravityAttack", ControlledEnemy->AttackStruct.GravityAttackWeight);

	BlackboardComp->SetValueAsFloat("AttackDelay", ControlledEnemy->AttackDelay);
	BlackboardComp->SetValueAsBool("CanAttack", true);
	BlackboardComp->SetValueAsBool("SecondPhase", false);
}
