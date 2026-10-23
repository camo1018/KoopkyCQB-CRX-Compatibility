// Pauses the CRX group and soldier settings that walk men off a Clear, Garrison,
// or Take cover order. The saved values are written back when the last such
// order on that group ends.

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
	bool m_bCombatFrozen;

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

	static bool IsCombatFrozen(SCR_AIGroup group)
	{
		if (!group)
			return false;

		KKCRX_GroupSuspend state = s_mGroups.Get(group);
		return state && state.m_bCombatFrozen;
	}

	static bool Suspend(SCR_AIGroup group, bool freezeCombatMove = false)
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
		if (freezeCombatMove)
			FreezeCombatMove(group, groupInfo, state);

		CaptureSoldiers(group, state);

		if (state.m_iDepth == 1 && SCR_BaseGameMode.KK_LogEnabled())
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

		if (SCR_BaseGameMode.KK_LogEnabled())
		{
			PrintFormat(
				"KKCRX: Restored CRX movement settings for %1",
				group
			);
		}
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

	// Take cover keeps moving through a fight. CRX combat move and cover
	// search would walk that push off the route, so those chances go to 0
	// while the order still owns the move. Resume puts the saved chances back.
	protected static void FreezeCombatMove(
		SCR_AIGroup group,
		SCR_AIGroupInfoComponent groupInfo,
		KKCRX_GroupSuspend state
	)
	{
		state.m_bCombatFrozen = true;
		groupInfo.SetCombatMoveChance(0);
		groupInfo.SetCombatCoverChance(0);
		groupInfo.SetCombatInCoverDynamicCoverSearchChance(0);

		SCR_AIConfigComponent config = GetConfig(group);
		if (!config)
			return;

		config.m_iCombatMoveChance = 0;
		config.m_iCombatCoverChance = 0;
		config.m_iCombatInCoverDynamicCoverSearchChance = 0;
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
	[Attribute("1", UIWidgets.CheckBox, "While a Clear, Garrison, or Take cover order is running, pause the CRX settings that pull soldiers off that order.", category: "Koopky CQB CRX")]
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

	override void OnActionRemoved()
	{
		super.OnActionRemoved();
		KKCRX_TryResume();
	}

	override void Supersede()
	{
		super.Supersede();
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

	// A restart that keeps this same activity is not a cancel. CRX stays
	// paused until this clear is finished, cancelled, or replaced.
	protected void KKCRX_TryResume()
	{
		if (!m_bKKCRX_Suspended || IsLive())
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

	override void OnActionRemoved()
	{
		super.OnActionRemoved();
		KKCRX_TryResume();
	}

	override void Supersede()
	{
		super.Supersede();
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

	// A restart that keeps this same activity is not a cancel. CRX stays
	// paused until this garrison is finished, cancelled, or replaced.
	protected void KKCRX_TryResume()
	{
		if (!m_bKKCRX_Suspended || IsLive())
			return;

		m_bKKCRX_Suspended = false;
		KKCRX_OrderSuspend.Resume(m_Group);
	}
}

modded class KK_AttackActivity
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
		KKCRX_SyncSuspend();
		return score;
	}

	override void OnActionDeselected()
	{
		super.OnActionDeselected();
		if (!IsLive())
			KKCRX_TryResume();
	}

	override void OnActionFailed()
	{
		super.OnActionFailed();
		if (!IsLive())
			KKCRX_TryResume();
	}

	override void OnActionRemoved()
	{
		super.OnActionRemoved();
		if (!IsLive())
			KKCRX_TryResume();
	}

	override void Supersede()
	{
		super.Supersede();
		KKCRX_TryResume();
	}

	protected void KKCRX_SyncSuspend()
	{
		// The fight at the point belongs to CRX again. A push or a bound
		// still holds those settings off. One man running back does not.
		if (!IsTakeCover() || KKCRX_FightYielded())
		{
			KKCRX_TryResume();
			return;
		}

		if (!m_bKKCRX_Suspended)
			KKCRX_TrySuspend();
		else
			KKCRX_OrderSuspend.CaptureLateSoldiers(m_Group);
	}

	protected bool KKCRX_FightYielded()
	{
		return ReleasedToFight();
	}

	protected void KKCRX_TrySuspend()
	{
		if (
			m_bKKCRX_Suspended ||
			m_bFinished ||
			m_bCancelled ||
			!m_AttackWaypoint ||
			!IsTakeCover()
		)
		{
			return;
		}

		if (KKCRX_OrderSuspend.Suspend(m_Group, true))
			m_bKKCRX_Suspended = true;
	}

	// A restart that keeps this same activity is not a handoff. CRX stays
	// paused until the order ends, unless a man at the point has been
	// given to the fight.
	protected void KKCRX_TryResume()
	{
		if (!m_bKKCRX_Suspended)
			return;

		if (IsLive() && !ReleasedToFight())
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

		// Recognition stays. CRX's rates, which change how he fights, do not.
		if (KKCRX_HouseOwns(owner))
		{
			KK_WriteHousePerception(perceptionComp, threatSystem);
			return;
		}

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

	override void EvaluateWeaponAndTarget(
		out bool outWeaponEvent,
		out bool outSelectedTargetChanged,
		out BaseTarget outPrevTarget,
		out BaseTarget outCurrentTarget,
		out bool outRetreatTargetChanged,
		out bool outCompartmentChanged)
	{
		// Aim and weapon choice stay with the house order. CRX writes both
		// again after that pass.
		if (KKCRX_HouseOwns(GetOwner()))
		{
			KK_EvaluateHouseOrder(
				outWeaponEvent,
				outSelectedTargetChanged,
				outPrevTarget,
				outCurrentTarget,
				outRetreatTargetChanged,
				outCompartmentChanged
			);
			return;
		}

		super.EvaluateWeaponAndTarget(
			outWeaponEvent,
			outSelectedTargetChanged,
			outPrevTarget,
			outCurrentTarget,
			outRetreatTargetChanged,
			outCompartmentChanged
		);

		// CRX selects the enemy again after Koopky clears him. That
		// selection aims the bound, and the aimed bound will not sprint.
		// The enemy he already had is kept, and put back when the sprint ends.
		IEntity sprinting = GetOwner();
		if (sprinting && KK_GarrisonHold.IsBoundSprint(sprinting))
		{
			KK_GarrisonHold.RememberSprintEnemy(sprinting, m_SelectedTarget);
			KK_ClearTarget();
			m_SelectedTargetVisible = false;
			outCurrentTarget = null;
			outSelectedTargetChanged = false;
			vector lane;
			if (KK_GarrisonHold.GetSprintLook(sprinting, lane))
				m_SelectedTargetDestinationPos = lane;
			return;
		}

		KK_HoldSprintEnemy(outCurrentTarget);

		// CRX puts the last-seen point back after Koopky clears it. That
		// point is what keeps the rifle firing along the old direction
		// once the building order has ended.
		if (KK_GarrisonHold.IsFreshOrder(GetOwner()) && KK_DropStaleTarget())
		{
			outCurrentTarget = null;
			outSelectedTargetChanged = false;
		}

		// A cover hold stands against something solid. This order's sight
		// trace hits that cover. Keep a living target the normal attack
		// can still see when CRX's own pass does not.
		IEntity owner = GetOwner();
		if (!owner || !KK_GarrisonHold.IsPinned(owner) || !KK_GarrisonHold.IsFreshOrder(owner))
			return;

		if (!m_SelectedTarget || !KK_GarrisonHold.IsLivingTarget(m_SelectedTarget))
			return;

		KK_GarrisonHold.EndFreshOrder(owner);
	}

	// A nearby casualty while the room is empty starts a rearm, and the
	// rearm drops the rifle. Inside a clear the route still owns that rifle.
	override void TryRearming()
	{
		if (KKCRX_RifleStaysUp(GetOwner(), null))
			return;

		super.TryRearming();
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

		// The bound is still his move. CRX's attack looks at the enemy and
		// that look walks the sprint. A man held on the spot can still shoot.
		if (KKCRX_OnRoute(character) || KKCRX_OnRoute(agentEntity))
			return 0;

		// The whole clear, including the walk in. A player command is a
		// high move, and CRX turns that into a strafe as soon as someone
		// is in sight, which is before the room gun owns the shot.
		if (
			KK_GarrisonHold.UseRoomCombat() &&
			(
				KK_GarrisonHold.OrderOwnsBehavior(character) ||
				KK_GarrisonHold.OrderOwnsBehavior(agentEntity)
			)
		)
		{
			m_bUseCombatMove = false;
			return 0;
		}

		// A remembered burst is not a target. CRX calls the base attack
		// directly, so this order would keep firing at that spot.
		bool onOrder =
			KK_GarrisonHold.UseRoomCombat() &&
			(
				KK_GarrisonHold.HasBuilding(character) ||
				KK_GarrisonHold.HasBuilding(agentEntity)
			);
		if (
			onOrder &&
			!KK_GarrisonHold.HasVisibleEnemy(character) &&
			!KK_GarrisonHold.HasVisibleEnemy(agentEntity)
		)
		{
			return 0;
		}

		// An empty gun cannot take the shot. A reload under pressure has to
		// leave so the move to cover can run. CRX would still score the attack.
		if (
			(KK_GarrisonHold.HasBuilding(character) && KK_GarrisonHold.CannotShoot(character)) ||
			(KK_GarrisonHold.HasBuilding(agentEntity) && KK_GarrisonHold.CannotShoot(agentEntity)) ||
			(KK_GarrisonHold.HasBuilding(character) && KK_GarrisonHold.MustDashToReload(character)) ||
			(KK_GarrisonHold.HasBuilding(agentEntity) && KK_GarrisonHold.MustDashToReload(agentEntity))
		)
		{
			return 0;
		}

		float score = super.CustomEvaluate();

		// CRX calls the base attack evaluate directly, so Koopky's flags
		// never land when CRX sits between this addon and Koopky.
		// Within range of the building, a clear does not take the combat
		// step. CRX turns that step into a short standing strafe. Farther
		// out, the attack keeps the step.
		bool clearingInside =
			(KK_GarrisonHold.IsClearing(character) && KK_GarrisonHold.OwnsShot(character)) ||
			(KK_GarrisonHold.IsClearing(agentEntity) && KK_GarrisonHold.OwnsShot(agentEntity));
		bool pinned =
			KK_GarrisonHold.IsPinned(character) ||
			KK_GarrisonHold.IsPinned(agentEntity);
		bool engageStop =
			KK_GarrisonHold.IsEngageStopped(character) ||
			KK_GarrisonHold.IsEngageStopped(agentEntity);
		bool doorFiring =
			KK_GarrisonHold.IsDoorFiring(character) ||
			KK_GarrisonHold.IsDoorFiring(agentEntity);

		vector approachGoal;
		bool steering =
			KK_GarrisonHold.GetApproachGoal(character, approachGoal) ||
			KK_GarrisonHold.GetApproachGoal(agentEntity, approachGoal);

		if (clearingInside || pinned || engageStop || doorFiring)
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
			m_fWaitTime.m_Value = KK_GarrisonHold.ShotDelayMs() / 1000;
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

		if (
			KK_GarrisonHold.HealingNow(body) ||
			(m_Utility && KK_GarrisonHold.HealingNow(m_Utility.GetOwner()))
		)
		{
			KK_GarrisonHold.HoldForHeal(body);
			return 0;
		}

		bool routeLocked =
			KKCRX_IsTakeCoverLocked(body) ||
			KK_GarrisonHold.IsBoundSprint(body) ||
			KK_GarrisonHold.IsRecalled(body);
		if (m_Utility)
		{
			routeLocked = routeLocked ||
				KKCRX_IsTakeCoverLocked(m_Utility.GetOwner()) ||
				KK_GarrisonHold.IsBoundSprint(m_Utility.GetOwner()) ||
				KK_GarrisonHold.IsRecalled(m_Utility.GetOwner());
		}

		// The whole clear keeps the route score. CRX drops a high move,
		// and a player command is one, to 70 or 90 once he has a target.
		// That drop is the shuffle, and it starts before the room gun
		// owns the shot.
		bool onHouse =
			KK_GarrisonHold.UseRoomCombat() &&
			(
				KK_GarrisonHold.OrderOwnsBehavior(body) ||
				(m_Utility && KK_GarrisonHold.OrderOwnsBehavior(m_Utility.GetOwner()))
			);
		if (onHouse)
		{
			m_bUseCombatMove = false;
			return vanilla.CustomEvaluate();
		}

		// Take cover's own move has to keep running. A bound sprint does too.
		// CRX would raise it to attack priority, and that hands the push to an aimed strafe.
		if (routeLocked)
		{
			float score = vanilla.CustomEvaluate();
			IEntity soldier = body;
			if (!soldier && m_Utility)
				soldier = m_Utility.GetOwner();

			// The bound's move is the sprint. Combat movement on that
			// behavior is the strafe toward the enemy.
			if (
				KK_GarrisonHold.IsBoundSprint(soldier) ||
				(m_Utility && KK_GarrisonHold.IsBoundSprint(m_Utility.GetOwner()))
			)
				m_bUseCombatMove = false;

			KKCRX_KeepBoundSprint(soldier);
			KKCRX_HoldPinned(soldier);
			KKCRX_KeepRecall(soldier);
			return score;
		}

		if (!KK_GarrisonHold.IsPinned(body))
			return super.CustomEvaluate();

		// CRX raises this move to attack priority and sprints when threatened.
		if (m_CharacterMovementComponent)
			m_CharacterMovementComponent.SetMovementTypeWanted(EMovementType.IDLE);

		return 0;
	}

	override void OnActionSelected()
	{
		// CRX's selection drops a high move into a combat step.
		if (
			m_Utility &&
			(
				KKCRX_HouseOwns(m_Utility.m_OwnerEntity) ||
				KKCRX_HouseOwns(m_Utility.GetOwner())
			)
		)
			vanilla.OnActionSelected();
		else
			super.OnActionSelected();

		// CRX queues a lower a few hundred milliseconds after every move.
		// A clear reissues that move along the route, so the lower keeps
		// landing on a rifle Koopky just raised.
		if (!m_Utility || !KKCRX_RifleStaysUp(m_Utility.m_OwnerEntity, m_Utility.GetOwner()))
			return;

		m_Utility.GetCallqueue().Remove(LowerWeaponDelayed);
		m_Utility.GetCallqueue().Remove(ProvideAmmoOrPickupInventoryItemsDelayed);
	}

	override void LowerWeaponDelayed()
	{
		if (m_Utility && KKCRX_RifleStaysUp(m_Utility.m_OwnerEntity, m_Utility.GetOwner()))
			return;

		super.LowerWeaponDelayed();
	}

	// Queued beside the other lower whenever a provide-ammo or pickup
	// action exists. A clear reissues the move, so this was still dropping
	// the rifle a few hundred milliseconds into the step and changing the
	// speed under it.
	override void ProvideAmmoOrPickupInventoryItemsDelayed(bool provideAmmoBehavior)
	{
		if (m_Utility && KKCRX_RifleStaysUp(m_Utility.m_OwnerEntity, m_Utility.GetOwner()))
			return;

		super.ProvideAmmoOrPickupInventoryItemsDelayed(provideAmmoBehavior);
	}
}

// Formation's selection drops the rifle with no check. A clear does not
// use that move. Skipping CRX here leaves the rifle on the route.
modded class SCR_AIMoveInFormationBehavior
{
	override void OnActionSelected()
	{
		if (
			m_Utility &&
			(
				KKCRX_HouseOwns(m_Utility.m_OwnerEntity) ||
				KKCRX_HouseOwns(m_Utility.GetOwner()) ||
				KKCRX_RifleStaysUp(m_Utility.m_OwnerEntity, m_Utility.GetOwner())
			)
		)
		{
			vanilla.OnActionSelected();
			return;
		}

		super.OnActionSelected();
	}

	override float CustomEvaluate()
	{
		IEntity body;
		if (m_Utility)
			body = m_Utility.m_OwnerEntity;

		bool onHouse =
			KKCRX_HouseOwns(body) ||
			(m_Utility && KKCRX_HouseOwns(m_Utility.GetOwner()));
		if (onHouse)
			return vanilla.CustomEvaluate();

		return super.CustomEvaluate();
	}
}

// Idle drops the rifle when CRX still has the weapon on autonomous.
// Holding it raised has to win for the whole clear, including a gap
// between move orders.
modded class SCR_AIIdleBehavior
{
	override void OnActionSelected()
	{
		if (
			m_Utility &&
			(
				KKCRX_HouseOwns(m_Utility.m_OwnerEntity) ||
				KKCRX_HouseOwns(m_Utility.GetOwner()) ||
				KKCRX_RifleStaysUp(m_Utility.m_OwnerEntity, m_Utility.GetOwner())
			)
		)
		{
			vanilla.OnActionSelected();
			return;
		}

		super.OnActionSelected();
	}
}

modded class SCR_AIMedicHealBehavior
{
	override void MedicHealBehaviorDelayed()
	{
		if (m_Utility && KKCRX_RifleStaysUp(m_Utility.m_OwnerEntity, m_Utility.GetOwner()))
			return;

		super.MedicHealBehaviorDelayed();
	}

	// CRX scores this after Koopky. The cluster gate has to be the last word.
	override float CustomEvaluate()
	{
		float score = super.CustomEvaluate();
		if (KK_GarrisonHold.ClearHealSuppressed(m_Utility))
			return 0;

		if (KK_GarrisonHold.ClearHealBoost(m_Utility))
			return Math.Max(score, SCR_AIActionBase.PRIORITY_BEHAVIOR_HEAL_HIGH_PRIORITY);

		return score;
	}
}

// CRX raises a heal to attack priority while he is still in the cluster.
// That raise lands after Koopky's gate, so the gate is applied again here.
modded class SCR_AIHealBehavior
{
	override float CustomEvaluate()
	{
		float score = super.CustomEvaluate();
		if (KK_GarrisonHold.ClearHealSuppressed(m_Utility))
			return 0;

		if (KK_GarrisonHold.ClearHealBoost(m_Utility))
			return Math.Max(score, SCR_AIActionBase.PRIORITY_BEHAVIOR_HEAL_HIGH_PRIORITY);

		return score;
	}
}

modded class SCR_AICombatMoveLogicBase
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		IEntity body;
		if (owner)
			body = owner.GetControlledEntity();

		if (KK_GarrisonHold.HealingNow(body) || KK_GarrisonHold.HealingNow(owner))
		{
			if (m_State && m_State.IsExecutingRequest())
				m_State.CancelRequest();

			IEntity healer = body;
			if (!healer)
				healer = owner;
			KK_GarrisonHold.HoldForHeal(healer);
			return ENodeResult.RUNNING;
		}

		// The route owns the feet for the whole order, including outside.
		if (KKCRX_HouseOwns(body) || KKCRX_HouseOwns(owner))
		{
			if (m_State && m_State.IsExecutingRequest())
				m_State.CancelRequest();

			return ENodeResult.RUNNING;
		}

		bool holdRoute =
			KK_GarrisonHold.IsPinned(body) ||
			KK_GarrisonHold.IsPinned(owner) ||
			KK_GarrisonHold.IsEngageStopped(body) ||
			KK_GarrisonHold.IsEngageStopped(owner) ||
			KKCRX_IsTakeCoverLocked(body) ||
			KKCRX_IsTakeCoverLocked(owner) ||
			KK_GarrisonHold.IsBoundSprint(body) ||
			KK_GarrisonHold.IsBoundSprint(owner) ||
			KK_GarrisonHold.IsRecalled(body) ||
			KK_GarrisonHold.IsRecalled(owner);

		if (holdRoute)
		{
			if (m_State && m_State.IsExecutingRequest())
				m_State.CancelRequest();

			IEntity soldier = body;
			if (!soldier)
				soldier = owner;

			KKCRX_KeepBoundSprint(soldier);
			KKCRX_HoldPinned(soldier);
			KKCRX_HoldEngageStop(soldier);
			KKCRX_KeepRecall(soldier);
			return ENodeResult.RUNNING;
		}

		// A raised threat keeps requesting a short strafe after the enemy
		// is gone. CRX calls the base combat move, so hold the node instead.
		// A clear is not in this loop. The request is refused when CRX
		// submits it, which is the indoor standing strafe.
		bool onOrder =
			KK_GarrisonHold.UseRoomCombat() &&
			(
				KK_GarrisonHold.HasBuilding(body) ||
				KK_GarrisonHold.HasBuilding(owner)
			);
		bool clearing =
			KK_GarrisonHold.IsClearing(body) ||
			KK_GarrisonHold.IsClearing(owner);
		if (
			onOrder &&
			!clearing &&
			!KK_GarrisonHold.HasVisibleEnemy(body) &&
			!KK_GarrisonHold.HasVisibleEnemy(owner)
		)
		{
			if (m_State && m_State.IsExecutingRequest())
				m_State.CancelRequest();

			return ENodeResult.RUNNING;
		}

		// An empty gun under fire sprints to Koopky's cover point. CRX's
		// combat move would replace that sprint with its own cover search.
		if (KK_GarrisonHold.MustDashToReload(body))
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
				if (target && KK_GarrisonHold.IsLivingTarget(target))
				{
					IEntity targetEntity = target.GetTargetEntity();
					if (targetEntity)
						aimPos = KK_GarrisonHold.ShotAimPoint(targetEntity);
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

		if (KKCRX_IsTakeCoverLocked(m_Entity) || KKCRX_IsTakeCoverLocked(m_ControlledEntity))
			return false;

		return super.SuppressedInCoverCondition();
	}
}

modded class SCR_AIGetCombatMovementParameters
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		IEntity body;
		if (owner)
			body = owner.GetControlledEntity();

		// CRX writes an aimed walk onto a move that still has a target.
		// The bound has to stay the speed Koopky set.
		if (KKCRX_HouseOwns(body) || KKCRX_HouseOwns(owner) || KKCRX_OnRoute(body) || KKCRX_OnRoute(owner))
			return vanilla.EOnTaskSimulate(owner, dt);

		return super.EOnTaskSimulate(owner, dt);
	}
}

