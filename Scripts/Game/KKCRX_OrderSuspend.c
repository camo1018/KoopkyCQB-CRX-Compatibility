// Pauses the CRX group and soldier settings that walk men off a Clear or Garrison order.
// The saved values are written back when the last such order on that group ends.

class KKCRX_SoldierDanger
{
	SCR_AIInfoComponent m_Info;
	int m_iChance;
}

class KKCRX_GroupSuspend
{
	int m_iDepth;

	CRX_EAIReturnToPositionOriginType m_eReturnToPosition;
	bool m_bInvestigate;
	bool m_bInvestigateBuildingSearch;
	int m_iInCoverSearchChance;
	int m_iCombatMoveChance;
	int m_iCombatCoverChance;
	bool m_bGlobalExclude;

	bool m_bHadConfig;
	CRX_EAIReturnToPositionOriginType m_eConfigReturnToPosition;
	bool m_bConfigInvestigate;
	bool m_bConfigInvestigateBuildingSearch;
	int m_iConfigInCoverSearchChance;
	int m_iConfigCombatMoveChance;
	int m_iConfigCombatCoverChance;
	bool m_bConfigGlobalExclude;
	bool m_bConfigFileExclude;

	ref array<ref KKCRX_SoldierDanger> m_aSoldiers = {};
}

class KKCRX_OrderSuspend
{
	protected static ref map<SCR_AIGroup, ref KKCRX_GroupSuspend> s_mGroups =
		new map<SCR_AIGroup, ref KKCRX_GroupSuspend>();

	static bool IsEnabled()
	{
		SCR_BaseGameMode mode = SCR_BaseGameMode.Get();
		if (!mode)
			return true;

		return mode.KKCRX_GetSuspendDuringOrders();
	}

	static bool Suspend(SCR_AIGroup group)
	{
		if (!group || !Replication.IsServer() || !IsEnabled())
			return false;

		SCR_AIGroupInfoComponent groupInfo = GetGroupInfo(group);
		if (!groupInfo)
			return false;

		KKCRX_GroupSuspend state = s_mGroups.Get(group);
		if (!state)
		{
			state = new KKCRX_GroupSuspend();
			Snapshot(group, groupInfo, state);
			s_mGroups.Set(group, state);
		}

		state.m_iDepth++;
		ApplyLimits(group, groupInfo);
		CaptureSoldiers(group, state);

		if (state.m_iDepth == 1)
		{
			PrintFormat(
				"KKCRX: Paused CRX movement settings for %1",
				group
			);
		}

		return true;
	}

	static void CaptureLateSoldiers(SCR_AIGroup group)
	{
		if (!group || !Replication.IsServer())
			return;

		KKCRX_GroupSuspend state = s_mGroups.Get(group);
		if (!state)
			return;

		CaptureSoldiers(group, state);
	}

	static void Resume(SCR_AIGroup group)
	{
		if (!group || !Replication.IsServer())
			return;

		KKCRX_GroupSuspend state = s_mGroups.Get(group);
		if (!state)
			return;

		state.m_iDepth--;
		if (state.m_iDepth > 0)
			return;

		Restore(group, state);
		s_mGroups.Remove(group);

		PrintFormat(
			"KKCRX: Restored CRX movement settings for %1",
			group
		);
	}

	protected static SCR_AIGroupInfoComponent GetGroupInfo(SCR_AIGroup group)
	{
		SCR_AIGroupInfoComponent groupInfo = group.GetGroupInfoComponent();
		if (groupInfo)
			return groupInfo;

		return SCR_AIGroupInfoComponent.Cast(
			group.FindComponent(SCR_AIGroupInfoComponent)
		);
	}

	protected static SCR_AIConfigComponent GetConfig(SCR_AIGroup group)
	{
		return SCR_AIConfigComponent.Cast(
			group.FindComponent(SCR_AIConfigComponent)
		);
	}

