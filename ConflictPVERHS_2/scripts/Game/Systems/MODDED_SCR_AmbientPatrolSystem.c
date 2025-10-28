modded class SCR_AmbientPatrolSystem : GameSystem
{
	// If loaded, the linear conflict pve mod will change this to true
	protected bool m_bIsLinearLoaded = false;
	
	override bool GetIsLinearLoaded()
	{
		return m_bIsLinearLoaded;
	}
	
	// Detect if linear mod is loaded
	override event protected void OnInit()
	{
		super.OnInit();
		Print("[CPR] Linear addon loaded? "+m_bIsLinearLoaded.ToString());
	}
	
	// Process AmbientPatrolSpawnPoint
	override protected void ProcessSpawnpoint(int spawnpointIndex)
	{
		// Vanilla code - check for null spawnpoint or empty group
		SCR_AmbientPatrolSpawnPointComponent spawnpoint = m_aPatrols[spawnpointIndex];
		
		if (!spawnpoint || (spawnpoint.GetMembersAlive() == 0 && !spawnpoint.GetIsSpawned()))
			return;

		// Check respawn timer and stop processing if its not time to respawn the group
		ChimeraWorld world = GetWorld();
		WorldTimestamp currentTime = world.GetServerTimestamp();
		if (spawnpoint.GetRespawnTimestamp().Greater(currentTime))
			return;
		
		// Variables for player distance calculation
		bool playersNear;
		bool playersVeryNear;
		bool playersFar = true;
		vector location = spawnpoint.GetOwner().GetOrigin();
		int distance;

		// Set distance variables
		// Define if any player is close enough to spawn or if all players are far enough to despawn
		foreach (IEntity player : m_aPlayers)
		{
			if (!player)
				continue;

			distance = vector.DistanceSq(player.GetOrigin(), location);

			if (distance > m_iDespawnDistanceSq)
				continue;

			playersFar = false;

			if (distance > m_iSpawnDistanceSq)
				continue;

			playersNear = true;

			if (distance > SPAWN_RADIUS_BLOCK_SQ)
				continue;

			playersVeryNear = true;
			break;
		}
		
		// Check for ai limits
		bool isAIOverLimit;
		AIWorld aiWorld = GetGame().GetAIWorld();

		if (aiWorld)
		{
			int maxChars = aiWorld.GetLimitOfActiveAIs();

			if (maxChars <= 0)
				isAIOverLimit = true;
			else
				isAIOverLimit = ((float)aiWorld.GetCurrentNumOfActiveAIs() / (float)maxChars) > spawnpoint.GetAILimitThreshold();
		}
		
		if (!isAIOverLimit && !playersVeryNear)
			spawnpoint.SetIsPaused(false);

		if (playersNear && !spawnpoint.GetIsPaused() && !spawnpoint.IsGroupActive())
		{
			// Do not spawn the patrol if the AI threshold setting has been reached
			if (isAIOverLimit)
			{
				spawnpoint.SetIsPaused(true);	// Make sure a patrol is not spawned too close to players when AI limit suddenly allows spawning of this group
				return;
			}

			spawnpoint.ActivateGroup();
//			return;
		}
		// End vanilla code
		
		// If is vehicle spawn
		// Get the vehicle spawnpoint from its parent in the world
		if(spawnpoint.IsVehicleGroup())
		{
			IEntity aiParent = spawnpoint.GetOwner().GetParent();
			if(aiParent)
			{
				SCR_AmbientVehicleSpawnPointComponent AVSPC = SCR_AmbientVehicleSpawnPointComponent.Cast(aiParent.FindComponent(SCR_AmbientVehicleSpawnPointComponent));
				if(AVSPC && !AVSPC.GetRelatedSpawnpoint())
					AVSPC.SetRelatedSpawnpoint(spawnpoint);
			}
		}
		
		// Custom code - If artillery group and spawned in already, detect if they can get in the turret
		if(spawnpoint.IsArtilleryGroup() && spawnpoint.GetIsSpawned())
		{
			if(spawnpoint.GetSpawnedGroup())
			{
				SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
				if(group)
				{
					IEntity artillery = spawnpoint.GetRelatedArty();
					if (!artillery)
						return;
					
					TurretControllerComponent TCComp = TurretControllerComponent.Cast(artillery.FindComponent(TurretControllerComponent));
					if(!TCComp)
						return;
					
					BaseCompartmentSlot compartment = TCComp.GetCompartmentSlot();
					if (!compartment)
						return;
					
					IEntity owner = compartment.GetOwner();
					SCR_BaseCompartmentManagerComponent m_CompartmentManager = SCR_BaseCompartmentManagerComponent.Cast(artillery.FindComponent(SCR_BaseCompartmentManagerComponent));
					if (!m_CompartmentManager)
						return;
					
					AIAgent agent = group.GetLeaderAgent();
					if (!agent)
						return;
					
					SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
					if (!character)
						return;
					
					CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
					if (!compartmentAccess)
						return;
					
					// if not, place them in it
					if (!compartmentAccess.CanGetInVehicleViaDoor(owner, m_CompartmentManager, 0))
					{
						array<Managed> compartmentManagers = {};
						FindComponentsInAllChildren(SCR_AIStaticArtilleryVehicleUsageComponent, artillery, false, 0, 19, compartmentManagers);
						SCR_AIStaticArtilleryVehicleUsageComponent baseComp = SCR_AIStaticArtilleryVehicleUsageComponent.Cast(compartmentManagers[0]);
						BaseCompartmentSlot slot = baseComp.GetTurretCompartmentSlot();
						IEntity compOwner = baseComp.GetOwner();
						if(!agent.IsAIActivated())
							agent.ActivateAI();
						agent.PreventMaxLOD();
						
						if(compartmentAccess && !character.IsInVehicle())
						{
							if(slot)
								compartmentAccess.GetInVehicle(compOwner, slot, true, -1, ECloseDoorAfterActions.INVALID, false);
						}
						return;
					}
				}
			}
			
			// Then check for being paused or deactivated and force LOD settings
			if(spawnpoint.GetIsPaused() || !spawnpoint.IsGroupActive())
			{
				spawnpoint.SetIsPaused(false);
				spawnpoint.ActivateGroup();
			}
			SetGroupLOD(spawnpoint);
		}
		
		SCR_CampaignMilitaryBaseComponent base;
		
		// If Counter group, check to see if stuck and if so, respawn them.
		if(spawnpoint.IsCounterGroup() && spawnpoint.GetIsSpawned())
		{
			SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
			if(group)
			{
				// Only check if the group is outside the completion radius
				AIWaypoint wpt = AIWaypoint.Cast(spawnpoint.GetWaypoint());
				vector lastPos = group.GetLastPosition();
				if(vector.Distance(group.GetOrigin(), wpt.GetOrigin()) > wpt.GetCompletionRadius())
				{
//					Print("Distance from squad to waypoint: "+vector.Distance(group.GetOrigin(), wpt.GetOrigin()));
					SCR_AIGroupUtilityComponent utilComp = group.GetGroupUtilityComponent();
					if (!utilComp || !utilComp.m_Perception)
				        return;
				    bool groupInCombat = true;
				    if (utilComp.m_Perception.m_aTargets.Count() == 0)
				    {
						groupInCombat = false;
				    }
//					Print("[CPR] Squad "+spawnpointIndex+" in combat? "+groupInCombat.ToString());
					if (group.GetOrigin() == lastPos && !groupInCombat)
					{
						base = SCR_CampaignMilitaryBaseComponent.Cast(spawnpoint.GetOwner().GetParent().FindComponent(SCR_CampaignMilitaryBaseComponent));
						if(base)
						{
							Print("[CPR] Squad for "+base.GetBaseName()+" stuck!  Remaining waves of group "+spawnpointIndex+": "+spawnpoint.GetRespawnWaves());
							spawnpoint.SetRespawnWaves(spawnpoint.GetRespawnWaves()/* + 1*/);
						}
						else
						{
							Print("[CPR] Squad for supply depot stuck!  Respawning...");
						}
						spawnpoint.DeactivateGroup();
						spawnpoint.DespawnPatrol();
						spawnpoint.SetIsSpawned(false);
						spawnpoint.SetNumMembersAlive(spawnpoint.GetMembersAlive());
						return;
					}
					group.SetLastPosition(group.GetOrigin());
				}
			}
		}
		
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		
		// If artillery group and counters are eliminated, spawn artillery emplacement
		if (spawnpoint.GetOwner().GetParent() && !spawnpoint.IsVehicleGroup() && spawnpoint.GetIsPostInit() && !spawnpoint.GetIsSpawned())
		{
			base = SCR_CampaignMilitaryBaseComponent.Cast(spawnpoint.GetOwner().GetParent().FindComponent(SCR_CampaignMilitaryBaseComponent));
			if(base)
			{

				if(spawnpoint.IsArtilleryGroup() && baseManager.CountersAreEliminated(base))
				{
					spawnpoint.SpawnPatrol();
					spawnpoint.SetIsPaused(false);
					spawnpoint.ActivateGroup();
					GetGame().GetCallqueue().CallLater(SetGroupLOD,12000,false,spawnpoint);
					GetGame().GetCallqueue().CallLater(SpawnArtilleryPlacement,10000,false,spawnpoint);
				}
			}
		}

		// If vehicle group and is not spawned in yet,
		// spawn and TP to parent vehicle from vehicle
		// spawnpoint and adjust group size to fill vehicle
		if(spawnpoint.IsVehicleGroup() && m_bIsLinearLoaded || 
			spawnpoint.IsVehicleGroup() && !m_bIsLinearLoaded && playersNear && !playersVeryNear)
		{
			if(!spawnpoint.GetIsSpawned())
			{
				spawnpoint.SpawnPatrol();
				spawnpoint.SetIsPaused(false);
				spawnpoint.ActivateGroup();
				GetGame().GetCallqueue().CallLater(SetGroupLOD,12000,false,spawnpoint);
				GetGame().GetCallqueue().CallLater(TeleportToVehicle,10000,false,spawnpoint);
				return;
			}
		}
		
		// If vehicle group is spawned in already,
		// check if AI is still owner of vehicle,
		// if not, change fuel consumption to normal.
		if(spawnpoint.IsVehicleGroup() && spawnpoint.GetIsSpawned())
		{
			CheckVehicleFuelConsumption(spawnpoint);
		}

		// Check for counter attack groups and spawning them,
		// skipping player proximity check if linear addon is loaded
		if (!spawnpoint.GetIsSpawned() && !playersVeryNear && !spawnpoint.IsArtilleryGroup())
		{
			if(spawnpoint.IsCounterGroup() && m_bIsLinearLoaded || 
				spawnpoint.IsCounterGroup() && !m_bIsLinearLoaded && playersNear || 
				spawnpoint.IsCounterGroup() && !m_bIsLinearLoaded && spawnpoint.GetCountersTriggered())
			{
				if(spawnpoint.GetRespawnWaves() == -1 && !playersNear)
					return;
				spawnpoint.SpawnPatrol();
				spawnpoint.SetIsPaused(false);
				spawnpoint.ActivateGroup();
				if(!spawnpoint.GetCountersTriggered())
					spawnpoint.SetCountersTriggered(true);
				GetGame().GetCallqueue().CallLater(SetGroupLOD,12000,false,spawnpoint);
			}
			// Spawn other ai groups and set/force LOD
			else if(playersNear && !spawnpoint.IsCounterGroup())
			{
				spawnpoint.SpawnPatrol();
				spawnpoint.SetIsPaused(false);
				spawnpoint.ActivateGroup();
				GetGame().GetCallqueue().CallLater(SetGroupLOD,12000,false,spawnpoint);
			}
		}

		// Determine if this spawnpoint is a child of a base
		if(spawnpoint.GetOwner().GetParent())
			base = SCR_CampaignMilitaryBaseComponent.Cast(spawnpoint.GetOwner().GetParent().FindComponent(SCR_CampaignMilitaryBaseComponent));
	
		// Gramps added this check to prevent ai capturing
		// a point from despawning when players leave the area
		// of a point under attack
		bool capturing = false;
		if (spawnpoint.GetIsSpawned() && spawnpoint.IsGroupActive() && spawnpoint.GetOwner().GetParent())
		{
			SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
			if(group)
			{
				if (spawnpoint.GetRespawnWaves() > 0)
					capturing = true;
				if (base && !baseManager.CountersAreEliminated(base))
					capturing = true;
			}
		}
		// End Gramps edit
		
		// Vanilla code
		// Delay is used so dying players don't see the despawn happen
		// If the spawnpoint is a child of a base,
		// is spawned in and players are not very close,
		// is not a vehicle or artillery group,
		// and has respawn waves > -1
		// (not infinite like for supply depot counters/reinforcements);
		// then do not despawn group
		if (base && spawnpoint.GetIsSpawned() && !playersVeryNear && !spawnpoint.IsVehicleGroup() && !spawnpoint.IsArtilleryGroup() && spawnpoint.GetRespawnWaves() > -1)
		{
			spawnpoint.SetDespawnTimer(null);
		}
		// If group is spawned in and players are far,
		// group is active and not a vehicle or artillery group,
		// and is not capturing a base;
		// then deactivate and despawn the group
		else if (spawnpoint.GetIsSpawned() && playersFar && spawnpoint.IsGroupActive() /*Gramps added>>*/&& !spawnpoint.IsVehicleGroup()&& !spawnpoint.IsArtilleryGroup() && !capturing)
		{
			WorldTimestamp despawnT = spawnpoint.GetDespawnTimer();

			if (despawnT == 0)
				spawnpoint.SetDespawnTimer(currentTime.PlusMilliseconds(DESPAWN_TIMEOUT));
			else if (currentTime.Greater(despawnT))
			{
				spawnpoint.DeactivateGroup();
				// Added by gramps to emulate old spawn system
				// that despawned groups when players are far
				spawnpoint.DespawnPatrol();
			}
		}
		// Anything else, do not despawn
		else
			spawnpoint.SetDespawnTimer(null);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Composes an array of surviving ambient patrols which can be serialized
	override int GetRemainingPatrolsInfo(out notnull array<int> remnantsInfo)
	{
		int size;
		ChimeraWorld world = GetWorld();
		WorldTimestamp curTime = world.GetServerTimestamp();

		foreach (SCR_AmbientPatrolSpawnPointComponent presence : m_aPatrols)
		{
			if (!presence)
				continue;

			SCR_AIGroup grp = presence.GetSpawnedGroup();

			if (presence.GetMembersAlive() < 0 && !grp && !presence.GetIsSpawned())
				continue;

			remnantsInfo.Insert(presence.GetID());

			if (grp)
			{
				size = remnantsInfo.Insert(grp.GetAgentsCount());
			}
			else
			{
				if (presence.GetIsSpawned())
					size = remnantsInfo.Insert(0);
				else
					size = remnantsInfo.Insert(presence.GetMembersAlive());
			}

			// Subtract current time so when data is loaded at scenario start, the delay is correct
			remnantsInfo.Insert(presence.GetRespawnTimestamp().DiffMilliseconds(curTime));

			/* >> Gramps added code for saving new variables */
			remnantsInfo.Insert(presence.GetRespawnWaves());
			/* << Gramps added */
		}

		return size;
	}
	
	// Sets group and individual agent LODs of a spawnpoint group
	override void SetGroupLOD(SCR_AmbientPatrolSpawnPointComponent spawnpoint)
	{
		SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
		if(group)
		{
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			
			group.PreventMaxLOD();
			group.SetPermanentLOD(-1);
			group.SetLOD(9);
			
			// Do the same for each individual agent
			foreach (AIAgent agent : agents)
			{
				agent.PreventMaxLOD();
				agent.SetPermanentLOD(-1);
				agent.SetLOD(9);
			}
		}
	}

	/* Gramps added - Check for resetting vehicle fuel consumption when its AI group dies	>> */
	override void CheckVehicleFuelConsumption(SCR_AmbientPatrolSpawnPointComponent spawnpoint)
	{
		SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
		if(!group)
		{
			// Vehicle group is dead, reset fuel consumption back to vanilla default
			// Get vehicle spawnpoint from its parent in the world
			IEntity aiParent = spawnpoint.GetOwner().GetParent();
			SCR_AmbientVehicleSpawnPointComponent AVSPC;
			if(aiParent)
				AVSPC = SCR_AmbientVehicleSpawnPointComponent.Cast(aiParent.FindComponent(SCR_AmbientVehicleSpawnPointComponent));
			Vehicle spawnedVehicle;
			if(AVSPC)
			{
				// Get vehicle from spawnpoint
				spawnedVehicle = AVSPC.GetSpawnedVehicle();
				if(spawnedVehicle)
				{
					SCR_FuelConsumptionComponent fuelComp = SCR_FuelConsumptionComponent.Cast(spawnedVehicle.FindComponent(SCR_FuelConsumptionComponent));
					if(fuelComp)
					{
						// Get fuel consumption component and reset value for player use
						SCR_FuelConsumptionComponentClass m_ComponentData = SCR_FuelConsumptionComponentClass.Cast(fuelComp.GetComponentData(spawnedVehicle));
						m_ComponentData.m_fFuelConsumption = 8;
					}
				}
			}
		}
	}

	/* Gramps added - TP to parent vehicle from vehicle spawnpoint >> */		
	override void TeleportToVehicle(SCR_AmbientPatrolSpawnPointComponent spawnpoint)
	{
		// Get group and its members
		SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
		if(group)
		{
			group.SetIsVehiclePatrol(true);
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			if(agents)
			{
				// Get the vehicle spawnpoint from its parent in the world
				IEntity aiParent = spawnpoint.GetOwner().GetParent();
				SCR_AmbientVehicleSpawnPointComponent AVSPC;
				if(aiParent)
					AVSPC = SCR_AmbientVehicleSpawnPointComponent.Cast(aiParent.FindComponent(SCR_AmbientVehicleSpawnPointComponent));
				Vehicle spawnedVehicle;
				if(AVSPC)
				{
					// Get the vehicle
					spawnedVehicle = AVSPC.GetSpawnedVehicle();
					if(spawnedVehicle)
					{
						SCR_FuelConsumptionComponent fuelComp = SCR_FuelConsumptionComponent.Cast(spawnedVehicle.FindComponent(SCR_FuelConsumptionComponent));
						if(fuelComp)
						{
							// Get fuel consumption component and set to none
							SCR_FuelConsumptionComponentClass m_ComponentData = SCR_FuelConsumptionComponentClass.Cast(fuelComp.GetComponentData(spawnedVehicle));
							m_ComponentData.m_fFuelConsumption = 0;
							// Set speed limit for AI
							AICarMovementComponent ACMC = AICarMovementComponent.Cast(spawnedVehicle.FindComponent(AICarMovementComponent));
							ACMC.SetCruiseSpeed(27);
						}
						// Add vehicle to AI groups array of vehicles
						array<string> groupVehicles = {};
						string vehicle = spawnedVehicle.ToString();
						groupVehicles.Insert(vehicle);
						group.SetGroupVehicles(groupVehicles);
						
						// Get vehicle compartments
						BaseCompartmentManagerComponent slotCompMan = BaseCompartmentManagerComponent.Cast(spawnedVehicle.FindComponent(BaseCompartmentManagerComponent));
						array<BaseCompartmentSlot> vehicleCompartments = new array<BaseCompartmentSlot>;
						slotCompMan.GetCompartments(vehicleCompartments);
						// Loop through all group members
						for(int j = 0; j < agents.Count(); j++)
						{
							AIAgent member = agents[j];
							if(member)
							{
								member.PreventMaxLOD();
								SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(member.GetControlledEntity());
								CompartmentAccessComponent CAComp = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
								// If not in vehicle, put in an open seat
								if(CAComp && character && !character.IsInVehicle() && j < vehicleCompartments.Count())
								{
									BaseCompartmentSlot slot = vehicleCompartments[j];
									if(slot)
										CAComp.GetInVehicle(spawnedVehicle, slot, true, -1, ECloseDoorAfterActions.INVALID, false);
								}
								// If no more seats open, remove the member from the group and delete
								else
								{
									group.RemoveAgent(member);
									RplComponent.DeleteRplEntity(member.GetControlledEntity().GetRootParent(), false);
								}
							}
						}
					}
				}
			}
		}
	}
	
	// Span artillery emplacement for artillery group spawnpoint
	override void SpawnArtilleryPlacement(SCR_AmbientPatrolSpawnPointComponent spawnpoint)
	{
		// Get group and its members
		SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
		if(group)
		{
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			if(agents)
			{
				// Get mortar prefab and set variables for spawning it in with a target waypoint
				string rsrcName = "{28D63631873AA636}Prefabs/Compositions/Slotted/SlotFlatSmall/MortarPlacement_S_USSR_01.et";
				Resource rsrc = Resource.Load(rsrcName);
				IEntity artillery;
				AIWaypoint wpt = spawnpoint.GetWaypoint();
				vector waypointVec = wpt.GetOrigin();
				spawnpoint.SetOriginalWaypointOrigin(waypointVec);
				vector randomDir = Vector(Math.RandomFloat(0, 360), 0, 0).AnglesToVector();
				float randFloat = Math.RandomFloat(1.0, wpt.GetCompletionRadius());
				vector randomVec = waypointVec + randFloat * randomDir;
				wpt.SetOrigin(randomVec);
				wpt.Update();
				
				if (rsrc.IsValid())
				{
					//---------------------------------------------------------------------------//
					//---------------------------------------------------------------------------//
					//---------------------------------------------------------------------------//
					//---   Thank you Bacon for showing me the way on these next few lines!   ---//
					//---------------------------------------------------------------------------//
					//---------------------------------------------------------------------------//
					//---------------------------------------------------------------------------//
					EntitySpawnParams spawnParams = new EntitySpawnParams();
					spawnParams.TransformMode = ETransformMode.WORLD;
					Math3D.MatrixIdentity4(spawnParams.Transform);
	    				spawnParams.Transform[3] = group.GetOrigin();
					vector targetPositionWorld = wpt.GetOrigin();
					
					// Point it toward the target and spawn it in
					SCR_Math3D.LookAt(spawnParams.Transform[3], targetPositionWorld, vector.Up, spawnParams.Transform);	
					artillery = GetGame().SpawnEntityPrefab(rsrc, GetGame().GetWorld(), spawnParams);
					
					spawnpoint.SetRelatedArty(artillery);
					
					// Snap emplacement to terrain
					SCR_EditableEntityComponent editableEntity = SCR_EditableEntityComponent.Cast(artillery.FindComponent(SCR_EditableEntityComponent));
					vector transform[4];
			
					if (!editableEntity)
					{
						artillery.GetTransform(transform);
						SCR_TerrainHelper.SnapToTerrain(transform, artillery.GetWorld());
						artillery.SetTransform(transform);
					}
					else
					{
						editableEntity.GetTransform(transform);
						if (!SCR_TerrainHelper.SnapToTerrain(transform, artillery.GetWorld()))
							return;
						editableEntity.SetTransformWithChildren(transform);
					}
					
					// Place an AI in the turret to operate the mortar
					array<Managed> compartmentManagers = {};
					FindComponentsInAllChildren(SCR_AIStaticArtilleryVehicleUsageComponent, artillery, false, 0, 19, compartmentManagers);
					SCR_AIStaticArtilleryVehicleUsageComponent baseComp = SCR_AIStaticArtilleryVehicleUsageComponent.Cast(compartmentManagers[0]);
					BaseCompartmentSlot slot = baseComp.GetTurretCompartmentSlot();
					IEntity compOwner = baseComp.GetOwner();
					for(int j = 0; j < agents.Count(); j++)
					{
						AIAgent member = agents[j];
						if(member)
						{
							SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(member.GetControlledEntity());
							CompartmentAccessComponent CAComp = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
							if(!member.IsAIActivated())
								member.ActivateAI();
							member.PreventMaxLOD();
							
							if(CAComp && character && !character.IsInVehicle() && j < 1)
							{
								if(slot)
									CAComp.GetInVehicle(compOwner, slot, true, -1, ECloseDoorAfterActions.INVALID, false);
							}
						}
					}
					
					// Get waypoint and set infinite shot count
					SCR_AIWaypointArtillerySupport asWpt = SCR_AIWaypointArtillerySupport.Cast(wpt);
					asWpt.SetTargetShotCount(-1, true);
				}
			}
		}
	}
}