modded class SCR_AIGetCombatMoveRequestParameters_Move
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		IEntity body;
		if (owner)
			body = owner.GetControlledEntity();

		// Inside a building, CRX rewrites this request into a short standing
		// strafe. On a clear that strafe is the shuffle, so the request
		// keeps the normal parameters and the route stays the move order.
		if (
			KKCRX_OnRoute(body) ||
			KKCRX_OnRoute(owner) ||
			KKCRX_HouseOwns(body) ||
			KKCRX_HouseOwns(owner) ||
			KK_GarrisonHold.IsClearing(body) ||
			KK_GarrisonHold.IsClearing(owner)
		)
			return vanilla.EOnTaskSimulate(owner, dt);

		return super.EOnTaskSimulate(owner, dt);
	}
}

modded class SCR_AIDangerReaction_ProjectileHit
{
	override bool PerformReaction(notnull SCR_AIUtilityComponent utility, notnull SCR_AIThreatSystem threatSystem, AIDangerEvent dangerEvent, int dangerEventCount)
	{
		if (KKCRX_HouseOwns(utility.m_OwnerEntity) || KKCRX_HouseOwns(utility.GetOwner()) || KKCRX_IsHoldingPost(utility.m_OwnerEntity) || KKCRX_IsReloadDash(utility.m_OwnerEntity) || KKCRX_IsTakeCoverLocked(utility.m_OwnerEntity) || KK_GarrisonHold.IsRecalled(utility.m_OwnerEntity))
			return true;

		return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);
	}
}

