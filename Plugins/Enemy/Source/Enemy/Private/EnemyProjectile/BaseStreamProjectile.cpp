#include "EnemyProjectile/BaseStreamProjectile.h"

#include "EnemyLogManager.h"
#include "NiagaraComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ABaseStreamProjectile::ABaseStreamProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	RootComponent = CollisionComp;
	// 오버랩 이벤트 생성 활성화
	CollisionComp->SetGenerateOverlapEvents(true);
	// 콜리전 프리셋: Custom, 활성화: Query and Physics
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 오브젝트 타입: WorldDynamic
	CollisionComp->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	// 모든 채널에 대한 기본 반응을 '무시(Ignore)'로 설정
	CollisionComp->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	
	// 특정 채널에 대한 반응을 '블록(Block)'으로 설정
	CollisionComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Overlap);
	CollisionComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	CollisionComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);
	CollisionComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Overlap);
	
	// 나이아가라 이펙트 컴포넌트 생성
	NiagaraEffectComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraEffectComp"));
	NiagaraEffectComp->SetupAttachment(RootComponent); 
	
	// 투사체 이동 컴포넌트 생성
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComp; // 이동을 적용할 컴포넌트 설정
	ProjectileMovement->bRotationFollowsVelocity = true; // 발사체가 날아가는 방향으로 회전
	ProjectileMovement->ProjectileGravityScale = 0.0f; // 중력 영향을 받지 않도록 설정

	ProjectileMovement->InitialSpeed = ProjectileSpeed; // 초기 속도 설정
	ProjectileMovement->MaxSpeed = ProjectileSpeed; // 최대 속도 설정
	
	Tags.Add(FName("EnemyProjectile"));
}

void ABaseStreamProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	GetWorld()->GetTimerManager().SetTimer(DurationTimerHandle, this, 
		&ABaseStreamProjectile::DeactivateZone, Duration, false);
	
	if (CollisionComp)
	{
		CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ABaseStreamProjectile::OnOverlapBegin);
	}
}

void ABaseStreamProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABaseStreamProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 자기 자신, 소유자, 다른 적 투사체, 또는 다른 적 캐릭터와 충돌한 경우 무시합니다.
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner() 
	 || OtherActor->ActorHasTag(FName("EnemyProjectile")) 
	 || OtherActor->ActorHasTag(FName("Enemy"))
	 || OtherActor->ActorHasTag(FName("Pet")) ) // 이 줄을 추가하세요.
	{
		return;
	}
	
	const bool bIsPlayer = OtherActor->ActorHasTag(FName("Player"));
	
	if ( bIsPlayer )
	{
		// 대미지 로그
		UE_LOG(LogTemp, Warning, TEXT("Stream Projectile %f hit Player: %s"), Damage , *OtherActor->GetName());
		
		UEnemyLogManager::EnemyLog(EEnemyLogType::Worm, FString::Printf(TEXT("[웜] 화염 방사 | 대미지[%.f]"), Damage));
		
		UGameplayStatics::ApplyDamage(
		   OtherActor,
		   Damage, // 헤더 파일에 선언된 대미지 변수
		   GetOwner() ? GetOwner()->GetInstigatorController() : nullptr,
		   this,
		   UDamageType::StaticClass()
		  );
		
		// 대미지 한번 주고 CollisionComp 콜리전 끄기
		if (CollisionComp) CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABaseStreamProjectile::DeactivateZone()
{
	//  나이아가라 이펙트의 신규 생성을 중지 (이미 생성된 파티클은 자연스럽게 남음)
	if (NiagaraEffectComp)
	{
		NiagaraEffectComp->Deactivate();
	}

	// 기존 파티클이 사라질 충분한 시간을 주고 삭제
	SetLifeSpan(2.0f); 
}

#if WITH_EDITOR
void ABaseStreamProjectile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// AttackRange 프로퍼티가 변경되었는지 확인합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseStreamProjectile, ProjectileSpeed))
	{
		if (ProjectileMovement)
		{
			// AttackRangeSphere의 반지름을 AttackRange 값으로 설정합니다.
			ProjectileMovement->InitialSpeed = ProjectileSpeed;
			ProjectileMovement->MaxSpeed = ProjectileSpeed;
		}
	}
}
#endif