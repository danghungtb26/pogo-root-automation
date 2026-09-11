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