modded class SCR_AIDangerReaction_DamageTaken
{
	override bool PerformReaction(notnull SCR_AIUtilityComponent utility, notnull SCR_AIThreatSystem threatSystem, AIDangerEvent dangerEvent, int dangerEventCount)
	{
		if (KKCRX_HouseOwns(utility.m_OwnerEntity) || KKCRX_HouseOwns(utility.GetOwner()) || KKCRX_IsHoldingPost(utility.m_OwnerEntity) || KKCRX_IsReloadDash(utility.m_OwnerEntity) || KKCRX_IsTakeCoverLocked(utility.m_OwnerEntity) || KK_GarrisonHold.IsRecalled(utility.m_OwnerEntity))
			return true;

		return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);
	}
}

modded class SCR_AIDangerReaction_Explosion
{
	override bool PerformReaction(notnull SCR_AIUtilityComponent utility, notnull SCR_AIThreatSystem threatSystem, AIDangerEvent dangerEvent, int dangerEventCount)
	{
		if (KKCRX_HouseOwns(utility.m_OwnerEntity) || KKCRX_HouseOwns(utility.GetOwner()) || KKCRX_IsHoldingPost(utility.m_OwnerEntity) || KKCRX_IsReloadDash(utility.m_OwnerEntity) || KKCRX_IsTakeCoverLocked(utility.m_OwnerEntity) || KK_GarrisonHold.IsRecalled(utility.m_OwnerEntity))
			return true;

		return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);
	}
}

