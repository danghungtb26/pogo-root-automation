public interface IDynamicTappable // TypeDefIndex: 4166
{
	// Properties
	public abstract Vector3 Position { get; }
	public abstract int Id { get; }
	public abstract ulong CellId { get; }
	public abstract ulong EncounterId { get; }
	[Nullable(0)]
	public abstract string TypeKey { get; }
	public abstract LatLng Location { get; }
	public abstract TappableLocation TappableLocation { get; }
	public abstract bool VisibleOnMap { get; set; }
	public abstract long DespawnTimeMs { get; }
	public abstract bool IsEncounter { set; }

	// Methods

	// RVA: -1 Offset: -1 Slot: 0
	public abstract Vector3 get_Position();

	// RVA: -1 Offset: -1 Slot: 1
	public abstract int get_Id();

	// RVA: -1 Offset: -1 Slot: 2
	public abstract ulong get_CellId();

	// RVA: -1 Offset: -1 Slot: 3
	public abstract ulong get_EncounterId();

	[NullableContext(0)]
	// RVA: -1 Offset: -1 Slot: 4
	public abstract string get_TypeKey();

	// RVA: -1 Offset: -1 Slot: 5
	public abstract LatLng get_Location();

	// RVA: -1 Offset: -1 Slot: 6
	public abstract TappableLocation get_TappableLocation();

	// RVA: -1 Offset: -1 Slot: 7
	public abstract bool get_VisibleOnMap();

	// RVA: -1 Offset: -1 Slot: 8
	public abstract void set_VisibleOnMap(bool value);

	// RVA: -1 Offset: -1 Slot: 9
	public abstract long get_DespawnTimeMs();

	// RVA: -1 Offset: -1 Slot: 10
	public abstract void set_IsEncounter(bool value);

	// RVA: -1 Offset: -1 Slot: 11
	public abstract void AnimateInteraction(Action onAnimationEnd);
}

// Namespace: Niantic.Holoholo.Services.DynamicTappables
public interface IDynamicTappableInternal : IDynamicTappable // TypeDefIndex: 4167
{
	// Methods

	// RVA: -1 Offset: -1 Slot: 0
	public abstract void Refresh(DynamicTappableController.Data data);

	// RVA: -1 Offset: -1 Slot: 1
	public abstract bool Tapped();

	// RVA: -1 Offset: -1 Slot: 2
	public abstract void Remove(bool removeImmediately);
}

// Namespace: Niantic.Holoholo.Services.DynamicTappables
public class DynamicTappablesServiceConfig // TypeDefIndex: 4168
{
	// Fields
	public string TypeKey; // 0x10
	public List<TappableProperties> Tappables; // 0x18

	// Methods

	// RVA: 0x8461F48 Offset: 0x845DF48 VA: 0x8461F48
	public void .ctor() { }
}

// Namespace: Niantic.Holoholo.Services.DynamicTappables
public interface IDynamicTappablesService : IService // TypeDefIndex: 4169
{
	// Properties
	public abstract bool IsSessionActive { get; }
	public abstract bool TappablesEnabled { get; }
	[Nullable(new[] { 2, 0 })]
	public abstract Action<IDynamicTappable, bool> TappableTapped { get; set; }
	[Nullable(new[] { 2, 0 })]
	public abstract Action<IDynamicTappable> TappableAdded { get; set; }
	[Nullable(new[] { 2, 0 })]
	public abstract Action<IDynamicTappable> TappableRemoved { get; set; }

	// Methods

	// RVA: -1 Offset: -1 Slot: 0
	public abstract IDynamicTappablesSession StartSession(DynamicTappablesServiceConfig config);

	[NullableContext(2)]
	// RVA: -1 Offset: -1 Slot: 1
	public abstract IDynamicTappablesSession GetSession(Tappable.Types.TappableType type);

	// RVA: -1 Offset: -1 Slot: 2
	public abstract IDynamicTappablesSession GetSession(string typeKey);

	// RVA: -1 Offset: -1 Slot: 3
	public abstract void EndSession(Tappable.Types.TappableType type);

	// RVA: -1 Offset: -1 Slot: 4
	public abstract void EndSession(string typeKey);

	// RVA: -1 Offset: -1 Slot: 5
	public abstract bool get_IsSessionActive();

	// RVA: -1 Offset: -1 Slot: 6
	public abstract bool get_TappablesEnabled();

	// RVA: -1 Offset: -1 Slot: 7
	public abstract bool GetTappables(List<IDynamicTappable> list);

	// RVA: -1 Offset: -1 Slot: 8
	public abstract void TapTappable(IDynamicTappable tappable);

	// RVA: -1 Offset: -1 Slot: 9
	public abstract Action<IDynamicTappable, bool> get_TappableTapped();

	// RVA: -1 Offset: -1 Slot: 10
	public abstract void set_TappableTapped(Action<IDynamicTappable, bool> value);

	// RVA: -1 Offset: -1 Slot: 11
	public abstract Action<IDynamicTappable> get_TappableAdded();

	// RVA: -1 Offset: -1 Slot: 12
	public abstract void set_TappableAdded(Action<IDynamicTappable> value);

	// RVA: -1 Offset: -1 Slot: 13
	public abstract Action<IDynamicTappable> get_TappableRemoved();

	// RVA: -1 Offset: -1 Slot: 14
	public abstract void set_TappableRemoved(Action<IDynamicTappable> value);

	// RVA: -1 Offset: -1 Slot: 15
	public abstract LatLng CreateSpawnLocationAroundPoint(LatLng center, float radiusM);
}

