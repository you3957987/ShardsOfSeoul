// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionHUDWidget.generated.h"

/**
 * C++ 상호작용 상시 정보 HUD 위젯
 */
UCLASS()
class SHARDSOFSEOUL_API UInteractionHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// C++ 단에서 2D 위치와 텍스트를 직접 덮어씌워 갱신하는 상호작용 처리 함수
	void UpdateTargetUI(FVector2D ScreenPos, const FText& DisplayText);

	// 상호작용 UI 숨김 처리 함수
	void HideTargetUI();

protected:
	// meta = (BindWidget)으로 지정하면 블루프린트 UI 내 동일한 이름의 컴포넌트와 자동 연동됩니다.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI")
	class UCanvasPanel* TargetInfoBox;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI")
	class UTextBlock* DescriptionText;
};