bool KKCRX_HouseOwns(IEntity soldier)
{
	return KK_GarrisonHold.UseRoomCombat() &&
		KK_GarrisonHold.OrderOwnsBehavior(soldier);
}

bool KKCRX_IsHoldingPost(IEntity soldier)
{
	return KK_GarrisonHold.IsPinned(soldier);
}

// A clear or an attack still wants the rifle up. A reload, an empty gun,
// a bash, and a sprint are not in this, so those lowers still run.
bool KKCRX_RifleStaysUp(IEntity soldier, IEntity other)
{
	return KKCRX_OneRifleStaysUp(soldier) || KKCRX_OneRifleStaysUp(other);
}

bool KKCRX_OneRifleStaysUp(IEntity soldier)
{
	if (KK_GarrisonHold.YieldsWeaponForHeal(soldier))
		return false;

	return KK_GarrisonHold.ClearWeaponStaysUp(soldier) ||
		KK_GarrisonHold.AttackWeaponStaysUp(soldier) ||
		KK_GarrisonHold.CombatOwnsWeapon(soldier);
}

// Soldiers whose weapon mode we changed from autonomous to raised.
// Putting autonomous back is limited to that set.
ref set<SCR_AIInfoComponent> s_KKCRX_HeldWeaponMode = new set<SCR_AIInfoComponent>();

void KKCRX_HoldWeaponMode(IEntity soldier)
{
	SCR_AIInfoComponent info = KKCRX_Info(soldier);
	if (!info || info.GetWeaponRaised() != CRX_EAIWeaponRaised.AUTONOMOUS)
		return;

	info.SetWeaponRaised(CRX_EAIWeaponRaised.RAISE);
	s_KKCRX_HeldWeaponMode.Insert(info);
}

void KKCRX_ReleaseWeaponMode(IEntity soldier)
{
	SCR_AIInfoComponent info = KKCRX_Info(soldier);
	if (!info || !s_KKCRX_HeldWeaponMode.Contains(info))
		return;

	s_KKCRX_HeldWeaponMode.RemoveItem(info);
	if (info.GetWeaponRaised() == CRX_EAIWeaponRaised.RAISE)
		info.SetWeaponRaised(CRX_EAIWeaponRaised.AUTONOMOUS);
}

SCR_AIInfoComponent KKCRX_Info(IEntity soldier)
{
	if (!soldier)
		return null;

	IEntity body = soldier;
	AIAgent agent = AIAgent.Cast(soldier);
	if (agent)
		body = agent.GetControlledEntity();

	if (!body)
		return null;

	return SCR_AIInfoComponent.Cast(body.FindComponent(SCR_AIInfoComponent));
}

// The bound runner has to keep the sprint Koopky issued. CRX aims that
// step at the enemy, and the sprint becomes a sidestep. The look stays on
// the lane from the look node. This only keeps the rifle down and the
// speed on sprint, and drops a combat move that is already running.
void KKCRX_KeepBoundSprint(IEntity soldier)
{
	if (!KK_GarrisonHold.IsBoundSprint(soldier))
		return;

	IEntity body = soldier;
	AIAgent agent = AIAgent.Cast(soldier);
	if (agent)
		body = agent.GetControlledEntity();

	if (!body)
		body = soldier;

	SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
		body.FindComponent(SCR_AIUtilityComponent)
	);
	if (!utility && agent)
	{
		utility = SCR_AIUtilityComponent.Cast(
			agent.FindComponent(SCR_AIUtilityComponent)
		);
	}

	// The request still running is the walk at the enemy. Sprint that,
	// and he leaves the waypoint. Drop it before the speed is put back.
	if (utility && utility.m_CombatMoveState)
	{
		if (utility.m_CombatMoveState.IsExecutingRequest())
			utility.m_CombatMoveState.CancelRequest();

		utility.m_CombatMoveState.EnableAiming(false);
		utility.m_CombatMoveState.m_bAimAtTarget = false;
	}

	// No lane stored yet. A look already on a man would turn this sprint
	// into a strafe. A commander look is the lane itself, and this leaves it.
	if (utility && utility.m_LookAction)
	{
		vector lane;
		if (!KK_GarrisonHold.GetSprintLook(body, lane))
			utility.m_LookAction.KKCRX_ReleaseRoute();
	}

	CharacterControllerComponent controller = CharacterControllerComponent.Cast(
		body.FindComponent(CharacterControllerComponent)
	);
	if (controller && (controller.IsWeaponRaised() || controller.IsWeaponADS()))
	{
		controller.SetWeaponADS(false);
		controller.SetWeaponRaised(false);
	}

	AICharacterMovementComponent movement = AICharacterMovementComponent.Cast(
		body.FindComponent(AICharacterMovementComponent)
	);
	if (movement)
		movement.SetMovementTypeWanted(EMovementType.SPRINT);
}

