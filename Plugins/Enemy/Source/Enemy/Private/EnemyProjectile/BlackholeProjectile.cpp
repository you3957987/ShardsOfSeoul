#include "EnemyProjectile/BlackholeProjectile.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"

ABlackholeProjectile::ABlackholeProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	RootComponent = SceneComponent;
	
	ParticleSystemComp = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleSystemComp"));
	ParticleSystemComp->SetupAttachment(RootComponent);
	
}

void ABlackholeProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	// 2. 유지시간 타이머 세팅: LifeTime 초가 지나면 OnBlackholeExpired 함수를 호출
	GetWorldTimerManager().SetTimer(
		BlackholeEndTimerHandle, 
		this, 
		&ABlackholeProjectile::BlackholeEnd, 
		LifeTime, 
		false // 반복 없음 (1회성)
	);
}

void ABlackholeProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    
	if (BlackHoleFlag == false) return;
    
	FVector BlackholeLocation = GetActorLocation();

	// 월드 내 플레이어 가져오기
	ACharacter* TargetCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (!TargetCharacter || !TargetCharacter->ActorHasTag(FName("Player"))) return;
    
	// 1. 방향 및 거리 계산
	FVector ToBlackhole = BlackholeLocation - TargetCharacter->GetActorLocation();
    
	// [수정] 거리 제한(MaxRadius) 체크를 완전히 제거하여 거리가 무한대가 됩니다.
	// 단, 나중에 거리에 따른 연출 등이 필요할 수 있으니 DistanceToTarget 변수만 남겨둡니다.
	float DistanceToTarget = ToBlackhole.Size(); 

	ToBlackhole.Normalize();
    
	// 2. 플레이어 저항력 계산
	FVector PlayerForward = TargetCharacter->GetActorForwardVector();
	float Resistance = FVector::DotProduct(ToBlackhole, PlayerForward);
    
	bool bIsMovingInput = false;
	UCharacterMovementComponent* Movement = TargetCharacter->GetCharacterMovement();
	if (Movement)
	{
		bIsMovingInput = Movement->GetCurrentAcceleration().SizeSquared() > 1.0f;
	}
    
	float FinalForceStrength = PullStrength; 
	float AbsoluteForce = FMath::Abs(FinalForceStrength); 

	if (Resistance < 0.0f && bIsMovingInput) 
	{
		AbsoluteForce *= 0.8f; // 도망칠 때 힘 감소
	}
    
	// 3. 플레이어에게만 직접 중력(힘) 적용하기
	if (Movement)
	{
		// ToBlackhole 방향으로 항상 일정한 크기(AbsoluteForce)의 힘을 가합니다.
		FVector ForceVector = ToBlackhole * AbsoluteForce;
        
		Movement->AddForce(ForceVector);
	}
}

void ABlackholeProjectile::BlackholeEnd()
{
	if (ParticleSystemComp)
	{
		// 캐스케이드 파티클 연출 멈추기 (이미 뿜어져 나온 입자들은 3초 동안 자연스럽게 사라짐)
		ParticleSystemComp->DeactivateSystem();
	}
	
	BlackHoleFlag = false;
    
	// [수정] 3초 후에 안전하게 ConditionalDestroy를 호출합니다.
	GetWorldTimerManager().SetTimer(
		DestroyTimerHandle,
		[this]() { this->Destroy(); }, // ◀ 람다식을 이용해 내부에서 안전하게 Destroy 호출
		3.0f,
		false
	);
}

#if WITH_EDITOR
void ABlackholeProjectile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
}
#endif

