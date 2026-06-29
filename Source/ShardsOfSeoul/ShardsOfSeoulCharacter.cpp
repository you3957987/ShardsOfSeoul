// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShardsOfSeoulCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ShardsOfSeoul.h"
#include <Character/SprintComp.h>

#include "Character/GrappleComp.h"
#include "Character/ShootingComp.h"
#include "Blueprint/UserWidget.h"
#include "UI/InteractionHUDWidget.h"

AShardsOfSeoulCharacter::AShardsOfSeoulCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true; // 웅크리기 기능 활성화

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AShardsOfSeoulCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	// 블루프린트에서 수동 추가한 컴포넌트들을 동적으로 캐싱 연동
	SprintComp = FindComponentByClass<USprintComp>();
	GrappleComp = FindComponentByClass<UGrappleComp>();
	ShootingComp = FindComponentByClass<UShootingComp>();

	// 상시 상호작용 2D HUD 위젯 스폰 및 뷰포트 등록
	if (InteractionHUDClass)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			InteractionHUDInstance = CreateWidget<UInteractionHUDWidget>(PC, InteractionHUDClass);
			if (InteractionHUDInstance)
			{
				InteractionHUDInstance->AddToViewport();
			}
		}
	}
	else
	{
		UE_LOG(LogShardsOfSeoul, Warning, TEXT("[Character] InteractionHUDClass is null! Please assign it in Blueprint."));
	}
}

void AShardsOfSeoulCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AShardsOfSeoulCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AShardsOfSeoulCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AShardsOfSeoulCharacter::Look);
		
		// 런타임 바인딩 시점에도 컴포넌트를 동적으로 재검색 및 검증
		SprintComp = FindComponentByClass<USprintComp>();
		GrappleComp = FindComponentByClass<UGrappleComp>();
		ShootingComp = FindComponentByClass<UShootingComp>();

		if (SprintComp && SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, SprintComp, &USprintComp::StartSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, SprintComp, &USprintComp::StopSprint);
		}
		if (GrappleComp && GrappleAction)
		{
			EnhancedInputComponent->BindAction(GrappleAction, ETriggerEvent::Started, GrappleComp, &UGrappleComp::Grapple);
		}
		if (ShootingComp && AimAction && FireAction)
		{
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, ShootingComp, &UShootingComp::StartAiming);
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, ShootingComp, &UShootingComp::StopAiming);
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, ShootingComp, &UShootingComp::Fire);
		}
	}
	else
	{
		UE_LOG(LogShardsOfSeoul, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AShardsOfSeoulCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AShardsOfSeoulCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AShardsOfSeoulCharacter::DoMove(float Right, float Forward)
{
	// 그래플 준비 중(몽타주 재생 중)이거나 비행 중일 때는 조작 무시
	if (GrappleComp && GrappleComp->IsGrapplingOrPreparing())
	{
		return;
	}

	if (GetController() != nullptr)
	{
		if (IsClimbing)
		{
			// Upward/Downward movement based on forward input when climbing
			AddMovementInput(FVector::UpVector, Forward);
		}
		else
		{
			// find out which way is forward
			const FRotator Rotation = GetController()->GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);

			// get forward vector
			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

			// get right vector 
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			// add movement 
			AddMovementInput(ForwardDirection, Forward);
			AddMovementInput(RightDirection, Right);
		}
	}
}

void AShardsOfSeoulCharacter::SetIsClimbing(bool bNewClimbing)
{
	IsClimbing = bNewClimbing;
}

void AShardsOfSeoulCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AShardsOfSeoulCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AShardsOfSeoulCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