// A man walked back to the point keeps that run. CRX is still on for the
// others, and aiming this step at the enemy turns it into a sidestep.
void KKCRX_KeepRecall(IEntity soldier)
{
	if (!KK_GarrisonHold.IsRecalled(soldier))
		return;

	IEntity body = soldier;
	AIAgent agent = AIAgent.Cast(soldier);
	if (agent)
		body = agent.GetControlledEntity();

	if (!body)
		body = soldier;

	SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
		body.FindComponent(SCR_AIUtilityComponent)
	);
	if (!utility && agent)
	{
		utility = SCR_AIUtilityComponent.Cast(
			agent.FindComponent(SCR_AIUtilityComponent)
		);
	}

	if (utility && utility.m_CombatMoveState)
	{
		if (utility.m_CombatMoveState.IsExecutingRequest())
			utility.m_CombatMoveState.CancelRequest();

		utility.m_CombatMoveState.EnableAiming(false);
		utility.m_CombatMoveState.m_bAimAtTarget = false;
	}

	if (utility && utility.m_LookAction)
		utility.m_LookAction.KKCRX_ReleaseRoute();

	AICharacterMovementComponent movement = AICharacterMovementComponent.Cast(
		body.FindComponent(AICharacterMovementComponent)
	);
	if (movement)
		movement.SetMovementTypeWanted(EMovementType.RUN);
}

// A soldier who has stopped to shoot is pinned. CRX still aims the move
// that was just cancelled, and that aim turns the stop into a sidestep.
void KKCRX_HoldPinned(IEntity soldier)
{
	if (!KK_GarrisonHold.IsPinned(soldier) || KK_GarrisonHold.IsBoundSprint(soldier))
		return;

	IEntity body = soldier;
	AIAgent agent = AIAgent.Cast(soldier);
	if (agent)
		body = agent.GetControlledEntity();

	if (!body)
		body = soldier;

	AICharacterMovementComponent movement = AICharacterMovementComponent.Cast(
		body.FindComponent(AICharacterMovementComponent)
	);
	if (movement)
		movement.SetMovementTypeWanted(EMovementType.IDLE);

	CharacterControllerComponent controller = CharacterControllerComponent.Cast(
		body.FindComponent(CharacterControllerComponent)
	);
	if (controller)
		controller.OverrideMaxSpeed(0);

	SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
		body.FindComponent(SCR_AIUtilityComponent)
	);
	if (!utility && agent)
	{
		utility = SCR_AIUtilityComponent.Cast(
			agent.FindComponent(SCR_AIUtilityComponent)
		);
	}

	if (!utility || !utility.m_CombatMoveState)
		return;

	utility.m_CombatMoveState.m_bAimAtTarget = false;
}

// Attack engage stops him with a speed cap. CRX still aims the move that
// was just cancelled, and that aim turns the stop into a sidestep. The
// attack step stays queued. Only the strafe is dropped.
void KKCRX_HoldEngageStop(IEntity soldier)
{
	if (!KK_GarrisonHold.IsEngageStopped(soldier) || KK_GarrisonHold.IsBoundSprint(soldier))
		return;

	IEntity body = soldier;
	AIAgent agent = AIAgent.Cast(soldier);
	if (agent)
		body = agent.GetControlledEntity();

	if (!body)
		body = soldier;

	CharacterControllerComponent controller = CharacterControllerComponent.Cast(
		body.FindComponent(CharacterControllerComponent)
	);
	if (controller)
		controller.OverrideMaxSpeed(0);

	SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
		body.FindComponent(SCR_AIUtilityComponent)
	);
	if (!utility && agent)
	{
		utility = SCR_AIUtilityComponent.Cast(
			agent.FindComponent(SCR_AIUtilityComponent)
		);
	}

	if (!utility || !utility.m_CombatMoveState)
		return;

	if (utility.m_CombatMoveState.IsExecutingRequest())
		utility.m_CombatMoveState.CancelRequest();

	utility.m_CombatMoveState.m_bAimAtTarget = false;
}

bool KKCRX_IsTakeCoverLocked(IEntity soldier)
{
	if (!soldier)
		return false;

	AIAgent agent = AIAgent.Cast(soldier);
	if (!agent)
	{
		AIControlComponent control = AIControlComponent.Cast(
			soldier.FindComponent(AIControlComponent)
		);
		if (control)
			agent = control.GetAIAgent();
	}

	if (!agent)
		return false;

	SCR_AIGroup group = SCR_AIGroup.Cast(agent);
	if (!group)
		group = SCR_AIGroup.Cast(agent.GetParentGroup());

	return KKCRX_OrderSuspend.IsCombatFrozen(group);
}

// The bound, the walk back, and a door wait are still the order's move.
// A man planted on his spot is not: he is there to shoot.
bool KKCRX_OnRoute(IEntity soldier)
{
	if (!soldier)
		return false;

	if (KK_GarrisonHold.IsBoundSprint(soldier) || KK_GarrisonHold.IsRecalled(soldier))
		return true;

	if (!KKCRX_IsTakeCoverLocked(soldier))
		return false;

	return !KK_GarrisonHold.IsPinned(soldier);
}

// The pair that is sprinting, and a man walked back to the point.
// Either one turns into a strafe when the head goes to a target.
bool KKCRX_IsDashing(IEntity soldier)
{
	return KK_GarrisonHold.IsBoundSprint(soldier) || KK_GarrisonHold.IsRecalled(soldier);
}

bool KKCRX_IsReloadDash(IEntity soldier)
{
	return KK_GarrisonHold.MustDashToReload(soldier);
}