	protected static void Snapshot(
		SCR_AIGroup group,
		SCR_AIGroupInfoComponent groupInfo,
		KKCRX_GroupSuspend state
	)
	{
		state.m_eReturnToPosition = groupInfo.GetReturnToPositionOriginType();
		state.m_bInvestigate = groupInfo.GetInvestigate();
		state.m_bInvestigateBuildingSearch = groupInfo.GetInvestigateBuildingSearch();
		state.m_iInCoverSearchChance = groupInfo.GetCombatInCoverDynamicCoverSearchChance();
		state.m_iCombatMoveChance = groupInfo.GetCombatMoveChance();
		state.m_iCombatCoverChance = groupInfo.GetCombatCoverChance();
		state.m_bGlobalExclude = groupInfo.GetGlobalSettingsOverrideExclude();

		SCR_AIConfigComponent config = GetConfig(group);
		if (!config)
			return;

		state.m_bHadConfig = true;

		state.m_eConfigReturnToPosition = config.m_eReturnToPositionOriginType;
		state.m_bConfigInvestigate = config.m_bInvestigate;
		state.m_bConfigInvestigateBuildingSearch = config.m_bInvestigateBuildingSearch;
		state.m_iConfigInCoverSearchChance = config.m_iCombatInCoverDynamicCoverSearchChance;
		state.m_iConfigCombatMoveChance = config.m_iCombatMoveChance;
		state.m_iConfigCombatCoverChance = config.m_iCombatCoverChance;
		state.m_bConfigGlobalExclude = config.m_bGlobalSettingsOverrideExclude;
		state.m_bConfigFileExclude = config.m_bConfigFilesSettingsOverrideExclude;
	}

	protected static void ApplyLimits(
		SCR_AIGroup group,
		SCR_AIGroupInfoComponent groupInfo
	)
	{
		groupInfo.SetReturnToPositionOriginType(CRX_EAIReturnToPositionOriginType.NEVER);
		groupInfo.SetInvestigate(false);
		groupInfo.SetInvestigateBuildingSearch(false);
		groupInfo.SetGlobalSettingsOverrideExclude(true);

		SCR_AIConfigComponent config = GetConfig(group);
		if (!config)
			return;

		// A later CRX init copies these fields back onto the group.
		// The exclude flags stop the config file and global override from doing that mid-order.
		config.m_eReturnToPositionOriginType = CRX_EAIReturnToPositionOriginType.NEVER;
		config.m_bInvestigate = false;
		config.m_bInvestigateBuildingSearch = false;
		config.m_bGlobalSettingsOverrideExclude = true;
		config.m_bConfigFilesSettingsOverrideExclude = true;
	}

	protected static void CaptureSoldiers(SCR_AIGroup group, KKCRX_GroupSuspend state)
	{
		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity body = agent.GetControlledEntity();
			if (!body)
				continue;

			SCR_AIInfoComponent info = SCR_AIInfoComponent.Cast(
				body.FindComponent(SCR_AIInfoComponent)
			);
			if (!info || HasSoldier(state, info))
				continue;

			KKCRX_SoldierDanger saved = new KKCRX_SoldierDanger();
			saved.m_Info = info;
			saved.m_iChance = info.GetDangerReactionChance();
			state.m_aSoldiers.Insert(saved);
			info.SetDangerReactionChance(0);
		}
	}

	protected static bool HasSoldier(KKCRX_GroupSuspend state, SCR_AIInfoComponent info)
	{
		foreach (KKCRX_SoldierDanger saved : state.m_aSoldiers)
		{
			if (saved && saved.m_Info == info)
				return true;
		}

		return false;
	}

	protected static void Restore(SCR_AIGroup group, KKCRX_GroupSuspend state)
	{
		SCR_AIConfigComponent config = GetConfig(group);
		if (state.m_bHadConfig && config)
		{
			config.m_eReturnToPositionOriginType = state.m_eConfigReturnToPosition;
			config.m_bInvestigate = state.m_bConfigInvestigate;
			config.m_bInvestigateBuildingSearch = state.m_bConfigInvestigateBuildingSearch;
			config.m_iCombatInCoverDynamicCoverSearchChance = state.m_iConfigInCoverSearchChance;
			config.m_iCombatMoveChance = state.m_iConfigCombatMoveChance;
			config.m_iCombatCoverChance = state.m_iConfigCombatCoverChance;
			config.m_bGlobalSettingsOverrideExclude = state.m_bConfigGlobalExclude;
			config.m_bConfigFilesSettingsOverrideExclude = state.m_bConfigFileExclude;
		}

		SCR_AIGroupInfoComponent groupInfo = GetGroupInfo(group);
		if (groupInfo)
		{
			groupInfo.SetReturnToPositionOriginType(state.m_eReturnToPosition);
			groupInfo.SetInvestigate(state.m_bInvestigate);
			groupInfo.SetInvestigateBuildingSearch(state.m_bInvestigateBuildingSearch);
			groupInfo.SetCombatInCoverDynamicCoverSearchChance(state.m_iInCoverSearchChance);
			groupInfo.SetCombatMoveChance(state.m_iCombatMoveChance);
			groupInfo.SetCombatCoverChance(state.m_iCombatCoverChance);
			groupInfo.SetGlobalSettingsOverrideExclude(state.m_bGlobalExclude);
		}

		foreach (KKCRX_SoldierDanger saved : state.m_aSoldiers)
		{
			if (!saved || !saved.m_Info)
				continue;

			saved.m_Info.SetDangerReactionChance(saved.m_iChance);
		}
	}
}

