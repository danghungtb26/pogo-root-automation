public class DynamicTappablesService : MonoBehaviour, IDynamicTappablesService, IService // TypeDefIndex: 4176
{
	// Fields
	[Inject]
	private readonly DiContainer container; // 0x20
	[Inject]
	private readonly IScheduler dbzi; // 0x28
	[Inject]
	private readonly IWorldMapRenderer dbzj; // 0x30
	[Inject]
	private readonly IGameMasterData dbzk; // 0x38
	[Inject]
	private readonly IRandomService dbzl; // 0x40
	private const float dbzm = 0,5;
	private const float dbzn = 1,5;
	private readonly Dictionary<string, DynamicTappablesSession> dbzo; // 0x48
	[Nullable(2)]
	private ISchedulerPromise dbzp; // 0x50
	[Nullable(new[] { 2, 0 })]
	[CompilerGenerated]
	private Action<IDynamicTappable, bool> <TappableTapped>k__BackingField; // 0x58
	[Nullable(new[] { 2, 0 })]
	[CompilerGenerated]
	private Action<IDynamicTappable> <TappableRemoved>k__BackingField; // 0x60
	[Nullable(new[] { 2, 0 })]
	[CompilerGenerated]
	private Action<IDynamicTappable> <TappableAdded>k__BackingField; // 0x68
	[Nullable(2)]
	private DynamicTappablesSession.CallbackConfig dbzq; // 0x70

	// Properties
	[Nullable(new[] { 2, 0 })]
	public Action<IDynamicTappable, bool> TappableTapped { get; set; }
	[Nullable(new[] { 2, 0 })]
	public Action<IDynamicTappable> TappableRemoved { get; set; }
	[Nullable(new[] { 2, 0 })]
	public Action<IDynamicTappable> TappableAdded { get; set; }
	private DynamicTappablesSession.CallbackConfig enwr { get; }
	public bool TappablesEnabled { get; }
	public bool IsSessionActive { get; }

	// Methods

	[CompilerGenerated]
	// RVA: 0x84623D0 Offset: 0x845E3D0 VA: 0x84623D0 Slot: 13
	public Action<IDynamicTappable, bool> get_TappableTapped() { }

	[CompilerGenerated]
	// RVA: 0x84623D8 Offset: 0x845E3D8 VA: 0x84623D8 Slot: 14
	public void set_TappableTapped(Action<IDynamicTappable, bool> value) { }

	[CompilerGenerated]
	// RVA: 0x84623E0 Offset: 0x845E3E0 VA: 0x84623E0 Slot: 17
	public Action<IDynamicTappable> get_TappableRemoved() { }

	[CompilerGenerated]
	// RVA: 0x84623E8 Offset: 0x845E3E8 VA: 0x84623E8 Slot: 18
	public void set_TappableRemoved(Action<IDynamicTappable> value) { }

	[CompilerGenerated]
	// RVA: 0x84623F0 Offset: 0x845E3F0 VA: 0x84623F0 Slot: 15
	public Action<IDynamicTappable> get_TappableAdded() { }

	[CompilerGenerated]
	// RVA: 0x84623F8 Offset: 0x845E3F8 VA: 0x84623F8 Slot: 16
	public void set_TappableAdded(Action<IDynamicTappable> value) { }

	// RVA: 0x8462400 Offset: 0x845E400 VA: 0x8462400
	private DynamicTappablesSession.CallbackConfig bfhg() { }

	// RVA: 0x8462568 Offset: 0x845E568 VA: 0x8462568 Slot: 10
	public bool get_TappablesEnabled() { }

	// RVA: 0x846258C Offset: 0x845E58C VA: 0x846258C Slot: 9
	public bool get_IsSessionActive() { }

	// RVA: 0x84626F0 Offset: 0x845E6F0 VA: 0x84626F0 Slot: 4
	public IDynamicTappablesSession StartSession(DynamicTappablesServiceConfig config) { }

	[NullableContext(2)]
	// RVA: 0x8463040 Offset: 0x845F040 VA: 0x8463040 Slot: 5
	public IDynamicTappablesSession GetSession(Tappable.Types.TappableType type) { }

	// RVA: 0x8463104 Offset: 0x845F104 VA: 0x8463104 Slot: 6
	public IDynamicTappablesSession GetSession(string typeKey) { }

	// RVA: 0x8463150 Offset: 0x845F150 VA: 0x8463150 Slot: 7
	public void EndSession(Tappable.Types.TappableType type) { }

	// RVA: 0x84631C4 Offset: 0x845F1C4 VA: 0x84631C4 Slot: 8
	public void EndSession(string typeKey) { }

	// RVA: 0x84632E4 Offset: 0x845F2E4 VA: 0x84632E4 Slot: 11
	public bool GetTappables(List<IDynamicTappable> list) { }

	// RVA: 0x8463458 Offset: 0x845F458 VA: 0x8463458 Slot: 12
	public void TapTappable(IDynamicTappable tappable) { }

	[IteratorStateMachine(typeof(DynamicTappablesService.cas))]
	// RVA: 0x8462FE4 Offset: 0x845EFE4 VA: 0x8462FE4
	private IEnumerator<ISchedule> bfhh() { }

	// RVA: 0x8462908 Offset: 0x845E908 VA: 0x8462908
	private void bfhi(string a) { }

	// RVA: 0x84639E0 Offset: 0x845F9E0 VA: 0x84639E0
	private void bfhj(IDynamicTappable a, bool b) { }

	// RVA: 0x8463A00 Offset: 0x845FA00 VA: 0x8463A00
	private void bfhk(IDynamicTappable a) { }

	// RVA: 0x8463A1C Offset: 0x845FA1C VA: 0x8463A1C
	private void bfhl(IDynamicTappable a) { }

	// RVA: 0x8463A38 Offset: 0x845FA38 VA: 0x8463A38 Slot: 19
	public LatLng CreateSpawnLocationAroundPoint(LatLng center, float radiusM) { }

	// RVA: 0x8463D4C Offset: 0x845FD4C VA: 0x8463D4C
	public void .ctor() { }
}