modded class SCR_AIAvoidCharacterBehavior
{
	override float CustomEvaluate()
	{
		if (!m_Utility)
			return super.CustomEvaluate();

		if (
			KKCRX_HouseOwns(m_Utility.m_OwnerEntity) ||
			KKCRX_HouseOwns(m_Utility.GetOwner())
		)
			return 0;

		// CRX calls the base avoid directly, so a pin or a take cover push
		// never reaches Koopky. Either one has to stay on the spot.
		if (
			m_Utility &&
			(
				KK_GarrisonHold.IsPinned(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsPinned(m_Utility.GetOwner()) ||
				KK_GarrisonHold.IsDoorFiring(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsDoorFiring(m_Utility.GetOwner()) ||
				KK_GarrisonHold.SprintBeforeReload(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.SprintBeforeReload(m_Utility.GetOwner()) ||
				KK_GarrisonHold.IsReloadBashing(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsReloadBashing(m_Utility.GetOwner()) ||
				KKCRX_IsTakeCoverLocked(m_Utility.m_OwnerEntity) ||
				KKCRX_IsTakeCoverLocked(m_Utility.GetOwner()) ||
				KK_GarrisonHold.IsRecalled(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsRecalled(m_Utility.GetOwner())
			)
		)
		{
			return 0;
		}

		return super.CustomEvaluate();
	}
}

modded class SCR_AIRetreatFromTargetBehavior
{
	override float CustomEvaluate()
	{
		// CRX calls the base retreat directly, so Koopky's zero never lands.
		// Knowing the target is there, without a sight line, is enough for
		// this to walk him back. A clear keeps the route.
		if (KK_GarrisonHold.ClearerPushing(m_Utility))
			return 0;

		return super.CustomEvaluate();
	}
}

modded class SCR_AIRetreatWhileLookAtBehavior
{
	override float CustomEvaluate()
	{
		if (KK_GarrisonHold.ClearerPushing(m_Utility))
			return 0;

		if (
			m_Utility &&
			(
				KK_GarrisonHold.IsDoorFiring(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsDoorFiring(m_Utility.GetOwner()) ||
				KK_GarrisonHold.IsPinned(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsPinned(m_Utility.GetOwner()) ||
				KK_GarrisonHold.SprintBeforeReload(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.SprintBeforeReload(m_Utility.GetOwner()) ||
				KK_GarrisonHold.IsReloadBashing(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsReloadBashing(m_Utility.GetOwner()) ||
				KKCRX_IsTakeCoverLocked(m_Utility.m_OwnerEntity) ||
				KKCRX_IsTakeCoverLocked(m_Utility.GetOwner()) ||
				KK_GarrisonHold.IsRecalled(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsRecalled(m_Utility.GetOwner())
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

		// A selected last-seen point sets endangered back to full on every
		// update. Drop it before that, until this order sees someone.
		// A cover hold keeps the living target. The sight trace hits the
		// cover he is planted against.
		bool coverHold =
			m_Utility &&
			(
				KK_GarrisonHold.IsPinned(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsPinned(m_Utility.GetOwner())
			);
		if (
			m_Utility &&
			!coverHold &&
			(
				KK_GarrisonHold.IsFreshOrder(m_Utility.m_OwnerEntity) ||
				KK_GarrisonHold.IsFreshOrder(m_Utility.GetOwner())
			) &&
			!KK_GarrisonHold.HasVisibleEnemy(m_Utility.m_OwnerEntity) &&
			!KK_GarrisonHold.HasVisibleEnemy(m_Utility.GetOwner()) &&
			m_Utility.m_CombatComponent
		)
		{
			m_Utility.m_CombatComponent.KK_ClearTarget();
		}

		// The threat value still moves. CRX's update also changes how he acts.
		if (
			m_Utility &&
			(
				KKCRX_HouseOwns(m_Utility.m_OwnerEntity) ||
				KKCRX_HouseOwns(m_Utility.GetOwner())
			)
		)
		{
			vanilla.Update(utility, timeSlice);
			return;
		}

		super.Update(utility, timeSlice);
	}

	override void ThreatBulletImpact(int count)
	{
		if (KKCRX_SprintIgnoring())
			return;

		if (
			m_Utility &&
			(
				KKCRX_HouseOwns(m_Utility.m_OwnerEntity) ||
				KKCRX_HouseOwns(m_Utility.GetOwner())
			)
		)
		{
			vanilla.ThreatBulletImpact(count);
			return;
		}

		super.ThreatBulletImpact(count);
	}

	override void ThreatExplosion(float distance)
	{
		if (KKCRX_SprintIgnoring())
			return;

		if (
			m_Utility &&
			(
				KKCRX_HouseOwns(m_Utility.m_OwnerEntity) ||
				KKCRX_HouseOwns(m_Utility.GetOwner())
			)
		)
		{
			vanilla.ThreatExplosion(distance);
			return;
		}

		super.ThreatExplosion(distance);
	}

	override void ThreatShotFired(float distance, int count)
	{
		if (KKCRX_SprintIgnoring())
			return;

		if (
			m_Utility &&
			(
				KKCRX_HouseOwns(m_Utility.m_OwnerEntity) ||
				KKCRX_HouseOwns(m_Utility.GetOwner())
			)
		)
		{
			vanilla.ThreatShotFired(distance, count);
			return;
		}

		super.ThreatShotFired(distance, count);
	}

	override void ThreatProjectileFlyby(int count)
	{
		if (KKCRX_SprintIgnoring())
			return;

		if (
			m_Utility &&
			(
				KKCRX_HouseOwns(m_Utility.m_OwnerEntity) ||
				KKCRX_HouseOwns(m_Utility.GetOwner())
			)
		)
		{
			vanilla.ThreatProjectileFlyby(count);
			return;
		}

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
		if (KKCRX_HouseOwns(m_OwnerEntity) || KKCRX_HouseOwns(GetOwner()))
			return KK_SelectHouseBehavior(unknownTarget);

		bool ignore =
			KK_GarrisonHold.IsIgnoringTargets(m_OwnerEntity) ||
			KK_GarrisonHold.IsIgnoringTargets(GetOwner());

		if (ignore && m_CombatComponent)
			m_CombatComponent.KK_ClearTarget();

		// The previous fight's unknown point would start an investigate
		// on a house he was just told to clear again. CRX calls the base
		// evaluate, so that point has to be dropped here.
		bool freshOrder =
			KK_GarrisonHold.IsFreshOrder(m_OwnerEntity) ||
			KK_GarrisonHold.IsFreshOrder(GetOwner());
		IEntity viewer = m_OwnerEntity;
		if (!viewer)
			viewer = GetOwner();
		bool staleUnknown =
			freshOrder &&
			(
				!unknownTarget ||
				!KK_GarrisonHold.IsLivingTarget(unknownTarget) ||
				!KK_GarrisonHold.SeesTarget(viewer, unknownTarget)
			);

		SCR_AIBehaviorBase result;
		if (ignore || staleUnknown)
			result = super.EvaluateBehavior(null);
		else
			result = super.EvaluateBehavior(unknownTarget);

		if (ignore)
			KK_GarrisonHold.EnforceSprintIgnore(this);
		else if (
			KKCRX_IsDashing(m_OwnerEntity) ||
			KKCRX_IsDashing(GetOwner())
		)
		{
			// Before the sprint move exists, the last fight is still
			// walking him at the enemy. That request is what he sprints.
			IEntity soldier = m_OwnerEntity;
			if (!soldier)
				soldier = GetOwner();

			KKCRX_KeepBoundSprint(soldier);
			KKCRX_KeepRecall(soldier);
		}
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
			KK_GarrisonHold.KeepClearWeaponRaised(soldier);
		}

		return result;
	}
}

modded class SCR_AICharacterSetMovementSpeed
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		IEntity body;
		if (owner)
			body = owner.GetControlledEntity();

		// CRX writes the speed after the route has set it.
		ENodeResult result;
		if (KKCRX_HouseOwns(body) || KKCRX_HouseOwns(owner))
			result = vanilla.EOnTaskSimulate(owner, dt);
		else
			result = super.EOnTaskSimulate(owner, dt);

		// CRX writes the speed from the tree after the order. An aimed
		// bound was coming out as a walk. Put the sprint back after the
		// tree has accepted the move. The rifle-up node can sit on a tree
		// CRX no longer uses for this step, so the jog and the hold raise here.
		if (KK_GarrisonHold.IsBoundSprint(body) || KK_GarrisonHold.IsBoundSprint(owner))
			KKCRX_KeepBoundSprint(body);
		else if (KK_GarrisonHold.IsRecalled(body) || KK_GarrisonHold.IsRecalled(owner))
		{
			KKCRX_KeepRecall(body);
			KKCRX_HoldEngageStop(body);
			KKCRX_HoldEngageStop(owner);
		}
		else
		{
			KKCRX_HoldPinned(body);
			KKCRX_HoldPinned(owner);
			KKCRX_HoldEngageStop(body);
			KKCRX_HoldEngageStop(owner);

			if (
				KK_GarrisonHold.IsMoveFire(body) ||
				KK_GarrisonHold.IsMoveFire(owner) ||
				KK_GarrisonHold.IsRoomFire(body) ||
				KK_GarrisonHold.IsRoomFire(owner)
			)
			{
				SCR_ChimeraAIAgent moving = SCR_ChimeraAIAgent.Cast(owner);
				if (moving)
					KK_GarrisonHold.ApplyMoveFire(moving.m_UtilityComponent);
			}
			else
			{
				IEntity soldier = body;
				if (!soldier)
					soldier = owner;

				if (KK_GarrisonHold.YieldsWeaponForHeal(soldier))
				{
					KKCRX_ReleaseWeaponMode(soldier);
					KK_GarrisonHold.LeaveWeaponForHeal(soldier);
				}
				else
				{
					KK_GarrisonHold.KeepClearWeaponRaised(soldier);
					if (KKCRX_RifleStaysUp(soldier, owner))
						KKCRX_HoldWeaponMode(soldier);
				}
			}
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

		if (KK_GarrisonHold.YieldsWeaponForHeal(body) || KK_GarrisonHold.YieldsWeaponForHeal(owner))
		{
			KKCRX_ReleaseWeaponMode(body);
			KKCRX_ReleaseWeaponMode(owner);

			IEntity healer = body;
			if (!healer)
				healer = owner;
			KK_GarrisonHold.LeaveWeaponForHeal(healer);
			return ENodeResult.SUCCESS;
		}

		if (KK_GarrisonHold.IsIgnoringTargets(body) || KK_GarrisonHold.IsIgnoringTargets(owner))
		{
			KKCRX_ReleaseWeaponMode(body);
			KKCRX_ReleaseWeaponMode(owner);

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

		// A bound sprint cannot keep the rifle up. The raise would cut the run.
		if (KK_GarrisonHold.IsBoundSprint(body) || KK_GarrisonHold.IsBoundSprint(owner))
		{
			KKCRX_ReleaseWeaponMode(body);
			KKCRX_ReleaseWeaponMode(owner);

			if (body)
			{
				CharacterControllerComponent controller =
					CharacterControllerComponent.Cast(
						body.FindComponent(CharacterControllerComponent)
					);

				// Sending the lower again restarts it and cuts the step off.
				if (controller && (controller.IsWeaponRaised() || controller.IsWeaponADS()))
				{
					controller.SetWeaponADS(false);
					controller.SetWeaponRaised(false);
				}
			}

			return ENodeResult.SUCCESS;
		}

		if (KK_GarrisonHold.OwnsShot(body) || KK_GarrisonHold.OwnsShot(owner))
		{
			SCR_ChimeraAIAgent roomSoldier = SCR_ChimeraAIAgent.Cast(owner);
			if (roomSoldier)
				KK_GarrisonHold.ApplyRoomShot(roomSoldier.m_UtilityComponent);

			IEntity clearer = body;
			if (!clearer)
				clearer = owner;
			KK_GarrisonHold.KeepClearWeaponRaised(clearer);
			if (KKCRX_RifleStaysUp(clearer, owner))
				KKCRX_HoldWeaponMode(clearer);
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

		if (
			KK_GarrisonHold.SprintBeforeReload(body) ||
			KK_GarrisonHold.SprintBeforeReload(owner) ||
			KK_GarrisonHold.IsReloadBashing(body) ||
			KK_GarrisonHold.IsReloadBashing(owner)
		)
		{
			KKCRX_ReleaseWeaponMode(body);
			KKCRX_ReleaseWeaponMode(owner);
			KK_GarrisonHold.LowerForReloadSprint(owner);
			return ENodeResult.SUCCESS;
		}

		IEntity clearer = body;
		if (!clearer)
			clearer = owner;
		if (KK_GarrisonHold.KeepClearWeaponRaised(clearer))
		{
			// RAISE makes CRX skip the tree port. That port is the lower.
			KKCRX_HoldWeaponMode(clearer);
			return ENodeResult.SUCCESS;
		}

		KKCRX_ReleaseWeaponMode(clearer);

		if (KKCRX_HouseOwns(body) || KKCRX_HouseOwns(owner))
			return vanilla.EOnTaskSimulate(owner, dt);

		return super.EOnTaskSimulate(owner, dt);
	}
}

modded class SCR_AIUpdateTargetAttackData
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		IEntity body;
		if (owner)
			body = owner.GetControlledEntity();

		// A lowered rifle is "look at the target" in this node. That look
		// is the running pair turning onto the enemy. No aim on this step.
		if (KKCRX_IsDashing(body) || KKCRX_IsDashing(owner))
		{
			SetVariableOut(PORT_FIRE_TREE_ID, FIRE_TREE_INVALID);
			SetVariableOut(PORT_VISIBLE, false);
			return ENodeResult.SUCCESS;
		}

		return super.EOnTaskSimulate(owner, dt);
	}

	override int ResolveFireTree(
		BaseTarget target,
		bool visible,
		bool weaponReady,
		out float fireRate)
	{
		IEntity body;
		if (m_CharacterControllerComponent)
			body = m_CharacterControllerComponent.GetOwner();

		IEntity utilityBody;
		IEntity utilityOwner;
		if (m_UtilityComponent)
		{
			utilityBody = m_UtilityComponent.m_OwnerEntity;
			utilityOwner = m_UtilityComponent.GetOwner();
		}

		if (
			KKCRX_IsDashing(body) ||
			KKCRX_IsDashing(utilityBody) ||
			KKCRX_IsDashing(utilityOwner)
		)
			return FIRE_TREE_INVALID;

		// The attack node has to run. It is what points the gun at the target
		// and brings it up. CRX then drops the gun whenever the fire tree has
		// no shot, which is the lower-and-raise while the room gun owns it.
		// An attack jog keeps the rifle up the same way. A clear does too,
		// or the next raise cuts the step off and he shuffles.
		// vanilla skips that drop. super is CRX, and the drop is in there.
		if (
			KKCRX_HouseOwns(body) ||
			KKCRX_HouseOwns(utilityBody) ||
			KKCRX_HouseOwns(utilityOwner) ||
			KKCRX_RifleStaysUp(body, utilityBody) ||
			KKCRX_RifleStaysUp(utilityOwner, null)
		)
			return vanilla.ResolveFireTree(target, visible, weaponReady, fireRate);

		return super.ResolveFireTree(target, visible, weaponReady, fireRate);
	}
}

modded class SCR_AILookAction
{
	override void LookAt(vector pos, float priority, float duration = 0.8)
	{
		if (KKCRX_LookOnHouse())
		{
			if (KK_GarrisonHold.IsPathLookWrite())
				vanilla.LookAt(pos, priority, duration);

			return;
		}

		if (KKCRX_SprintIgnoring())
			return;

		// Attack engage already wrote its point. CRX would replace it
		// with its own enemy look and drop the skill offset.
		if (KKCRX_EngageLookHeld())
			return;

		// The lane look is already on him. A look at the enemy, applied
		// and then replaced, is the turn off the sprint and back.
		if (KKCRX_LookDashing())
			return;

		// An enemy look is 80, and an unidentified man is 50. Either one
		// turns the bound into a walk. The look along the route is lower,
		// and the step does not start without it.
		if (KKCRX_LookOnRoute())
		{
			if (priority >= 50 || KKCRX_LookAtContact(pos))
			{
				KKCRX_ReleaseRoute();
				KKCRX_RestoreRoute();
				return;
			}

			vanilla.LookAt(pos, priority, duration);
			KKCRX_RestoreRoute();
			return;
		}

		super.LookAt(pos, priority, duration);
		KKCRX_RelockHold();
	}

	override void LookAt(IEntity ent, float priority, float duration = 0.8)
	{
		if (KKCRX_LookOnHouse())
			return;

		// Facing the enemy turns the sprint into a strafe. A look along
		// the route still has to run, or the step never starts.
		if (KKCRX_SprintIgnoring() || KKCRX_LookDashing())
			return;

		if (KKCRX_EngageLookHeld())
			return;

		if (KKCRX_LookOnRoute())
		{
			KKCRX_ReleaseRoute();
			KKCRX_RestoreRoute();
			return;
		}

		super.LookAt(ent, priority, duration);
		KKCRX_RelockHold();
	}

	// The house order owns the head. A path snap writes the fields itself.
	protected bool KKCRX_LookOnHouse()
	{
		if (!m_Utility)
			return false;

		return KKCRX_HouseOwns(m_Utility.m_OwnerEntity) ||
			KKCRX_HouseOwns(m_Utility.GetOwner());
	}

	// Drops a look that is already on a man. The lane snap outranks him,
	// and cancelling that snap is what lets the head swing back.
	void KKCRX_ReleaseRoute()
	{
		if (m_fPriority >= SCR_AILookAction.PRIO_COMMANDER)
			return;

		if (m_vPosition == vector.Zero || !KKCRX_LookAtContact(m_vPosition))
			return;

		Cancel();
	}

	// The behavior tree finishes a look on its own timer and CRX then
	// aims the head at the enemy. While he is sprinting, that finish
	// is what swings him off the lane and back.
	override void Complete()
	{
		if (KKCRX_LookOnHouse())
		{
			vanilla.Complete();
			return;
		}

		if (KKCRX_LookDashing() && m_fPriority >= SCR_AILookAction.PRIO_COMMANDER)
			return;

		// Finishing this look is what lets CRX aim the head at its own point.
		if (KKCRX_EngageLookHeld())
			return;

		super.Complete();
	}

	override void MoveLookParametersToNode(
		out bool outCanLook,
		out vector outLookPos,
		out float outLookDuration,
		out bool outCancelLook,
		out bool outRestartLook)
	{
		if (KKCRX_LookOnHouse())
		{
			vanilla.MoveLookParametersToNode(
				outCanLook,
				outLookPos,
				outLookDuration,
				outCancelLook,
				outRestartLook
			);
			return;
		}

		// Attack engage already wrote its point, skill offset included.
		// CRX would replace it with its own enemy look.
		if (KKCRX_EngageLookHeld())
		{
			vanilla.MoveLookParametersToNode(
				outCanLook,
				outLookPos,
				outLookDuration,
				outCancelLook,
				outRestartLook
			);
			return;
		}

		// CRX's own look runs in this call and points him at the enemy.
		// The next pass points him down the lane. That pair of writes is
		// the head thrash. While he is sprinting, this node does not
		// enter CRX, and it does not restart the turn.
		if (KKCRX_LookDashing())
		{
			vector lane;
			IEntity body;
			if (m_Utility)
				body = m_Utility.m_OwnerEntity;

			bool haveLane = KK_GarrisonHold.GetSprintLook(body, lane);
			if (!haveLane && m_Utility)
				haveLane = KK_GarrisonHold.GetSprintLook(m_Utility.GetOwner(), lane);

			m_bCancelLook = false;
			m_bRestartLook = false;

			if (!haveLane)
			{
				outCanLook = false;
				outCancelLook = false;
				outRestartLook = false;
				outLookPos = vector.Zero;
				outLookDuration = 0;
				return;
			}

			// Once, to take the head off the enemy. After that the point
			// moves with him and the turn is not started again.
			bool restart = !m_bKKLaneHeld;
			m_bKKLaneHeld = true;

			outCanLook = true;
			outCancelLook = false;
			outRestartLook = restart;
			outLookPos = lane;
			outLookDuration = 8;
			m_vPosition = lane;
			m_fPriority = SCR_AILookAction.PRIO_COMMANDER;
			m_fDuration = 8;
			return;
		}

		m_bKKLaneHeld = false;
		super.MoveLookParametersToNode(
			outCanLook,
			outLookPos,
			outLookDuration,
			outCancelLook,
			outRestartLook
		);
	}

	protected bool m_bKKLaneHeld;

	protected void KKCRX_RestoreRoute()
	{
		if (!m_Utility)
			return;

		IEntity body = m_Utility.m_OwnerEntity;
		if (!body)
			body = m_Utility.GetOwner();

		KKCRX_KeepBoundSprint(body);
		KKCRX_KeepRecall(body);
	}

	// A man on his spot may look, so he can shoot. The look must not
	// leave him walking.
	protected void KKCRX_RelockHold()
	{
		if (!m_Utility)
			return;

		IEntity body = m_Utility.m_OwnerEntity;
		if (!body)
			body = m_Utility.GetOwner();

		KKCRX_HoldPinned(body);
	}

	protected bool KKCRX_LookOnRoute()
	{
		if (!m_Utility)
			return false;

		return KKCRX_OnRoute(m_Utility.m_OwnerEntity) ||
			KKCRX_OnRoute(m_Utility.GetOwner());
	}

	// Attack engage is aiming. A sprint keeps the lane instead.
	protected bool KKCRX_EngageLookHeld()
	{
		if (!m_Utility || KKCRX_LookDashing() || KKCRX_SprintIgnoring())
			return false;

		return KK_GarrisonHold.PerceivedFireActive(m_Utility.m_OwnerEntity) ||
			KK_GarrisonHold.PerceivedFireActive(m_Utility.GetOwner());
	}

	protected bool KKCRX_LookDashing()
	{
		if (!m_Utility)
			return false;

		return KKCRX_IsDashing(m_Utility.m_OwnerEntity) ||
			KKCRX_IsDashing(m_Utility.GetOwner());
	}

	protected bool KKCRX_LookAtContact(vector pos)
	{
		if (!m_Utility)
			return false;

		if (m_Utility.m_CombatComponent)
		{
			BaseTarget current = m_Utility.m_CombatComponent.GetCurrentTarget();
			if (KKCRX_CloseToTarget(pos, current))
				return true;
		}

		IEntity body = m_Utility.m_OwnerEntity;
		if (!body)
		{
			AIAgent agent = AIAgent.Cast(m_Utility.GetOwner());
			if (agent)
				body = agent.GetControlledEntity();
		}

		if (!body)
			return false;

		PerceptionComponent perception = PerceptionComponent.Cast(
			body.FindComponent(PerceptionComponent)
		);
		if (!perception)
			return false;

		array<BaseTarget> seen = {};
		for (int category = 0; category < 3; category++)
		{
			ETargetCategory kind = ETargetCategory.ENEMY;
			if (category == 1)
				kind = ETargetCategory.DETECTED;
			else if (category == 2)
				kind = ETargetCategory.UNKNOWN;

			seen.Clear();
			perception.GetTargetsList(seen, kind);
			foreach (BaseTarget candidate : seen)
			{
				if (KKCRX_CloseToTarget(pos, candidate))
					return true;
			}
		}

		return false;
	}

	protected bool KKCRX_CloseToTarget(vector pos, BaseTarget target)
	{
		if (!target)
			return false;

		if (KKCRX_CloseToPoint(pos, target.GetLastSeenPosition()))
			return true;

		IEntity body = target.GetTargetEntity();
		if (!body)
			return false;

		return KKCRX_CloseToPoint(pos, body.GetOrigin());
	}

	protected bool KKCRX_CloseToPoint(vector pos, vector at)
	{
		vector flat = pos - at;
		flat[1] = 0;
		return flat.Length() <= 6;
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

// Outermost, so CRX cannot accept a combat move during the bound.
// That request is the walk toward the enemy. The sprint is the order.
modded class SCR_AICombatMoveState
{
	override void ApplyNewRequest(notnull SCR_AICombatMoveRequestBase request)
	{
		if (KK_GarrisonHold.IsSprintMoveLocked(this) || KK_GarrisonHold.IsClearMoveLocked(this))
		{
			request.m_eState = SCR_EAICombatMoveRequestState.CANCELED;
			if (m_Request && m_Request.m_eState == SCR_EAICombatMoveRequestState.EXECUTING)
				m_Request.m_eState = SCR_EAICombatMoveRequestState.CANCELED;

			m_Request = null;
			m_bAimAtTarget = false;
			return;
		}

		super.ApplyNewRequest(request);
	}

	override void EnableAiming(bool enable)
	{
		if (
			enable &&
			(
				KK_GarrisonHold.IsSprintMoveLocked(this) ||
				KK_GarrisonHold.IsClearMoveLocked(this)
			)
		)
		{
			m_bAimAtTarget = false;
			return;
		}

		super.EnableAiming(enable);
	}
}
