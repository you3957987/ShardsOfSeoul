#include "EnemyBoss/MagicSwordMan/BossMagicSwordManAIController.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBoss/MagicSwordMan/BossMagicSwordMan.h"

// 깃 추가 확인용
void ABossMagicSwordManAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ControlledEnemy = Cast<ABossMagicSwordMan>(GetPawn());
}

void ABossMagicSwordManAIController::SetBlackboardKey()
{
	Super::SetBlackboardKey();
	
	if ( BlackboardComp == nullptr || ControlledEnemy == nullptr ) return;

	BlackboardComp->SetValueAsBool("CanAttack", true);
	BlackboardComp->SetValueAsFloat("AttackDelay", ControlledEnemy->AttackDelay);
	BlackboardComp->SetValueAsObject(TEXT("TargetCharacter"), TargetPawn);
	
	BlackboardComp->SetValueAsFloat("W_CloseAttack", ControlledEnemy->AttackStruct.CloseAttackWeight);
	BlackboardComp->SetValueAsFloat("W_DashAttack", ControlledEnemy->AttackStruct.DashAttackWeight);
	BlackboardComp->SetValueAsBool("bAirAttack", false);
	BlackboardComp->SetValueAsFloat("W_CloseJumpUpAttack", ControlledEnemy->AttackStruct.CloseJumpUpAttackWeight);
	BlackboardComp->SetValueAsFloat("W_DashJumpUpAttack", ControlledEnemy->AttackStruct.DashJumpUpAttackWeight);
	BlackboardComp->SetValueAsFloat("W_JumpAttack", ControlledEnemy->AttackStruct.JumpAttackWeight);	
	BlackboardComp->SetValueAsFloat("W_Guard", ControlledEnemy->AttackStruct.GuardWeight);
	BlackboardComp->SetValueAsFloat("W_PowerAttack", ControlledEnemy->AttackStruct.PowerAttackWeight);
	BlackboardComp->SetValueAsFloat( "W_BladeWaveAttack", ControlledEnemy->AttackStruct.BladeWaveAttackWeight);
	BlackboardComp->SetValueAsFloat("AttackDelay", ControlledEnemy->AttackStruct.AirAttackDelay);
	BlackboardComp->SetValueAsFloat("W_BackDash", ControlledEnemy->AttackStruct.BackDashWeight);
	
	BlackboardComp->SetValueAsBool("SecondPhase", false);
}