modded class SCR_BaseGameMode
{
	[Attribute("1", UIWidgets.CheckBox, "While a Clear or Garrison order is running, pause the CRX settings that pull soldiers off that order.", category: "Koopky CQB CRX")]
	protected bool m_bKKCRX_SuspendDuringOrders;

	bool KKCRX_GetSuspendDuringOrders()
	{
		return m_bKKCRX_SuspendDuringOrders;
	}
}

modded class KK_ClearBuildingActivity
{
	protected bool m_bKKCRX_Suspended;

	override void OnActionSelected()
	{
		super.OnActionSelected();
		KKCRX_TrySuspend();
	}

	override float CustomEvaluate()
	{
		float score = super.CustomEvaluate();

		if (m_bKKCRX_Suspended)
			KKCRX_OrderSuspend.CaptureLateSoldiers(m_Group);

		return score;
	}

	override void OnActionDeselected()
	{
		super.OnActionDeselected();
		KKCRX_TryResume();
	}

	override void OnActionFailed()
	{
		super.OnActionFailed();
		KKCRX_TryResume();
	}

	protected void KKCRX_TrySuspend()
	{
		if (
			m_bKKCRX_Suspended ||
			m_bFinished ||
			m_bCancelled ||
			!m_ClearWaypoint
		)
		{
			return;
		}

		if (KKCRX_OrderSuspend.Suspend(m_Group))
			m_bKKCRX_Suspended = true;
	}

	protected void KKCRX_TryResume()
	{
		if (!m_bKKCRX_Suspended)
			return;

		m_bKKCRX_Suspended = false;
		KKCRX_OrderSuspend.Resume(m_Group);
	}
}

modded class KK_GarrisonBuildingActivity
{
	protected bool m_bKKCRX_Suspended;

	override void OnActionSelected()
	{
		super.OnActionSelected();
		KKCRX_TrySuspend();
	}

	override float CustomEvaluate()
	{
		float score = super.CustomEvaluate();

		if (m_bKKCRX_Suspended)
			KKCRX_OrderSuspend.CaptureLateSoldiers(m_Group);

		return score;
	}

	override void OnActionDeselected()
	{
		super.OnActionDeselected();
		KKCRX_TryResume();
	}

	override void OnActionFailed()
	{
		super.OnActionFailed();
		KKCRX_TryResume();
	}

	protected void KKCRX_TrySuspend()
	{
		if (
			m_bKKCRX_Suspended ||
			m_bFinished ||
			m_bCancelled ||
			!m_GarrisonWaypoint
		)
		{
			return;
		}

		if (KKCRX_OrderSuspend.Suspend(m_Group))
			m_bKKCRX_Suspended = true;
	}

	protected void KKCRX_TryResume()
	{
		if (!m_bKKCRX_Suspended)
			return;

		m_bKKCRX_Suspended = false;
		KKCRX_OrderSuspend.Resume(m_Group);
	}
}

modded class KK_PerceptionBoost
{
	static void KKCRX_SetActive(IEntity soldier, bool active)
	{
		if (!soldier)
			return;

		if (active)
			s_ActiveSoldiers.Insert(soldier);
		else
			s_ActiveSoldiers.RemoveItem(soldier);
	}
}

