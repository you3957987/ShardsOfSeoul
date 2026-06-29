// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/InteractionHUDWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

void UInteractionHUDWidget::UpdateTargetUI(FVector2D ScreenPos, const FText& DisplayText)
{
	if (TargetInfoBox)
	{
		// 가시성 활성화
		TargetInfoBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// 캔버스 슬롯을 얻어와 2D 화면 픽셀 좌표 지정
		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(TargetInfoBox->Slot);
		if (CanvasSlot)
		{
			CanvasSlot->SetPosition(ScreenPos);
		}
	}

	if (DescriptionText)
	{
		DescriptionText->SetText(DisplayText);
	}
}

void UInteractionHUDWidget::HideTargetUI()
{
	if (TargetInfoBox)
	{
		// UI 박스 비활성화 (Collapsed)
		TargetInfoBox->SetVisibility(ESlateVisibility::Collapsed);
	}
}
