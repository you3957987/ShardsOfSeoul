#include "EnemyBoss/Hechi/Hechi.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyProjectile/BaseEnemyProjectile.h"

AHechi::AHechi()
{
	LaserSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LaserSpawnPoint"));
	LaserSpawnPoint->SetupAttachment(RootComponent);
}

void AHechi::BeginPlay()
{
	Super::BeginPlay();
	
	CommonBossLogData.BossID = BossLogId; // 로그 데이터에 보스 ID 기록
	
}

void AHechi::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

float AHechi::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator,
	AActor* DamageCauser)
{
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AHechi::Die()
{
	// 액터가 사라질 때 타이머 청소
	GetWorldTimerManager().ClearTimer(LaserTimerHandle);
	
	Super::Die();
}

UAnimMontage* AHechi::StartLaserAttack()
{
	if ( LaserAttackMontage )
	{
		PlayAnimMontage(LaserAttackMontage);
		
		bFocusPlayerAfterAttack = false;
		
		if ( BlackboardComp )
		{
			BlackboardComp->SetValueAsFloat("AttackDelay", AttackStruct.LaserAttackDelay); // 행동 딜레이 설정
		}
		
		return LaserAttackMontage;
	}
	return nullptr;
}

void AHechi::StartLaser()
{
	UE_LOG(LogTemp, Log, TEXT("Hechi: StartLaser"));
	
	if (LaserSpawnPoint && LaserEffect)
	{
		if (CurrentLaserComp && CurrentLaserComp->IsActive())
		{
			CurrentLaserComp->Deactivate();
			CurrentLaserComp = nullptr;
		}
		CurrentLaserComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			LaserEffect, 
			LaserSpawnPoint, 
			NAME_None, 
			FVector::Zero(),        
			FRotator::ZeroRotator,   
			EAttachLocation::KeepRelativeOffset, 
			true
		);
		
		GetWorldTimerManager().SetTimer(LaserTimerHandle, this, &AHechi::StopLaser, 2.0f, false);
	}
}

void AHechi::StopLaser()
{
	// 타이머 핸들 초기화
	GetWorldTimerManager().ClearTimer(LaserTimerHandle);

	if (CurrentLaserComp)
	{
		CurrentLaserComp->Deactivate();       // 나이아가라 시스템 페이드아웃 (부드럽게 꺼짐)
		CurrentLaserComp = nullptr;
	}
}

void AHechi::StartShootLaserProjectile()
{
	UE_LOG(LogTemp, Log, TEXT("Hechi: Start Shooting Projectiles"));

	// 1. 혹시 이미 사격 중이라면 타이머를 한 번 초기화해서 중복 실행 방지
	GetWorldTimerManager().ClearTimer(ProjectileTimerHandle);

	// 2. 함수가 호출되자마자 첫 발을 즉시 발사합니다.
	FireOneProjectile();

	// 3. ★ 핵심: ProjectileFireRate(예: 0.2초) 마다 FireOneProjectile 함수를 '무한 반복(true)' 호출합니다.
	GetWorldTimerManager().SetTimer(
		ProjectileTimerHandle, 
		this, 
		&AHechi::FireOneProjectile, 
		ProjectileFireRate, 
		true // ◀ true로 설정하여 일정 텀마다 계속 발사되도록 함
	);
}

void AHechi::FireOneProjectile()
{
	// 실제로 발사체를 월드에 스폰하는 로직
	if (LaserProjectileClass && LaserSpawnPoint)
	{
		FVector SpawnLocation = LaserSpawnPoint->GetComponentLocation();
       
		// 1. 컴포넌트의 원래 회전 값을 가져옵니다.
		FRotator SpawnRotation = LaserSpawnPoint->GetComponentRotation();

		// 2. ★ Y축 회전(Pitch) 값에 90도를 더해 각도를 정면으로 들어 올립니다.
		SpawnRotation.Pitch += 90.0f; 

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();

		// 보정된 SpawnRotation을 넣어 발사체를 스폰합니다.
		GetWorld()->SpawnActor<ABaseEnemyProjectile>(
		   LaserProjectileClass, 
		   SpawnLocation, 
		   SpawnRotation, 
		   SpawnParams
		);
	}
}

void AHechi::StopShootLaserProjectole()
{
	UE_LOG(LogTemp, Log, TEXT("Hechi: Stop Shooting Projectiles"));

	// ★ 타이머를 클리어하면 즉시 연사가 멈춥니다.
	GetWorldTimerManager().ClearTimer(ProjectileTimerHandle);
}

#if WITH_EDITOR
void AHechi::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AHechi, bDebugMode))
	{
		if ( bDebugMode == true )
		{

		}
		else
		{

		}
	}
}
#endif