modded class SCR_AICombatComponent
{
	override void UpdatePerceptionFactor(
		PerceptionComponent perceptionComp,
		SCR_AIThreatSystem threatSystem
	)
	{
		IEntity owner = GetOwner();

		// Sprinting past a fight outside the building. Koopky wants no
		// recognition for that stretch, and CRX would put its rates back.
		if (owner && KK_GarrisonHold.IsIgnoringTargets(owner))
		{
			if (perceptionComp)
				perceptionComp.SetPerceptionFactor(0);

			return;
		}

		bool sharpOrder =
			owner &&
			perceptionComp &&
			threatSystem &&
			KK_PerceptionBoost.IsActiveSoldier(owner) &&
			KK_PerceptionBoost.UseSharpCombat();

		// Koopky replaces CRX recognition with the base-game rates while an
		// order is active. Drop that mark for this call so CRX's own rates run.
		// Koopky already stored its recognition-speed slider on this component,
		// and CRX multiplies by that slider.
		if (sharpOrder)
			KK_PerceptionBoost.KKCRX_SetActive(owner, false);

		super.UpdatePerceptionFactor(perceptionComp, threatSystem);

		if (sharpOrder)
			KK_PerceptionBoost.KKCRX_SetActive(owner, true);
	}
}

modded class SCR_AIAttackBehavior
{
	override float CustomEvaluate()
	{
		IEntity character;
		IEntity agentEntity;
		if (m_Utility)
		{
			character = m_Utility.m_OwnerEntity;
			agentEntity = m_Utility.GetOwner();
		}

		// An empty gun cannot take the shot. CRX would still score the attack,
		// and that holds the reload off.
		if (
			(KK_GarrisonHold.HasBuilding(character) && KK_GarrisonHold.CannotShoot(character)) ||
			(KK_GarrisonHold.HasBuilding(agentEntity) && KK_GarrisonHold.CannotShoot(agentEntity))
		)
		{
			return 0;
		}

		float score = super.CustomEvaluate();

		// CRX calls the base attack evaluate directly, so Koopky's flags
		// never land when CRX sits between this addon and Koopky.
		bool pinned =
			KK_GarrisonHold.IsPinned(character) ||
			KK_GarrisonHold.IsPinned(agentEntity);
		bool doorFiring =
			KK_GarrisonHold.IsDoorFiring(character) ||
			KK_GarrisonHold.IsDoorFiring(agentEntity);

		vector approachGoal;
		bool steering =
			KK_GarrisonHold.GetApproachGoal(character, approachGoal) ||
			KK_GarrisonHold.GetApproachGoal(agentEntity, approachGoal);

		if (pinned || doorFiring)
		{
			m_bUseCombatMove = false;
			if (KK_GarrisonHold.IsPinned(character))
				KK_GarrisonHold.SetPinned(character, true);
			if (KK_GarrisonHold.IsPinned(agentEntity))
				KK_GarrisonHold.SetPinned(agentEntity, true);
		}
		else if (steering)
		{
			m_bUseCombatMove = true;
		}

		return score;
	}

	override void InitWaitTime(SCR_AIUtilityComponent utility)
	{
		// A shot this addon owns uses Koopky's delay, not CRX's reaction wait.
		if (utility && KK_GarrisonHold.OwnsShot(utility.m_OwnerEntity))
		{
			m_fWaitTime.m_Value = KK_GarrisonHold.ShotDelaySeconds();
			return;
		}

		super.InitWaitTime(utility);

		if (
			!utility ||
			!KK_PerceptionBoost.IsActiveSoldier(utility.m_OwnerEntity) ||
			!KK_PerceptionBoost.UseSharpCombat()
		)
		{
			return;
		}

		// CRX calls the base wait directly, so Koopky's zero never lands.
		m_fWaitTime.m_Value = 0;
	}
}

modded class SCR_AIMoveIndividuallyBehavior
{
	override float CustomEvaluate()
	{
		IEntity body;
		if (m_Utility)
			body = m_Utility.m_OwnerEntity;

		if (!KK_GarrisonHold.IsPinned(body))
			return super.CustomEvaluate();

		// CRX raises this move to attack priority and sprints when threatened.
		if (m_CharacterMovementComponent)
			m_CharacterMovementComponent.SetMovementTypeWanted(EMovementType.IDLE);

		return 0;
	}
}

