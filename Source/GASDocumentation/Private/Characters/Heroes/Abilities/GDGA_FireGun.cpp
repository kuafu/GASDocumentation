// Copyright 2020 Dan Kestranek.


#include "Characters/Heroes/Abilities/GDGA_FireGun.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Characters/Heroes/GDHeroCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"

UGDGA_FireGun::UGDGA_FireGun()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTag Ability1Tag = FGameplayTag::RequestGameplayTag(FName("Ability.Skill.Ability1"));
	AbilityTags.AddTag(Ability1Tag);
	ActivationOwnedTags.AddTag(Ability1Tag);

	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Skill")));

	Range = 1000.0f;
	Damage = 12.0f;

    bInputContinuePressed = false;
    AnimationRate = 1.f;
}

void UGDGA_FireGun::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo * ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData * TriggerEventData)
{
    // 修改为连发的Machine Gun

#if 0
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
#else

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}

    PlayFireAnimation();

#endif

    bInputContinuePressed = true;

    UAbilityTask_WaitInputRelease* WaitInputRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
    WaitInputRelease->OnRelease.AddDynamic(this, &UGDGA_FireGun::OnInputRelease);
    WaitInputRelease->Activate();
}


void UGDGA_FireGun::PlayFireAnimation()
{
	UAnimMontage* MontageToPlay = FireHipMontage;

	if (GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.AimDownSights"))) &&
		!GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.AimDownSights.Removal"))))
	{
		MontageToPlay = FireIronsightsMontage;
	}

	// Play fire montage and wait for event telling us to spawn the projectile
    {
        UGDAT_PlayMontageAndWaitForEvent* AnimTask = UGDAT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(this, NAME_None, MontageToPlay, FGameplayTagContainer(), AnimationRate, NAME_None, false, 1.0f);
        AnimTask->OnBlendOut.AddDynamic(this, &UGDGA_FireGun::OnCompleted);
        AnimTask->OnCompleted.AddDynamic(this, &UGDGA_FireGun::OnCompleted);
        AnimTask->OnInterrupted.AddDynamic(this, &UGDGA_FireGun::OnCancelled);
        AnimTask->OnCancelled.AddDynamic(this, &UGDGA_FireGun::OnCancelled);
        AnimTask->EventReceived.AddDynamic(this, &UGDGA_FireGun::EventReceived);

	// ReadyForActivation() is how you activate the AbilityTask in C++. Blueprint has magic from K2Node_LatentGameplayTaskCall that will automatically call ReadyForActivation().
        AnimTask->ReadyForActivation();
    }

}

void UGDGA_FireGun::OnCancelled(FGameplayTag EventTag, FGameplayEventData EventData)
{
    bInputContinuePressed = false;
    CurrentActorInfo->AbilitySystemComponent->CurrentMontageStop();

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

// 动画结束后继续播放
void UGDGA_FireGun::OnCompleted(FGameplayTag EventTag, FGameplayEventData EventData)
{
    if (bInputContinuePressed)
    {
        //PlayFireAnimation();
    }
    else
    {
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
}

void UGDGA_FireGun::EventReceived(FGameplayTag EventTag, FGameplayEventData EventData)
{
    UE_LOG(LogTemp, Log, TEXT(">>UGDGA_FireGun::EventReceived Tag:%s"), *EventTag.ToString());

	// Montage told us to end the ability before the montage finished playing.
	// Montage was set to continue playing animation even after ability ends so this is okay.
	if (EventTag == FGameplayTag::RequestGameplayTag(FName("Event.Montage.EndAbility")))
	{
        if (!bInputContinuePressed)
        {
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        }
		return;
	}

	// Only spawn projectiles on the Server.
	// Predicting projectiles is an advanced topic not covered in this example.
	if (GetOwningActorFromActorInfo()->GetLocalRole() == ROLE_Authority && EventTag == FGameplayTag::RequestGameplayTag(FName("Event.Montage.SpawnProjectile")))
	{
		AGDHeroCharacter* Hero = Cast<AGDHeroCharacter>(GetAvatarActorFromActorInfo());
		if (!Hero)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		}

		FVector Start = Hero->GetGunComponent()->GetSocketLocation(FName("Muzzle"));
		FVector End = Hero->GetCameraBoom()->GetComponentLocation() + Hero->GetFollowCamera()->GetForwardVector() * Range;
		FRotator Rotation = UKismetMathLibrary::FindLookAtRotation(Start, End);

		FGameplayEffectSpecHandle DamageEffectSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffect, GetAbilityLevel());
		
		// Pass the damage to the Damage Execution Calculation through a SetByCaller value on the GameplayEffectSpec
		DamageEffectSpecHandle.Data.Get()->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), Damage);

		FTransform MuzzleTransform = Hero->GetGunComponent()->GetSocketTransform(FName("Muzzle"));
		MuzzleTransform.SetRotation(Rotation.Quaternion());
		MuzzleTransform.SetScale3D(FVector(1.0f));

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AGDProjectile* Projectile = GetWorld()->SpawnActorDeferred<AGDProjectile>(ProjectileClass, MuzzleTransform, GetOwningActorFromActorInfo(),
			Hero, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Projectile->DamageEffectSpecHandle = DamageEffectSpecHandle;
		Projectile->Range = Range;
		Projectile->FinishSpawning(MuzzleTransform);
	}
}

void UGDGA_FireGun::OnInputRelease(float TimeHeld)
{
    CurrentActorInfo->AbilitySystemComponent->CurrentMontageStop();
    bInputContinuePressed = false;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
