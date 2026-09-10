public abstract class MapPokemon : MonoBehaviourScope, IMapPokemon, IScope // TypeDefIndex: 14517
{
	// Fields
	private const float CATCH_POKEMON_RPC_TIMEOUT_SEC = 5;
	[Inject]
	private readonly IMapExploreState mapExploreState; // 0x28
	[Inject]
	private readonly IMapScene mapObjectRoot; // 0x30
	[Inject]
	private readonly IRpcHandler rpcHandler; // 0x38
	[Inject]
	private readonly IMapPlaceDirectoryService mapPlaceDirectory; // 0x40
	[Inject]
	private readonly IPokemonScaleService scaleService; // 0x48
	[Inject]
	private readonly IScheduler scheduler; // 0x50
	[Inject]
	private readonly IPrefabPool prefabPool; // 0x58
	[Inject]
	private readonly IQuestService questService; // 0x60
	[Inject]
	private readonly ITelemetryService telemetryService; // 0x68
	[Inject]
	private readonly LazyInject<IArPlusDeviceService> arPlusDeviceService; // 0x70
	[Inject]
	protected readonly IGameMasterData gameMasterData; // 0x78
	[Inject]
	protected readonly IPokemonUtilityService pokemonUtilityService; // 0x80
	[Inject]
	private readonly ICameraRig cameraRig; // 0x88
	[Inject]
	private readonly IPokemonParticleService pokemonParticleService; // 0x90
	[Inject]
	private readonly IBroadcastPubSubService broadcastPubSubService; // 0x98
	[Inject]
	private readonly IAddressablesService addressablesService; // 0xA0
	[Inject]
	private readonly IMapObjectRepositioningService mapObjectRepositioningService; // 0xA8
	[Inject]
	private readonly IWorldMapRenderer mapRenderer; // 0xB0
	[Inject]
	private readonly IPreferencesStorage preferencesStorage; // 0xB8
	[Inject]
	private readonly IMapPlaceDecoratorService mapPlaceDecoratorService; // 0xC0
	[Inject]
	private readonly ICodeGateService codeGateService; // 0xC8
	[Inject]
	private readonly IMapPokemonVisualFeatureGateService mapPokemonVisualFeatureGateService; // 0xD0
	[Inject]
	private readonly IInputTrackerPluginWrapper inputTrackerPluginWrapper; // 0xD8
	[Inject]
	private readonly IXpAwardService xpAwardService; // 0xE0
	[Inject]
	private readonly IItemBag itemBag; // 0xE8
	[SerializeField]
	protected float minimumTouchRadius; // 0xF0
	[SerializeField]
	private AudioSource audioSource; // 0xF8
	[SerializeField]
	private TapGesture tapGesture; // 0x100
	[SerializeField]
	protected MapObjectPosition mapObjectPosition; // 0x108
	[SerializeField]
	private ParticleSystem spawnFxPrefab; // 0x110
	[Header("Spawn Ring")]
	[SerializeField]
	private MapPokemonRing mapPokemonRing; // 0x118
	[SerializeField]
	private float animStartTime; // 0x120
	[SerializeField]
	private PokemonMaterialLerp materialLerp; // 0x128
	[SerializeField]
	private float materialLerpDurationEnterEncounter; // 0x130
	[SerializeField]
	private float materialLerpDurationExitEncounter; // 0x134
	[SerializeField]
	private PokemonAppraisal appraisal; // 0x138
	[SerializeField]
	private float timeToTurnFullCircle; // 0x140
	[Header("Special Animation")]
	[SerializeField]
	private float chanceToBeMad; // 0x144
	[SerializeField]
	private float chanceToHate; // 0x148
	[SerializeField]
	private float minCycleTimeSec; // 0x14C
	[SerializeField]
	private float maxCycleTimeSec; // 0x150
	[Header("Flee Fx")]
	[SerializeField]
	private EncounterPokemonValues encounterPokemonValues; // 0x158
	[SerializeField]
	private float fleeFxScale; // 0x160
	[SerializeField]
	private long fleeFxDurationMs; // 0x168
	[Header("Drop Shadow")]
	[SerializeField]
	private GameObject dropShadow; // 0x170
	[Header("Strong Fx")]
	[SerializeField]
	private AssetReferenceGameObject strongFx; // 0x178
	private readonly string EVENT_THEME_SPAWN_FX_ADDRESS; // 0x180
	[Nullable(new[] { 2, 0 })]
	private PooledPrefab<ParticleSystem> pooledSpawnFx; // 0x188
	private ParticleSystem mapPokemonSpawnFx; // 0x190
	[Nullable(new[] { 2, 0 })]
	private PooledPrefab<GameObject> pooledQuestIndicator; // 0x198
	[Nullable(2)]
	[CompilerGenerated]
	private Action EncounterValidationFlee; // 0x1A0
	[Nullable(2)]
	private ISchedulerPromise despawnHandlerPromise; // 0x1A8
	[Nullable(2)]
	private ISchedulerPromise faceDirectionPromise; // 0x1B0
	[Nullable(2)]
	private ISchedulerPromise strongAnimationLoopPromise; // 0x1B8
	[Nullable(2)]
	private GameObject spawnedStrongFx; // 0x1C0
	private bool _selectionEnabled; // 0x1C8
	[Nullable(2)]
	private IMapPlaceDecoration _rootDecoration; // 0x1D0
	[Nullable(2)]
	private IMapPlaceDecoration _iconDecoration; // 0x1D8
	[SerializeField]
	private HoloCharacter holoCharacter; // 0x1E0
	private bool addedToDictionary; // 0x1E8
	private bool startedToDestroy; // 0x1E9
	[Nullable(new[] { 2, 0 })]
	private IAssetRequest<MapPokemonSpawnFxData> spawnFxAssetRequest; // 0x1F0
	private IAssetRequest<GameObject> strongFxAssetRequest; // 0x1F8
	[CompilerGenerated]
	private Nullable<long> <DespawnTime>k__BackingField; // 0x200
	[CompilerGenerated]
	private bool <Destroyed>k__BackingField; // 0x210

	// Properties
	public abstract ulong EncounterId { get; }
	public abstract string SpawnPointId { get; }
	public abstract PokemonProto Pokemon { get; }
	public abstract PokemonDisplayProto PokemonDisplay { get; }
	public abstract int PokemonId { get; }
	public abstract int Cp { get; }
	public virtual Item SourceType { get; }
	protected bool ShouldUseMapPokemonRing { get; }
	public virtual AttractedPokemonContext AttractedPokemonContext { get; }
	public virtual CharacterSize Size { get; }
	public abstract int AppraisalStar { get; }
	public LatLng Location { get; }
	public HoloCharacter HoloCharacter { get; }
	public virtual bool DestroyOnFlee { get; }
	[Obsolete("Only used for deprecated MapObjectRepositioning logic")]
	private float PokemonRadius { get; }
	public Nullable<long> DespawnTime { get; set; }
	public bool Destroyed { get; set; }
	public bool IsSleepingSnorlax { get; }

	// Methods

	// RVA: -1 Offset: -1 Slot: 29
	public abstract ulong get_EncounterId();

	// RVA: -1 Offset: -1 Slot: 30
	public abstract string get_SpawnPointId();

	// RVA: -1 Offset: -1 Slot: 31
	public abstract PokemonProto get_Pokemon();

	// RVA: -1 Offset: -1 Slot: 32
	public abstract PokemonDisplayProto get_PokemonDisplay();

	// RVA: -1 Offset: -1 Slot: 33
	public abstract int get_PokemonId();

	// RVA: -1 Offset: -1 Slot: 34
	public abstract int get_Cp();

	// RVA: 0x7F90C4C Offset: 0x7F8CC4C VA: 0x7F90C4C Slot: 35
	public virtual Item get_SourceType() { }

	// RVA: 0x7F9025C Offset: 0x7F8C25C VA: 0x7F9025C
	protected bool get_ShouldUseMapPokemonRing() { }

	// RVA: 0x7F90444 Offset: 0x7F8C444 VA: 0x7F90444
	protected void SetMapPokemonRing(MapPokemonSpawnSource spawnSource, Item lureItem = 0) { }

	// RVA: 0x7F8F2AC Offset: 0x7F8B2AC VA: 0x7F8F2AC
	protected void HideMapPokemonRing() { }

	// RVA: 0x7F90F0C Offset: 0x7F8CF0C VA: 0x7F90F0C Slot: 36
	public virtual AttractedPokemonContext get_AttractedPokemonContext() { }

	// RVA: 0x7F90F14 Offset: 0x7F8CF14 VA: 0x7F90F14 Slot: 37
	public virtual CharacterSize get_Size() { }

	// RVA: -1 Offset: -1 Slot: 38
	public abstract int get_AppraisalStar();

	// RVA: 0x7F90F4C Offset: 0x7F8CF4C VA: 0x7F90F4C Slot: 12
	public LatLng get_Location() { }

	[NullableContext(2)]
	[CompilerGenerated]
	// RVA: 0x7F90F64 Offset: 0x7F8CF64 VA: 0x7F90F64 Slot: 27
	public void add_EncounterValidationFlee(Action value) { }

	[NullableContext(2)]
	[CompilerGenerated]
	// RVA: 0x7F91000 Offset: 0x7F8D000 VA: 0x7F91000 Slot: 28
	public void remove_EncounterValidationFlee(Action value) { }

	// RVA: 0x7F9109C Offset: 0x7F8D09C VA: 0x7F9109C
	protected void OnEncounterValidationFlee() { }

	// RVA: 0x7F901E4 Offset: 0x7F8C1E4 VA: 0x7F901E4 Slot: 14
	public HoloCharacter get_HoloCharacter() { }

	// RVA: 0x7F910B8 Offset: 0x7F8D0B8 VA: 0x7F910B8 Slot: 39
	public virtual bool get_DestroyOnFlee() { }

	// RVA: 0x7F910C0 Offset: 0x7F8D0C0 VA: 0x7F910C0
	private float get_PokemonRadius() { }

	[CompilerGenerated]
	// RVA: 0x7F9130C Offset: 0x7F8D30C VA: 0x7F9130C
	public Nullable<long> get_DespawnTime() { }

	[CompilerGenerated]
	// RVA: 0x7F9131C Offset: 0x7F8D31C VA: 0x7F9131C
	private void set_DespawnTime(Nullable<long> value) { }

	[CompilerGenerated]
	// RVA: 0x7F91328 Offset: 0x7F8D328 VA: 0x7F91328 Slot: 16
	public bool get_Destroyed() { }

	[CompilerGenerated]
	// RVA: 0x7F91330 Offset: 0x7F8D330 VA: 0x7F91330
	private void set_Destroyed(bool value) { }

	// RVA: 0x7F91338 Offset: 0x7F8D338 VA: 0x7F91338 Slot: 19
	public bool get_IsSleepingSnorlax() { }

	[Inject]
	// RVA: 0x7F91340 Offset: 0x7F8D340 VA: 0x7F91340
	private void Setup() { }

	// RVA: 0x7F9146C Offset: 0x7F8D46C VA: 0x7F9146C Slot: 40
	public virtual void RunMaterialLerp(bool toTarget) { }

	// RVA: 0x7F914A0 Offset: 0x7F8D4A0 VA: 0x7F914A0
	private void OnEnable() { }

	// RVA: 0x7F91C38 Offset: 0x7F8DC38 VA: 0x7F91C38
	private void OnDisable() { }

	// RVA: -1 Offset: -1 Slot: 41
	public abstract IPromise<PokemonEncounterResponse> SendEncounterRequest();

	// RVA: 0x7F91ECC Offset: 0x7F8DECC VA: 0x7F91ECC Slot: 42
	public virtual IPromise<CatchPokemonOutProto> TryCapture(PokeballThrow throwData, ARPlusEncounterValuesProto arEncounterValues) { }

	// RVA: 0x7F92624 Offset: 0x7F8E624 VA: 0x7F92624 Slot: 43
	protected virtual void PlaySpawnFx() { }

	[IteratorStateMachine(typeof(MapPokemon.<PlaySpawnFxNextFrame>d__116))]
	// RVA: 0x7F927F8 Offset: 0x7F8E7F8 VA: 0x7F927F8
	private IEnumerator<ISchedule> PlaySpawnFxNextFrame() { }

	// RVA: 0x7F8F3C0 Offset: 0x7F8B3C0 VA: 0x7F8F3C0
	protected void SetDespawnTime(long timestamp) { }

	[IteratorStateMachine(typeof(MapPokemon.<DespawnHandler>d__118))]
	// RVA: 0x7F9285C Offset: 0x7F8E85C VA: 0x7F9285C
	private IEnumerator<ISchedule> DespawnHandler(long timestamp) { }

	// RVA: 0x7F928C8 Offset: 0x7F8E8C8 VA: 0x7F928C8
	protected void PlayFleeFx(bool shrink = True) { }

	// RVA: 0x7F8EBD0 Offset: 0x7F8ABD0 VA: 0x7F8EBD0
	protected void InitBase(LatLng location, bool checkPlayer = False) { }

	// RVA: 0x7F8F168 Offset: 0x7F8B168 VA: 0x7F8F168
	protected void TrueNorth(HoloPokemonId pokemonId) { }

	// RVA: 0x7F931F4 Offset: 0x7F8F1F4 VA: 0x7F931F4
	public IPromise FaceTowardsCoroutine(Vector3 position, float anglesPerSecond) { }

	// RVA: 0x7F934DC Offset: 0x7F8F4DC VA: 0x7F934DC
	private IEnumerator<ISchedule> FaceTowards(Vector3 position, float anglesPerSecond) { }

	[IteratorStateMachine(typeof(MapPokemon.<TurnFromTo>d__124))]
	// RVA: 0x7F93718 Offset: 0x7F8F718 VA: 0x7F93718
	private IEnumerator<ISchedule> TurnFromTo(Vector3 startDir, Vector3 endDir, float anglesPerSecond) { }

	// RVA: 0x7F937C8 Offset: 0x7F8F7C8 VA: 0x7F937C8
	private void OnTapped() { }

	// RVA: 0x7F93870 Offset: 0x7F8F870 VA: 0x7F93870
	private void OnTap(object sender, EventArgs e) { }

	// RVA: 0x7F939CC Offset: 0x7F8F9CC VA: 0x7F939CC
	public void SetSelection(bool enabled) { }

	// RVA: 0x7F939D4 Offset: 0x7F8F9D4 VA: 0x7F939D4
	protected void LogEncounterMetrics(string encounterType, PokemonProto proto) { }

	// RVA: 0x7F91624 Offset: 0x7F8D624 VA: 0x7F91624
	private void HandleAssetLoaded() { }

	// RVA: 0x7F94410 Offset: 0x7F90410 VA: 0x7F94410
	private void CreateMapIcons() { }

	// RVA: 0x7F94118 Offset: 0x7F90118 VA: 0x7F94118
	private void SpawnEventThemePokemonFx(string eventName) { }

	// RVA: 0x7F93BE8 Offset: 0x7F8FBE8 VA: 0x7F93BE8
	private void MaybeShowPokemonGlow() { }

	// RVA: 0x7F9474C Offset: 0x7F9074C VA: 0x7F9474C
	private void GmtUpdateCheckPokemonGlow(GameMasterData.DynamicGmtUpdate update) { }

	[IteratorStateMachine(typeof(MapPokemon.<SleepingSnorlaxEncounterRoutine>d__134))]
	// RVA: 0x7F93D8C Offset: 0x7F8FD8C VA: 0x7F93D8C
	private IEnumerator<ISchedule> SleepingSnorlaxEncounterRoutine() { }

	// RVA: 0x7F9481C Offset: 0x7F9081C VA: 0x7F9481C Slot: 25
	public void CheckQuestIndicator() { }

	// RVA: 0x7F93DE8 Offset: 0x7F8FDE8 VA: 0x7F93DE8
	protected void SpawnOrDestroyStrongFx() { }

	// RVA: 0x7F94CCC Offset: 0x7F90CCC VA: 0x7F94CCC Slot: 6
	protected override void OnDestroy() { }

	// RVA: 0x7F94E44 Offset: 0x7F90E44 VA: 0x7F94E44 Slot: 44
	public virtual void Destroy() { }

	// RVA: 0x7F8FD00 Offset: 0x7F8BD00 VA: 0x7F8FD00 Slot: 45
	protected virtual void HandleDestroy() { }

	// RVA: 0x7F92F80 Offset: 0x7F8EF80 VA: 0x7F92F80
	private void FaceDirection(float anglesPerSecond) { }

	// RVA: 0x7F8FDB4 Offset: 0x7F8BDB4 VA: 0x7F8FDB4
	protected void .ctor() { }

	// RVA: 0x7F94EF4 Offset: 0x7F90EF4 VA: 0x7F94EF4 Slot: 7
	private Transform Niantic.Holoholo.Map.IMapPokemon.get_transform() { }

	[CompilerGenerated]
	// RVA: 0x7F94EFC Offset: 0x7F90EFC VA: 0x7F94EFC
	private void <PlaySpawnFxNextFrame>b__116_0() { }

	[CompilerGenerated]
	// RVA: 0x7F94FB8 Offset: 0x7F90FB8 VA: 0x7F94FB8
	private void <PlayFleeFx>b__119_0(float t) { }

	[CompilerGenerated]
	// RVA: 0x7F951B8 Offset: 0x7F911B8 VA: 0x7F951B8
	private void <FaceTowardsCoroutine>b__122_0() { }

	[CompilerGenerated]
	// RVA: 0x7F951CC Offset: 0x7F911CC VA: 0x7F951CC
	private void <SpawnEventThemePokemonFx>b__131_0(MapPokemonSpawnFxData fx) { }

	[CompilerGenerated]
	// RVA: 0x7F95204 Offset: 0x7F91204 VA: 0x7F95204
	private void <CheckQuestIndicator>b__135_0() { }

	[CompilerGenerated]
	// RVA: 0x7F953B0 Offset: 0x7F913B0 VA: 0x7F953B0
	private void <SpawnOrDestroyStrongFx>b__136_0(GameObject strongFxGameObject) { }

	[CompilerGenerated]
	// RVA: 0x7F95440 Offset: 0x7F91440 VA: 0x7F95440
	private void <FaceDirection>b__140_0() { }
}