modded class SCR_AICombatMoveLogicBase
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		IEntity body;
		if (owner)
			body = owner.GetControlledEntity();

		if (KK_GarrisonHold.IsPinned(body))
		{
			if (m_State && m_State.IsExecutingRequest())
				m_State.CancelRequest();

			return ENodeResult.RUNNING;
		}

		vector goal;
		if (m_Utility && KK_GarrisonHold.GetApproachGoal(body, goal))
		{
			SCR_AIBehaviorBase executed =
				SCR_AIBehaviorBase.Cast(m_Utility.GetExecutedAction());

			if (executed && executed.m_bUseCombatMove)
			{
				vector aimPos = goal;
				if (m_CombatComp)
				{
					BaseTarget target = m_CombatComp.GetCurrentTarget();
					if (target)
					{
						IEntity targetEntity = target.GetTargetEntity();
						if (targetEntity)
							aimPos = targetEntity.GetOrigin();
						else
							aimPos = target.GetLastSeenPosition();
					}
				}

				KK_GarrisonHold.SteerToward(m_Utility, goal, aimPos);
				return ENodeResult.RUNNING;
			}
		}

		return super.EOnTaskSimulate(owner, dt);
	}

	protected override bool SuppressedInCoverCondition()
	{
		if (KKCRX_IsHoldingPost(m_Entity) || KKCRX_IsHoldingPost(m_ControlledEntity))
			return false;

		return super.SuppressedInCoverCondition();
	}
}

modded class SCR_AIDangerReaction_ProjectileHit
{
	override bool PerformReaction(notnull SCR_AIUtilityComponent utility, notnull SCR_AIThreatSystem threatSystem, AIDangerEvent dangerEvent, int dangerEventCount)
	{
		if (KKCRX_IsHoldingPost(utility.m_OwnerEntity))
			return true;

		return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);
	}
}

modded class SCR_AIDangerReaction_DamageTaken
{
	override bool PerformReaction(notnull SCR_AIUtilityComponent utility, notnull SCR_AIThreatSystem threatSystem, AIDangerEvent dangerEvent, int dangerEventCount)
	{
		if (KKCRX_IsHoldingPost(utility.m_OwnerEntity))
			return true;

		return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);
	}
}

modded class SCR_AIDangerReaction_Explosion
{
	override bool PerformReaction(notnull SCR_AIUtilityComponent utility, notnull SCR_AIThreatSystem threatSystem, AIDangerEvent dangerEvent, int dangerEventCount)
	{
		if (KKCRX_IsHoldingPost(utility.m_OwnerEntity))
			return true;

		return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);
	}
}

bool KKCRX_IsHoldingPost(IEntity soldier)
{
	return KK_GarrisonHold.IsPinned(soldier);
}

