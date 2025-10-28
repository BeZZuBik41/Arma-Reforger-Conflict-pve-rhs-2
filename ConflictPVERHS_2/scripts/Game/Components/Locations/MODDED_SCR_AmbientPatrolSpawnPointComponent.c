modded class SCR_AmbientPatrolSpawnPointComponent : ScriptComponent
{
    // Custom variables configurable in world editor
    // Number of respawn waves for a counter attack group:
    // setting of 2 will spawn one group with 2 more waves
    // once the original group is eliminated for a total of 3 groups
    [Attribute("0", UIWidgets.EditBox, "How many waves will the group respawn. (0 = no respawn, -1 = infinite respawn)", "-1 inf 1")]
    protected int m_iRespawnWaves;
    
    // Delay between spawning each group in milliseconds
    [Attribute("2500", UIWidgets.EditBox, "Delay between spawning each group in milliseconds", "1000 10000 1000")]
    protected float m_fSpawnDelayMs;
    
    override int GetRespawnWaves()
    {
        return m_iRespawnWaves;
    }
    
    override void SetRespawnWaves(int waves)
    {
        m_iRespawnWaves = waves;
    }
    
    // To designate a vehicle group:
    // Must be a child of an ambient vehicle spawnpoint 
    [Attribute("0", desc: "Check this box if this is a vehicle group.  Make sure this is a child of an ambient vehicle spawnpoint.")]
    protected bool m_bIsVehicleGroup;
    
    override bool IsVehicleGroup()    // For vehicle patrols
    {
        return m_bIsVehicleGroup;
    }
    
    // To designate an artillery group:
    // Must add a child AIWaypoint_ArtillerySupport on the target
    [Attribute("0", desc: "Check this box if this is an artillery group.  Make sure you add a target AIWaypoint_ArtillerySupport as a child.")]
    protected bool m_bIsArtilleryGroup;
    
    override bool IsArtilleryGroup()    // For artillery patrols
    {
        return m_bIsArtilleryGroup;
    }
    
    // To designate a counter attack group:
    // Must be a child of a CampaingMilitaryBase
    // with an origin outside the base
    // and have a child AIWaypoint_Defend_CP on the destination
    [Attribute("0", desc: "Check this box if this is a respawning counter attack group. Respawn waves must be 1 or more and 'Respawn Period' must be greater than 0. Make sure this is a child of a conflict military base.")]
    protected bool m_bIsCounterGroup;
    
    override bool IsCounterGroup()    // For counter attack groups
    {
        return m_bIsCounterGroup;
    }
    
	// To set up a custom group:
	// Add 20 characters to a custom group
	// Using variant characters is recommended
	// Create new group types in enum SCR_EGroupType
	[Attribute("1", UIWidgets.EditBox, "Multiply the group size (4) by this number. (1 = 4 members, 2 = 8 members, 3 = 12 members, etc. Max = 5 or 20 members)", "1 5 1")]
    protected int m_iGroupMultiplier;
	
	override int GetGroupMultiplier()
	{
		return m_iGroupMultiplier;
	}
	
	
	[Attribute("0", UIWidgets.EditBox, "group size, if set to 0, will use m_iGroupMultiplier, max 21, by number of units in ConflictPVERHS_2\Prefabs\Groups\BLUFOR\RHS_USAF\RHS_USAF_USMC_MEF\AmbientPatrols", "0 21 0")]
	protected int m_iUnitCount;
	
	int GetUnitCount()
	{
		return m_iUnitCount;
	}
    
    // For player distance checks
    protected ref array<IEntity> m_aPlayers = {};
    
    // To get artillery entity from the spawnpoint
    protected IEntity m_eRelatedArty;
    
    override IEntity GetRelatedArty()
    {
        return m_eRelatedArty;
    }
    
    override void SetRelatedArty(IEntity arty)
    {
        m_eRelatedArty = arty;
    }
    
    // Used for storing the original waypoint origin for its artillery target
    protected vector origWptOrigin;
    
    override vector GetOriginalWaypointOrigin()
    {
        return origWptOrigin;
    }
    
    override void SetOriginalWaypointOrigin(vector pos)
    {
        origWptOrigin = pos;
    }
    
    // For detecting if the linear mod has been initialized
    protected bool m_bIsPostInit = false;
    
    override bool GetIsPostInit()
    {
        return m_bIsPostInit;
    }
    
    // For determining if counter attacks have started
    protected bool m_bCountersTriggered = false;
    
    override bool GetCountersTriggered()
    {
        return m_bCountersTriggered;
    }
    
    override void SetCountersTriggered(bool triggered)
    {
        m_bCountersTriggered = triggered;
    }
    
    // For remembering number of members still alive when respawning stuck counters
    protected int m_iNumMembersAlive = -1;
    
    override int GetNumMembersAlive()
    {
        return m_iNumMembersAlive;
    }
    
    override void SetNumMembersAlive(int num)
    {
        m_iNumMembersAlive = num;
    }
        
    //------------------------------------------------------------------------------------------------
    // Function for refreshing the list of players (m_aPlayers)
    protected override void RefreshPlayerList()
    {
        m_aPlayers.Clear();
        array<int> playerIds = {};
        PlayerManager pc = GetGame().GetPlayerManager();
        int playersCount = pc.GetPlayers(playerIds);
        foreach (int playerId : playerIds)
        {
            IEntity player = pc.GetPlayerControlledEntity(playerId);
            if (!player)
                continue;
            CharacterControllerComponent comp = CharacterControllerComponent.Cast(player.FindComponent(CharacterControllerComponent));
            if (!comp || comp.IsDead())
                continue;
            m_aPlayers.Insert(player);
        }
    }
    
    // Helper function to spawn a single group with specified parameters
    protected void SpawnSingleGroup(int unitsToSpawn, int groupIndex, int totalGroups)
    {
        if (m_iMembersAlive > 0)
        {
            m_Group.SetMaxUnitsToSpawn(m_iMembersAlive);
        }
        else if (unitsToSpawn > 0)
        {
            m_Group.SetMaxUnitsToSpawn(unitsToSpawn);
        }
        
        // Check for counter group
        if (IsCounterGroup())
        {
            if (GetNumMembersAlive() != -1)
            {
                m_Group.SetMaxUnitsToSpawn(GetNumMembersAlive());
                SetNumMembersAlive(-1);
            }
            m_Group.SpawnUnits();
            GetGame().GetCallqueue().CallLater(SetGroupMaxLOD, 15000, false, m_Group);
        }
        // Check for artillery group
        else if (IsArtilleryGroup())
        {
            m_Group.SpawnUnits();
            GetGame().GetCallqueue().CallLater(SetGroupMaxLOD, 15000, false, m_Group);
        }
        // Else, vanilla
        else
        {
            Print("[CPR] Spawning group " + (groupIndex + m_Group.GetAgentsCount()).ToString() + " of " + totalGroups.ToString());
            m_Group.SpawnUnits();
        }
    }
    
    // Override vanilla function
    override void SpawnPatrol()
    {
        // Vanilla code
        SCR_FactionAffiliationComponent comp = SCR_FactionAffiliationComponent.Cast(GetOwner().FindComponent(SCR_FactionAffiliationComponent));
        
        if (!comp)
            return;
        
        SCR_Faction faction = SCR_Faction.Cast(comp.GetAffiliatedFaction());
        
        if (!faction)
            faction = SCR_Faction.Cast(comp.GetDefaultAffiliatedFaction());
        
        if (faction != m_SavedFaction || m_iRespawnPeriod > 0)
            Update(faction);
        
        if (m_sPrefab.IsEmpty())
            return;
        
        Resource prefab = Resource.Load(m_sPrefab);
        
        if (!prefab || !prefab.IsValid())
            return;
        
        m_bSpawned = true;
        m_bActive = true;
        
        EntitySpawnParams params = EntitySpawnParams();
        params.TransformMode = ETransformMode.WORLD;
        params.Transform[3] = GetOwner().GetOrigin();
        
        // Gramps added - Counter attack randomizer
        if (IsCounterGroup())
        {
            RefreshPlayerList();
            
            // Get a random spawn point for counter attack
            float dist = vector.Distance(GetOwner().GetOrigin(), m_Waypoint.GetOrigin());
            vector waypointVec = m_Waypoint.GetOrigin();
            vector randomDir = Vector(Math.RandomFloat(0, 360), 0, 0).AnglesToVector();
            vector randomVec = waypointVec + dist * randomDir;
            bool spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(randomVec, randomVec, 40, 4, 4, TraceFlags.ENTS | TraceFlags.WORLD, GetGame().GetWorld());
            randomVec[1] = this.GetOwner().GetWorld().GetSurfaceY(randomVec[0], randomVec[2]);
            
            // Define if any player is too close to spawn counter attack group
            bool playersVeryNear;
            int distance;
            foreach (IEntity player : m_aPlayers)
            {
                if (!player)
                    continue;
                distance = vector.DistanceSq(player.GetOrigin(), randomVec);
                if (distance > 45000)
                    continue;
                playersVeryNear = true;
                break;
            }
            
            // Define if selected random spawn point is in the water
            BaseWorld world = this.GetOwner().GetWorld();
            vector outWaterSurfacePoint;
            EWaterSurfaceType outType;
            vector transformWS[4];
            vector obbExtents;
            bool isUnderwater = ChimeraWorldUtils.TryGetWaterSurface(world, randomVec, outWaterSurfacePoint, outType, transformWS, obbExtents);
            
            // If random spawn point is in the water or players are too near, pick a new point
            int limiter = 0;    // Fix for infinite loops
            while (isUnderwater && limiter < 30 || 
                playersVeryNear && limiter < 30 || 
                !spawnEmpty && limiter < 30 || 
                IsTooSteep(randomVec) && limiter < 30)
            {
                randomDir = Vector(Math.RandomFloat(0, 360), 0, 0).AnglesToVector();
                randomVec = waypointVec + dist * randomDir;
                spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(randomVec, randomVec, 40, 4, 4, TraceFlags.ENTS | TraceFlags.WORLD, GetGame().GetWorld());
                randomVec[1] = this.GetOwner().GetWorld().GetSurfaceY(randomVec[0], randomVec[2]);
                isUnderwater = ChimeraWorldUtils.TryGetWaterSurface(world, randomVec, outWaterSurfacePoint, outType, transformWS, obbExtents);
                RefreshPlayerList();
                playersVeryNear = false;
                foreach (IEntity player : m_aPlayers)
                {
                    if (!player)
                        continue;
                    distance = vector.DistanceSq(player.GetOrigin(), randomVec);
                    if (distance > 45000)
                        continue;
                    playersVeryNear = true;
                    break;
                }
                limiter++;
            }
            if(limiter >= 30)
            {
                Print("[CPR] Randomizer limit reached, counter attack skipped!");
                m_bSpawned = false;
                m_bActive = false;
                return;
            }
            // Once all checks are passed, set random spawn point
            params.Transform[3] = randomVec;
        }
        
        // Gramps added - Artillery spawnpoint randomizer
        if (m_Waypoint && IsArtilleryGroup())
        {
            RefreshPlayerList();
            
            // Get a random spawn point for artillery
            float dist = vector.Distance(GetOwner().GetOrigin(), m_Waypoint.GetOrigin());
            vector waypointVec = m_Waypoint.GetOrigin();
            vector randomDir = Vector(Math.RandomFloat(0, 360), 0, 0).AnglesToVector();
            vector randomVec = waypointVec + dist * randomDir;
            randomVec[1] = this.GetOwner().GetWorld().GetSurfaceY(randomVec[0], randomVec[2]);
            bool spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(randomVec, randomVec, 40, 4, 4, TraceFlags.ENTS | TraceFlags.WORLD, GetGame().GetWorld());
            
            // Define if any player is too close to spawn artillery group
            bool playersVeryNear;
            int distance;
            foreach (IEntity player : m_aPlayers)
            {
                if (!player)
                    continue;
                distance = vector.DistanceSq(player.GetOrigin(), randomVec);
                if (distance > 45000)
                    continue;
                playersVeryNear = true;
                break;
            }
            
            // Define nearest base distance
            SCR_GameModeCampaign m_Campaign = SCR_GameModeCampaign.Cast(GetGame().GetGameMode());
            SCR_CampaignMilitaryBaseManager baseMan = m_Campaign.GetBaseManager();
            SCR_CampaignMilitaryBaseComponent closestBase = baseMan.FindClosestBase(randomVec);
            float distBase = vector.Distance(closestBase.GetOwner().GetOrigin(), randomVec);
            
            // Define nearest road distance
            SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
            RoadNetworkManager rNMan = aiWorld.GetRoadNetworkManager();
            BaseRoad road;
            float distRoad;
            rNMan.GetClosestRoad(randomVec, road, distRoad);
            
            // Define if selected random spawn point is in the water
            BaseWorld world = this.GetOwner().GetWorld();
            vector outWaterSurfacePoint;
            EWaterSurfaceType outType;
            vector transformWS[4];
            vector obbExtents;
            bool isUnderwater = ChimeraWorldUtils.TryGetWaterSurface(world, randomVec, outWaterSurfacePoint, outType, transformWS, obbExtents);
            
            // Define if arty group can hit/reach the target waypoint
            bool canHitTarget = false;
            float aimAngleMinRad = 0.785398;
            float aimAngleMaxRad = 1.48353;
            float distToTgtHoriz = vector.DistanceXZ(randomVec, waypointVec);
            float distToTgtVert = waypointVec[1] - randomVec[1];
            float aimAngleTanMin = Math.Tan(aimAngleMinRad);
            float aimAngleTanMax = Math.Tan(aimAngleMaxRad);
            ResourceName ammoPrefab = "{98EC9C526AFBA282}Prefabs/Weapons/Ammo/Ammo_Shell_82mm_HE_O832DU.et";
            Resource ammoResource = Resource.Load(ammoPrefab);
            BaseResourceObject ammoResourceObj = ammoResource.GetResource();
            IEntitySource ammoEntitySrc = ammoResourceObj.ToEntitySource();
            array<float> initSpeedCoefficients = {};
            bool ammoSupportsSpeedConfigurations = GetAmmoInitialSpeedCoefficients(ammoPrefab, initSpeedCoefficients);
            if (initSpeedCoefficients.IsEmpty())
                initSpeedCoefficients.Insert(1.0);
            float aimOffsetVert = 0;
            int initSpeedId = -1;
            if (distToTgtHoriz > 0)
            {
                for (int i = 0; i < initSpeedCoefficients.Count(); i++)
                {
                    float initialSpeedCoefficient = initSpeedCoefficients[i];
                    float flightTime;
                    bool ballisticsGood = BallisticTable.GetAimHeightOfProjectileAltitudeFromSource(distToTgtHoriz, aimOffsetVert, flightTime, ammoEntitySrc, distToTgtVert, initialSpeedCoefficient);
                    if (!ballisticsGood)
                    {
                        aimOffsetVert = BallisticTable.GetHeightFromProjectileSource(distToTgtHoriz, flightTime, ammoEntitySrc, initSpeedCoef: initialSpeedCoefficient, bDirectFire: false);
                        ballisticsGood = flightTime > 0;
                    }
                    if (!ballisticsGood)
                        continue;
                    float aimAngleTan = (aimOffsetVert + distToTgtVert) / distToTgtHoriz;
                    if (aimAngleTan > aimAngleTanMax || aimAngleTan < aimAngleTanMin)
                        continue;
                    canHitTarget = true;
                    initSpeedId = i;
                    break;
                }
            }
            
            // If random spawn point is in the water,
            // players are too near, terrain is too steep,
            // or can not hit target, pick a new point
            int limiter = 0;    // Fix for infinite loops
            while (isUnderwater && limiter < 30 ||
                    playersVeryNear && limiter < 30 ||
                    !spawnEmpty && limiter < 30 ||
                    IsTooSteep(randomVec) && limiter < 30 ||
                    !canHitTarget && limiter < 30 ||
                    distBase < 300 && limiter < 30 ||
                    SurfaceIsRoad(randomVec) && limiter < 30)
            {
                // New random position
                randomDir = Vector(Math.RandomFloat(0, 360), 0, 0).AnglesToVector();
                randomVec = waypointVec + dist * randomDir;
                spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(randomVec, randomVec, 40, 4, 4, TraceFlags.ENTS | TraceFlags.WORLD, GetGame().GetWorld());
                // Water check
                randomVec[1] = this.GetOwner().GetWorld().GetSurfaceY(randomVec[0], randomVec[2]);
                isUnderwater = ChimeraWorldUtils.TryGetWaterSurface(world, randomVec, outWaterSurfacePoint, outType, transformWS, obbExtents);
                // Nearby player check
                RefreshPlayerList();
                playersVeryNear = false;
                foreach (IEntity player : m_aPlayers)
                {
                    if (!player)
                        continue;
                    distance = vector.DistanceSq(player.GetOrigin(), randomVec);
                    if (distance > 45000)
                        continue;
                    playersVeryNear = true;
                    break;
                }
                // Can hit target waypoint check
                distToTgtHoriz = vector.DistanceXZ(randomVec, waypointVec);
                distToTgtVert = waypointVec[1] - randomVec[1];
                if (distToTgtHoriz > 0)
                {
                    for (int i = 0; i < initSpeedCoefficients.Count(); i++)
                    {
                        float initialSpeedCoefficient = initSpeedCoefficients[i];
                        float flightTime;
                        bool ballisticsGood = BallisticTable.GetAimHeightOfProjectileAltitudeFromSource(distToTgtHoriz, aimOffsetVert, flightTime, ammoEntitySrc, distToTgtVert, initialSpeedCoefficient);
                        if (!ballisticsGood)
                        {
                            aimOffsetVert = BallisticTable.GetHeightFromProjectileSource(distToTgtHoriz, flightTime, ammoEntitySrc, initSpeedCoef: initialSpeedCoefficient, bDirectFire: false);
                            ballisticsGood = flightTime > 0;
                        }
                        if (!ballisticsGood)
                            continue;
                        float aimAngleTan = (aimOffsetVert + distToTgtVert) / distToTgtHoriz;
                        if (aimAngleTan > aimAngleTanMax || aimAngleTan < aimAngleTanMin)
                            continue;
                        canHitTarget = true;
                        initSpeedId = i;
                        break;
                    }
                }
                limiter++;
            }
            // If limiter is reached,
            // discontinue search until next round
            if(limiter >= 30)
            {
                Print("[CPR] Randomizer limit reached, artillery group skipped!");
                m_bSpawned = false;
                m_bActive = false;
                return;
            }
            // Once all checks are passed, set random spawn point
            params.Transform[3] = randomVec;
        }
        
        // Vanilla code
        if (m_iRespawnPeriod == 0 && m_Waypoint && Math.RandomFloat01() >= 0.5)
        {
            AIWaypointCycle cycleWP = AIWaypointCycle.Cast(m_Waypoint);
            
            if (cycleWP)
            {
                array<AIWaypoint> waypoints = {};
                cycleWP.GetWaypoints(waypoints);
                params.Transform[3] = GetOwner().GetOrigin();
            }
        }
        
        m_Group = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(prefab, null, params));
        
        if (!m_Group)
            return;
        
        if (!m_Group.GetSpawnImmediately())
        {
            int groupMultiplier = GetGroupMultiplier();
            int unitsToSpawn = 0;
            
            // Если это Vehicle Group, определяем количество мест в транспорте
            if (IsVehicleGroup())
            {
                IEntity aiParent = GetOwner().GetParent();
                if (aiParent)
                {
                    SCR_AmbientVehicleSpawnPointComponent AVSPC = SCR_AmbientVehicleSpawnPointComponent.Cast(aiParent.FindComponent(SCR_AmbientVehicleSpawnPointComponent));
                    if (AVSPC)
                    {
                        Vehicle spawnedVehicle = AVSPC.GetSpawnedVehicle();
                        if (spawnedVehicle)
                        {
                            BaseCompartmentManagerComponent slotCompMan = BaseCompartmentManagerComponent.Cast(spawnedVehicle.FindComponent(BaseCompartmentManagerComponent));
                            if (slotCompMan)
                            {
                                array<BaseCompartmentSlot> vehicleCompartments = new array<BaseCompartmentSlot>;
                                slotCompMan.GetCompartments(vehicleCompartments);
                                unitsToSpawn = vehicleCompartments.Count();
                            }
                        }
                    }
                }
            }
            else
            {
                // Если GetUnitCount() не равен 0, используем его значение
                if (GetUnitCount() != 0)
                {
                    unitsToSpawn = GetUnitCount();
                }
            }
            
            // Создаем группы с задержкой
            for (int i = 0; i < groupMultiplier; i++)
            {
                GetGame().GetCallqueue().CallLater(SpawnSingleGroup, m_fSpawnDelayMs * i, false, unitsToSpawn, i, groupMultiplier);
            }
        }
        
        m_Group.AddWaypoint(m_Waypoint);
        m_Group.SetAmbientPatrol(this);    // Save spawnpoint to group
        if ((m_iRespawnPeriod != 0 && m_iRespawnWaves > -1))
            m_Group.GetOnAgentRemoved().Insert(OnAgentRemoved);
    }
    
    override void OnAgentRemoved()
    {
        if (!m_Group || m_Group.GetAgentsCount() > 0)
            return;
        
        SCR_CampaignMilitaryBaseComponent base;
        if (GetOwner().GetParent() && IsCounterGroup() && m_iRespawnWaves == 0)
        {
            base = SCR_CampaignMilitaryBaseComponent.Cast(GetOwner().GetParent().FindComponent(SCR_CampaignMilitaryBaseComponent));
            if(base)
            {
                array<SCR_AmbientPatrolSpawnPointComponent> counters = base.GetCounterSpawnpoints();
                if (counters && counters.Contains(this))
                {
                    base.RemoveCounterSpawnpoint(this);
                }
            }
        }

        ChimeraWorld world = GetOwner().GetWorld();
        if (m_fRespawnTimestamp.GreaterEqual(world.GetServerTimestamp()))
            return;
        
        // Set up respawn timestamp, convert s to ms, reset original group size
        m_fRespawnTimestamp = world.GetServerTimestamp().PlusSeconds(m_iRespawnPeriod);
        if(m_iRespawnWaves > -1)
            m_iRespawnWaves--;
        if(m_iRespawnWaves == 0)
            m_iRespawnWaves = -2;
        m_iMembersAlive = -1;
        m_bSpawned = false;
    }
    
    // Prevent max LOD for both group and its members
    override void SetGroupMaxLOD(SCR_AIGroup group)
    {
        if(group)
        {
            array<AIAgent> agents = {};
            group.GetAgents(agents);
            
            group.PreventMaxLOD();
            
            foreach (AIAgent agent : agents)
            {
                agent.PreventMaxLOD();
            }
        }
    }
    
    // Check for if random point is on a road
    override bool SurfaceIsRoad(vector pos)
    {
        pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
        TraceParam params = new TraceParam();
        params.Flags = TraceFlags.WORLD;
        params.Start = pos + 0.01 * vector.Up;
        params.End = pos - 0.01 * vector.Up;
        GetGame().GetWorld().TraceMove(params, null);
        ResourceName material = params.TraceMaterial;
        bool isOnRoad = false;
        if(material.IndexOf("Road_") >= 0)        isOnRoad = true;
        return isOnRoad;
    }
    
    // Calculate if selected random spawn point is on too steep a hill
    override bool IsTooSteep(vector pos)
    {
        vector pos1, pos2, pos3, pos4;
        pos1 = pos;
        pos2 = pos;
        pos3 = pos;
        pos4 = pos;
        pos1[0] = pos[0] + 4;
        pos2[0] = pos[0] + 4;
        pos3[0] = pos[0] - 4;
        pos4[0] = pos[0] - 4;
        pos1[2] = pos[2] + 4;
        pos2[2] = pos[2] - 4;
        pos3[2] = pos[2] + 4;
        pos4[2] = pos[2] - 4;
        pos1[1] = GetGame().GetWorld().GetSurfaceY(pos1[0], pos1[2]);
        pos2[1] = GetGame().GetWorld().GetSurfaceY(pos2[0], pos2[2]);
        pos3[1] = GetGame().GetWorld().GetSurfaceY(pos3[0], pos3[2]);
        pos4[1] = GetGame().GetWorld().GetSurfaceY(pos4[0], pos4[2]);
        float diff1 = pos1[1]-pos2[1];
        float diff2 = pos1[1]-pos3[1];
        float diff3 = pos1[1]-pos4[1];
        float diff4 = pos2[1]-pos4[1];
        bool isTooSteep = diff1 > 2 || diff1 < -2 || diff2 > 2 || diff2 < -2 || diff3 > 2 || diff3 < -2 || diff4 > 2 || diff4 < -2;
        return isTooSteep;
    }
    
    // Returns true if ammo supports initial speed configurations
    // Used for calculating if artillery can hit its target
    override bool GetAmmoInitialSpeedCoefficients(ResourceName ammoResourceName, notnull array<float> outCoefficients)
    {
        outCoefficients.Clear();
        
        int defaultConfigId;
        array<float> initSpeedCoefficients = SCR_MortarShellGadgetComponent.GetPrefabInitSpeedCoef(ammoResourceName, defaultConfigId);
        
        if (initSpeedCoefficients && !initSpeedCoefficients.IsEmpty())
        {
            outCoefficients.Copy(initSpeedCoefficients);
            return true;
        }
        
        return false;
    }
};