// Namespace: Niantic.Holoholo.Services.DynamicTappables
public interface IDynamicTappablesSession // TypeDefIndex: 4170
{
	// Properties
	[Nullable(new[] { 2, 0 })]
	public abstract Action<IDynamicTappable, bool> TappableTapped { get; set; }
	[Nullable(new[] { 2, 0 })]
	public abstract Action<IDynamicTappable> TappableAdded { get; set; }
	[Nullable(new[] { 2, 0 })]
	public abstract Action<IDynamicTappable> TappableRemoved { get; set; }
	public abstract bool IsSessionActive { get; }

	// Methods

	// RVA: -1 Offset: -1 Slot: 0
	public abstract void EndSession();

	// RVA: -1 Offset: -1 Slot: 1
	public abstract IEnumerable<IDynamicTappable> GetTappables();

	// RVA: -1 Offset: -1 Slot: 2
	public abstract IDynamicTappable GetTappable(ulong id);

	// RVA: -1 Offset: -1 Slot: 3
	public abstract void AddTappables(List<TappableProperties> tappables);

	// RVA: -1 Offset: -1 Slot: 4
	public abstract void AddTappable(TappableProperties tappable);

	// RVA: -1 Offset: -1 Slot: 5
	public abstract Action<IDynamicTappable, bool> get_TappableTapped();

	// RVA: -1 Offset: -1 Slot: 6
	public abstract void set_TappableTapped(Action<IDynamicTappable, bool> value);

	// RVA: -1 Offset: -1 Slot: 7
	public abstract Action<IDynamicTappable> get_TappableAdded();

	// RVA: -1 Offset: -1 Slot: 8
	public abstract void set_TappableAdded(Action<IDynamicTappable> value);

	// RVA: -1 Offset: -1 Slot: 9
	public abstract Action<IDynamicTappable> get_TappableRemoved();

	// RVA: -1 Offset: -1 Slot: 10
	public abstract void set_TappableRemoved(Action<IDynamicTappable> value);

	// RVA: -1 Offset: -1 Slot: 11
	public abstract bool get_IsSessionActive();
}

// Namespace: Niantic.Holoholo.Services.DynamicTappables
public interface IDynamicTappableBehaviorService // TypeDefIndex: 4171
{
	// Methods

	// RVA: -1 Offset: -1 Slot: 0
	public abstract void ProcessTappableTapped(IDynamicTappable tappable, bool isDone);
}

// Namespace: Niantic.Holoholo.Services.DynamicTappables
public struct TappableEncounterDiscovered : IPubSubMessage // TypeDefIndex: 4172
{}

// Namespace: Niantic.Holoholo.Services.DynamicTappables
public class TappableProperties // TypeDefIndex: 4173
{
	// Fields
	public int Id; // 0x10
	public ulong CellId; // 0x18
	public ulong EncounterId; // 0x20
	public string TypeKey; // 0x28
	public LatLng Location; // 0x30
	[Nullable(2)]
	public TappableLocation TappableLocation; // 0x40
	public bool PreviouslyTapped; // 0x48
	public long ExpirationTimeMs; // 0x50

	// Methods

	// RVA: 0x8461F6C Offset: 0x845DF6C VA: 0x8461F6C
	public void .ctor() { }
}

// Namespace: 
[CompilerGenerated]
[Serializable]
private sealed class DynamicTappablesService.<>c // TypeDefIndex: 4174
{
	// Fields
	public static readonly DynamicTappablesService.<>c <>9; // 0x0
	public static Func<DynamicTappablesSession, bool> <>9__27_0; // 0x8

	// Methods

	// RVA: 0x8463DC4 Offset: 0x845FDC4 VA: 0x8463DC4
	private static void .cctor() { }

	// RVA: 0x8463E14 Offset: 0x845FE14 VA: 0x8463E14
	public void .ctor() { }

	// RVA: 0x8463E18 Offset: 0x845FE18 VA: 0x8463E18
	internal bool bfhb(DynamicTappablesSession a) { }
}

// Namespace: 
[CompilerGenerated]
private sealed class DynamicTappablesService.cas : IEnumerator<ISchedule>, IEnumerator, IDisposable // TypeDefIndex: 4175
{
	// Fields
	private int dbzg; // 0x10
	private ISchedule dbzh; // 0x18
	public DynamicTappablesService <>4__this; // 0x20

	// Properties
	private ISchedule enwp { get; }
	private object enwq { get; }

	// Methods

	[DebuggerHidden]
	// RVA: 0x84638CC Offset: 0x845F8CC VA: 0x84638CC
	public void .ctor(int a) { }

	[DebuggerHidden]
	// RVA: 0x8463E2C Offset: 0x845FE2C VA: 0x8463E2C Slot: 5
	private void bfhc() { }

	// RVA: 0x8463E30 Offset: 0x845FE30 VA: 0x8463E30 Slot: 6
	private bool MoveNext() { }

	[DebuggerHidden]
	// RVA: 0x84642EC Offset: 0x84602EC VA: 0x84642EC Slot: 4
	private ISchedule bfhd() { }

	[DebuggerHidden]
	// RVA: 0x84642F4 Offset: 0x84602F4 VA: 0x84642F4 Slot: 8
	private void bfhe() { }

	[DebuggerHidden]
	// RVA: 0x846432C Offset: 0x846032C VA: 0x846432C Slot: 7
	private object bfhf() { }
}

// Namespace: Niantic.Holoholo.Services.DynamicTappables
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

// Namespace: 