modded class SCR_AIRetreatWhileLookAtBehavior
{
	override float CustomEvaluate()
	{
		if (
			m_Utility &&
			(
				KK_GarrisonHold.IsDoorFiring(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsDoorFiring(m_Utility.GetOwner())
			)
		)
		{
			return 0;
		}

		return super.CustomEvaluate();
	}
}

modded class SCR_AIThreatSystem
{
	override void Update(SCR_AIUtilityComponent utility, float timeSlice)
	{
		if (
			m_Utility &&
			(
				KK_GarrisonHold.IsIgnoringTargets(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsIgnoringTargets(m_Utility.GetOwner())
			)
		)
		{
			if (m_Agent && m_Agent.GetDangerEventsCount() > 0)
				m_Agent.ClearDangerEvents(m_Agent.GetDangerEventsCount() + 1);

			KK_IgnoreForSprint();
			return;
		}

		super.Update(utility, timeSlice);
	}

	override void ThreatBulletImpact(int count)
	{
		if (KKCRX_SprintIgnoring())
			return;

		super.ThreatBulletImpact(count);
	}

	override void ThreatProjectileFlyby(int count)
	{
		if (KKCRX_SprintIgnoring())
			return;

		super.ThreatProjectileFlyby(count);
	}

	protected bool KKCRX_SprintIgnoring()
	{
		return m_Utility &&
			(
				KK_GarrisonHold.IsIgnoringTargets(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsIgnoringTargets(m_Utility.GetOwner())
			);
	}
}

modded class SCR_AIUtilityComponent
{
	override SCR_AIBehaviorBase EvaluateBehavior(BaseTarget unknownTarget)
	{
		bool ignore =
			KK_GarrisonHold.IsIgnoringTargets(m_OwnerEntity) ||
			KK_GarrisonHold.IsIgnoringTargets(GetOwner());

		if (ignore && m_CombatComponent)
			m_CombatComponent.KK_ClearTarget();

		SCR_AIBehaviorBase result;
		if (ignore)
			result = super.EvaluateBehavior(null);
		else
			result = super.EvaluateBehavior(unknownTarget);

		if (ignore)
			KK_GarrisonHold.EnforceSprintIgnore(this);
		else if (KK_GarrisonHold.OwnsShot(m_OwnerEntity) || KK_GarrisonHold.OwnsShot(GetOwner()))
			KK_GarrisonHold.ApplyRoomShot(this);
		else if (
			KK_GarrisonHold.IsMoveFire(m_OwnerEntity) ||
			KK_GarrisonHold.IsMoveFire(GetOwner()) ||
			KK_GarrisonHold.IsRoomFire(m_OwnerEntity) ||
			KK_GarrisonHold.IsRoomFire(GetOwner())
		)
			KK_GarrisonHold.ApplyMoveFire(this);

		if (!ignore)
		{
			IEntity soldier = m_OwnerEntity;
			if (!soldier)
				soldier = GetOwner();

			KK_GarrisonHold.ConsiderTopOff(soldier);
		}

		return result;
	}
}

modded class SCR_AISetWeaponRaised
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		IEntity body;
		if (owner)
			body = owner.GetControlledEntity();

		if (KK_GarrisonHold.IsIgnoringTargets(body) || KK_GarrisonHold.IsIgnoringTargets(owner))
		{
			if (body)
			{
				CharacterControllerComponent controller =
					CharacterControllerComponent.Cast(
						body.FindComponent(CharacterControllerComponent)
					);

				if (controller)
					controller.SetWeaponRaised(false);
			}

			return ENodeResult.SUCCESS;
		}

		if (KK_GarrisonHold.OwnsShot(body) || KK_GarrisonHold.OwnsShot(owner))
		{
			SCR_ChimeraAIAgent roomSoldier = SCR_ChimeraAIAgent.Cast(owner);
			if (roomSoldier)
				KK_GarrisonHold.ApplyRoomShot(roomSoldier.m_UtilityComponent);

			return ENodeResult.SUCCESS;
		}

		if (
			KK_GarrisonHold.IsMoveFire(body) ||
			KK_GarrisonHold.IsMoveFire(owner) ||
			KK_GarrisonHold.IsRoomFire(body) ||
			KK_GarrisonHold.IsRoomFire(owner)
		)
		{
			SCR_ChimeraAIAgent soldier = SCR_ChimeraAIAgent.Cast(owner);
			if (soldier)
				KK_GarrisonHold.ApplyMoveFire(soldier.m_UtilityComponent);

			return ENodeResult.SUCCESS;
		}

		// The move order raises on a loop. That raise restarts a reload.
		if (KK_GarrisonHold.IsQuietReload(body) || KK_GarrisonHold.IsQuietReload(owner))
			return ENodeResult.SUCCESS;

		return super.EOnTaskSimulate(owner, dt);
	}
}

modded class SCR_AIUpdateTargetAttackData
{
	override int ResolveFireTree(
		BaseTarget target,
		bool visible,
		bool weaponReady,
		out float fireRate)
	{
		IEntity body;
		if (m_CharacterControllerComponent)
			body = m_CharacterControllerComponent.GetOwner();

		// The attack node has to run. It is what points the gun at the target
		// and brings it up. CRX then drops the gun whenever the fire tree has
		// no shot, which is the lower-and-raise while the room gun owns it.
		if (KK_GarrisonHold.CombatOwnsWeapon(body))
			return vanilla.ResolveFireTree(target, visible, weaponReady, fireRate);

		return super.ResolveFireTree(target, visible, weaponReady, fireRate);
	}
}

modded class SCR_AILookAction
{
	override void LookAt(vector pos, float priority, float duration = 0.8)
	{
		if (KKCRX_SprintIgnoring())
			return;

		super.LookAt(pos, priority, duration);
	}

	override void LookAt(IEntity ent, float priority, float duration = 0.8)
	{
		if (KKCRX_SprintIgnoring())
			return;

		super.LookAt(ent, priority, duration);
	}

	protected bool KKCRX_SprintIgnoring()
	{
		return m_Utility &&
			(
				KK_GarrisonHold.IsIgnoringTargets(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsIgnoringTargets(m_Utility.GetOwner())
			);
	}
}
